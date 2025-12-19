#include "analyzer/analyzer.h"
#include "diagnostics/result.h"

typedef struct Analyzer {
    String source;
    Reporter* reporter;
    TypeRegistry* types;
    BindingRegistry* bindings;
    ScopeLocation scope_location;
    Expression* expressions;
    usize* function_variable_space;
    usize function_id;
    AstStorage* storage;
} Analyzer;

static Analyzer with_scope_location(Analyzer analyzer, ScopeLocation scope_location) {
    return (Analyzer){
        .source = analyzer.source,
        .reporter = analyzer.reporter,
        .types = analyzer.types,
        .bindings = analyzer.bindings,
        .scope_location = scope_location,
        .expressions = analyzer.expressions,
        .function_variable_space = analyzer.function_variable_space,
        .function_id = analyzer.function_id,
        .storage = analyzer.storage,
    };
}

static void analyze_module(Analyzer* analyzer, Module* module);
static void register_constant_def(Analyzer* analyzer, ConstantDef* constant_def);
static void type_constant_def(Analyzer* analyzer, ConstantDef* constant_def);
static void analyze_constant_def(Analyzer* analyzer, ConstantDef* constant_def);
static void analyze_function_signature(Analyzer* analyzer, Function* function);
static void analyze_pattern(Analyzer* analyzer, Pattern* pattern);
static void analyze_variable_def(Analyzer* analyzer, VariableDef* variable_def);
static void analyze_expression(Analyzer* analyzer, Expression* expression);
static void analyze_unary_operation(Analyzer* analyzer, UnaryOperation* unary_operation, TypeId* dst);
static void analyze_binary_operation(Analyzer* analyzer, BinaryOperation* binary_operation, TypeId* dst);
static void analyze_variable_ref(Analyzer* analyzer, VariableRef* variable_ref);
static void analyze_function_body(Analyzer* analyzer, Function* function);
static void resolve_type(Analyzer* analyzer, TypeName name, TypeId* dst);

bool analyze(Ast* ast, Reporter* reporter) {
    TypeBinding bool_binding = {
        .name = STRING_LITERAL("Bool"),
        .type = TYPE_BOOL,
    };
    insert_type_binding(&ast->bindings, ROOT_SCOPE_ID, bool_binding, NULL);

    Analyzer analyzer = {
        .source = ast->source,
        .reporter = reporter,
        .types = &ast->types,
        .bindings = &ast->bindings,
        .scope_location = scope_end_location(ast->bindings, ROOT_SCOPE_ID),
        .expressions = ast->expressions.data,
        .function_variable_space = NULL,
        .function_id = 0,
        .storage = &ast->storage,
    };

    analyze_module(&analyzer, &ast->root);
    return reporter_error_count(reporter) == 0;
}

static void analyze_module(Analyzer* parent, Module* module) {
    ScopeLocation global_scope = scope_new(parent->bindings, parent->scope_location);
    module->global_scope = global_scope.scope_id;
    Analyzer analyzer = with_scope_location(*parent, global_scope);
    for (usize i = 0; i < module->global_constants.len; i++) {
        ConstantDef* constant_def = &module->global_constants.data[i];
        register_constant_def(&analyzer, constant_def);
    }
    for (usize i = 0; i < module->global_constants.len; i++) {
        ConstantDef* constant_def = &module->global_constants.data[i];
        type_constant_def(&analyzer, constant_def);
    }
    for (usize i = 0; i < module->global_constants.len; i++) {
        ConstantDef* constant_def = &module->global_constants.data[i];
        analyze_constant_def(&analyzer, constant_def);
    }
    return;
}

static void register_constant_def(Analyzer* analyzer, ConstantDef* constant_def) {
    ValueBinding binding = {
        .name = constant_def->name.string,
        .type = TYPE_INVALID,
        .store = {
            .kind = VALUE_STORE_CONSTANT,
            .as.constant = constant_def->value,
        },
    };
    BindingMut slot;
    if (!insert_value_binding(
        analyzer->bindings,
        analyzer->scope_location.scope_id,
        binding,
        &slot
    )) {
        // TODO: error handling
        return;
    }
    constant_def->binding = slot.id;
    return;
}

static void type_constant_def(Analyzer* analyzer, ConstantDef* constant_def) {
    if (constant_def->explicitly_typed) {
        TypeId type;
        resolve_type(analyzer, constant_def->type_name, &type);
        constant_def->type = type;
        ValueBinding* binding = get_binding_mut(
            analyzer->bindings,
            constant_def->binding
        ).as.value;
        binding->type = type;
    }

    Expression* value = &analyzer->expressions[constant_def->value];
    switch (value->kind) {
    case EXPRESSION_FUNCTION:;
        Function* function = &value->as.function;
        analyze_function_signature(analyzer, function);
        if (!constant_def->explicitly_typed) {
            FunctionType type = {
                .input = function->input.type,
                .output = function->output_type,
            };
            TypeId type_id = get_or_register_function_type(analyzer->types, type);
            value->type = type_id;
            constant_def->type = type_id;
            ValueBinding* binding = get_binding_mut(
                analyzer->bindings,
                constant_def->binding
            ).as.value;
            binding->type = type_id;
        }
        break;

    default:
        // TODO: error handling
        return;
    }
}

static void analyze_constant_def(Analyzer* analyzer, ConstantDef* constant_def) {
    Expression* expression = &analyzer->expressions[constant_def->value];
    if (expression->kind == EXPRESSION_FUNCTION) {
        // already analyzed the function signature.
        analyze_function_body(analyzer, &expression->as.function);
        return;
    }
    analyze_expression(analyzer, expression);
}

static void analyze_function_signature(Analyzer* analyzer, Function* function) {
    ScopeLocation output_scope =
        scope_new(analyzer->bindings, analyzer->scope_location);
    Analyzer function_analyzer =
        with_scope_location(*analyzer, output_scope);
    function_analyzer.function_variable_space = &function->variable_space;
    function_analyzer.function_id = function->function_id;
    analyze_pattern(&function_analyzer, &function->input);

    if (!function->explicit_output_type) {
        // TODO: error handling;
        return;
    }
    resolve_type(analyzer, function->output_type_name, &function->output_type);
}

static void analyze_pattern(Analyzer* parent, Pattern* pattern) {
    ScopeLocation scope = scope_new(parent->bindings, parent->scope_location);
    pattern->scope = scope.scope_id;
    Analyzer analyzer = with_scope_location(*parent, scope);

    TypeId inner_type = TYPE_INVALID;
    switch (pattern->kind) {
    case PATTERN_VARIABLE:
        analyze_variable_def(&analyzer, &pattern->as.variable);
        inner_type = pattern->as.variable.type;
        break;
    }

    if (pattern->explicitly_typed) {
        resolve_type(&analyzer, pattern->type_name, &pattern->type);
    } else {
        pattern->type = inner_type;
    }
    if (
        pattern->type != TYPE_INVALID
        && inner_type != TYPE_INVALID
        && pattern->type != inner_type
    ) {
        // TODO: error handling
        if (!pattern->explicitly_typed) {
            pattern->type = inner_type;
        }
    }
}

static void analyze_variable_def(Analyzer* analyzer, VariableDef* variable_def) {
    if (!variable_def->explicitly_typed) {
        // TODO: error handling
        return;
    }
    resolve_type(analyzer, variable_def->type_name, &variable_def->type);
    ValueBinding binding = {
        .name = variable_def->name.string,
        .type = variable_def->type,
        .store = {
            .kind = VALUE_STORE_VARIABLE,
            .as.variable = {
                .index = (*analyzer->function_variable_space)++,
                .function_id = analyzer->function_id,
            },
        },
    };
    BindingMut binding_entry;
    if (!push_value_binding(
        analyzer->bindings,
        &analyzer->scope_location,
        binding,
        &binding_entry
    )) {
        // TODO: error handling
        return;
    }
    variable_def->binding = binding_entry.id;
}

static void analyze_expression(Analyzer* analyzer, Expression* expression) {
    switch (expression->kind) {
    case EXPRESSION_VARIABLE:
        analyze_variable_ref(analyzer, &expression->as.variable);
        expression->type =
            get_binding(*analyzer->bindings, expression->as.variable.binding).as.value.type;
        break;
    
    case EXPRESSION_FUNCTION:
        analyze_function_signature(analyzer, &expression->as.function);
        analyze_function_body(analyzer, &expression->as.function);
        FunctionType type = {
            .input = expression->as.function.input.type,
            .output = expression->as.function.output_type,
        };
        TypeId type_id = get_or_register_function_type(analyzer->types, type);
        expression->type = type_id;
        break;

    case EXPRESSION_LITERAL_BOOL:
        expression->type = TYPE_BOOL;
        break;

    case EXPRESSION_UNARY_OPERATION:
        analyze_unary_operation(analyzer, &expression->as.unary_operation, &expression->type);
        break;

    case EXPRESSION_BINARY_OPERATION:
        analyze_binary_operation(analyzer, &expression->as.binary_operation, &expression->type);
        break;
    }
}

static void analyze_unary_operation(
    Analyzer* analyzer,
    UnaryOperation* unary_operation,
    TypeId* dst
) {
    Expression* operand = &analyzer->expressions[unary_operation->operand];
    analyze_expression(analyzer, operand);

    switch (unary_operation->operator) {
    case OPERATION_NOT:
        if (operand->type != TYPE_BOOL) {
            // TODO: error handling
            exit(-1);
        }
        *dst = TYPE_BOOL;
        break;
    }
}

static void analyze_binary_operation(
    Analyzer* analyzer,
    BinaryOperation* binary_operation,
    TypeId* dst
) {
    Expression* lhs = &analyzer->expressions[binary_operation->operand_left];
    Expression* rhs = &analyzer->expressions[binary_operation->operand_right];
    analyze_expression(analyzer, lhs);
    analyze_expression(analyzer, rhs);

    switch (binary_operation->operator) {
    case OPERATION_FUNCTION_CALL:;
        Type lhs_type = get_type(*analyzer->types, lhs->type);
        if (lhs_type.kind != TYPE_FUNCTION) {
            // TODO: error handling
            log_error("tried to call non-function value");
            exit(-1);
        }
        if (lhs_type.as.function.input != rhs->type) {
            // TODO: error handling
        }
        *dst = lhs_type.as.function.output;
        break;

    case OPERATION_OR:
    case OPERATION_AND:;
        if (lhs->type != TYPE_BOOL || rhs->type != TYPE_BOOL) {
            // TODO: error handling
        }
        *dst = TYPE_BOOL;
        break;
    }
}

static void analyze_variable_ref(Analyzer* analyzer, VariableRef* variable_ref) {
    BindingId binding_id;
    if (!find_binding(
        *analyzer->bindings,
        analyzer->scope_location,
        variable_ref->name.string,
        &binding_id
    )) {
        // TODO: error handling
        return;
    }
    // FIXME: use the binding id directly?
    Binding binding = get_binding(*analyzer->bindings, binding_id);
    if (binding.kind != BINDING_VALUE) {
        // TODO: error handling
        return;
    }
    if (binding.as.value.store.kind == VALUE_STORE_VARIABLE) {
        if (binding.as.value.store.as.variable.function_id != analyzer->function_id) {
            // TODO: error handling
            log_error("variable accessed outside of function");
            exit(-1);
        }
    }
    variable_ref->binding = binding_id;
}

static void analyze_function_body(Analyzer* parent, Function* function) {
    ScopeLocation scope = scope_new(
        parent->bindings,
        scope_end_location(*parent->bindings, function->input.scope)
    );
    function->output_scope = scope.scope_id;
    Analyzer analyzer = with_scope_location(*parent, scope);
    analyzer.function_variable_space = &function->variable_space;
    analyzer.function_id = function->function_id;
    Expression* output = &analyzer.expressions[function->output];
    analyze_expression(&analyzer, output);
    if (output->type != function->output_type) {
        // TODO: error handling
    }
}

static void resolve_type(Analyzer* analyzer, TypeName name, TypeId* dst) {
    switch (name.kind) {
    case TYPE_NAME_IDENTIFIER:;
        BindingId binding_id;
        if (!find_binding(
            *analyzer->bindings,
            analyzer->scope_location,
            name.as.identifier.string,
            &binding_id
        )) {
            // TODO: error handling
            return;
        }
        Binding binding = get_binding(*analyzer->bindings, binding_id);
        if (binding.kind != BINDING_TYPE) {
            // TODO: error handling
            return;
        }
        *dst = binding.as.type.type;
    }
}
