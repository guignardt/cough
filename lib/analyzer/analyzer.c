#include <assert.h>

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

static void unavailable_name(Analyzer* analyzer, Identifier ident, BindingMut binding) {
    StringBuf note;
    Range def_range;
    switch (binding.kind) {
    case BINDING_TYPE:;
        note = format("note: a type with the same name was defined here:");
        def_range = binding.as.type->source_definition;
        break;
    case BINDING_VALUE:
        note = format("note: a constant with the same name was defined here:");
        def_range = binding.as.value->source_definition;
    }
    report_start(analyzer->reporter, SEVERITY_ERROR, CE_DUPLICATE_BINDING_NAME);
    report_message(
        analyzer->reporter,
        format("unavailable binding name `%.*s`", (int)ident.string.len, ident.string.data)
    );
    report_source_code(analyzer->reporter, ident.range);
    report_message(analyzer->reporter, note);
    report_source_code(analyzer->reporter, def_range);
    report_end(analyzer->reporter);
}

static void expected_constant(Analyzer* analyzer, Expression expr) {
    report_start(analyzer->reporter, SEVERITY_ERROR, CE_EXPECTED_CONSTANT);
    report_message(analyzer->reporter, format("expected constant expression"));
    report_source_code(analyzer->reporter, expr.range);
    report_end(analyzer->reporter);
}

static void constant_function_missing_explicit_return_type(Analyzer* analyzer, Function expr) {
    report_start(analyzer->reporter, SEVERITY_ERROR, CE_IMPLICIT_TYPE);
    report_message(analyzer->reporter, format("expected explicit return type in constant function"));
    report_source_code(analyzer->reporter, expr.signature_range);
    report_end(analyzer->reporter);
}

static void mismatched_types(
    Analyzer* analyzer,
    TypeId expected,
    bool explicit_constraint,
    Range expected_source,
    TypeId found,
    Range found_range
) {
    report_start(analyzer->reporter, SEVERITY_ERROR, CE_MISMATCHED_TYPES);
    String expected_name = get_type(*analyzer->types, expected).pretty_name;
    String found_name = get_type(*analyzer->types, found).pretty_name;
    report_message(analyzer->reporter, format(
        "mismatched types: expected value of type `%.*s`, found `%.*s`",
        (int)expected_name.len, expected_name.data,
        (int)found_name.len, found_name.data
    ));
    report_source_code(analyzer->reporter, found_range);
    if (explicit_constraint) {
        report_message(analyzer->reporter, format("note: type constraint here:"));
        report_source_code(analyzer->reporter, expected_source);
    }
    report_end(analyzer->reporter);
}

static void binding_not_found(Analyzer* analyzer, Identifier name) {
    report_start(analyzer->reporter, SEVERITY_ERROR, CE_BINDING_NOT_FOUND);
    report_message(analyzer->reporter, format(
        "no binding with name `%.*s` in the current scope",
        (int)name.string.len, name.string.data
    ));
    report_source_code(analyzer->reporter, name.range);
    report_end(analyzer->reporter);
}

static void binding_not_type(Analyzer* analyzer, Identifier name, Binding binding) {
    report_start(analyzer->reporter, SEVERITY_ERROR, CE_INVALID_BINDING_KIND);
    report_message(analyzer->reporter, format(
        "expected type, found value binding `%.*s`",
        (int)name.string.len, name.string.data
    ));
    report_source_code(analyzer->reporter, name.range);
    report_message(analyzer->reporter, format("note: this binding was defined here:"));
    report_source_code(analyzer->reporter, binding.as.value.source_definition);
    report_end(analyzer->reporter);
}

static void binding_not_value(Analyzer* analyzer, Identifier name, Binding binding) {
    report_start(analyzer->reporter, SEVERITY_ERROR, CE_INVALID_BINDING_KIND);
    report_message(analyzer->reporter, format(
        "expected value, found type binding `%.*s`",
        (int)name.string.len, name.string.data
    ));
    report_source_code(analyzer->reporter, name.range);
    report_message(analyzer->reporter, format("note: this binding was defined here:"));
    report_source_code(analyzer->reporter, binding.as.type.source_definition);
    report_end(analyzer->reporter);
}

static void called_non_function(Analyzer* analyzer, TypeId type, Range range) {
    String name = get_type(*analyzer->types, type).pretty_name;
    report_start(analyzer->reporter, SEVERITY_ERROR, CE_MISMATCHED_TYPES);
    report_message(analyzer->reporter, format(
        "tried to call non-function value of type `%.*s`",
        (int)name.len, name.data
    ));
    report_source_code(analyzer->reporter, range);
    report_end(analyzer->reporter);
}

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

static void analyze_module2(Analyzer* analyzer, Module* module);
static void register_constant_def(Analyzer* analyzer, ConstantDef* constant_def);
static void type_constant_def(Analyzer* analyzer, ConstantDef* constant_def);
static void analyze_constant_def(Analyzer* analyzer, ConstantDef* constant_def);
static void analyze_function_signature(Analyzer* analyzer, Function* function);
static void analyze_pattern(Analyzer* analyzer, Pattern* pattern);
static void analyze_variable_def(Analyzer* analyzer, VariableDef* variable_def);
static void analyze_expression2(Analyzer* analyzer, Expression* expression);
static void analyze_unary_operation(Analyzer* analyzer, UnaryOperation* unary_operation, Range range, TypeId* dst);
static void analyze_binary_operation(Analyzer* analyzer, BinaryOperation* binary_operation, Range range, TypeId* dst);
static TypeId type_bitwise_binary( Analyzer* analyzer, Expression* first, Expression* second);
static TypeId type_arithmetic_binary( Analyzer* analyzer, Expression* first, Expression* second, int* index);
static void analyze_variable_ref(Analyzer* analyzer, VariableRef* variable_ref);
static void analyze_function_body(Analyzer* analyzer, Function* function);
static void resolve_type(Analyzer* analyzer, TypeName name, TypeId* dst);

void analyze_module(Module* module, AstData* data, Reporter* reporter) {
    Analyzer analyzer = {
        .source = data->source,
        .reporter = reporter,
        .types = &data->types,
        .bindings = &data->bindings,
        .scope_location = scope_end_location(data->bindings, ROOT_SCOPE_ID),
        .expressions = data->expressions.data,
        .function_variable_space = NULL,
        .function_id = 0,
        .storage = &data->storage,
    };
    analyze_module2(&analyzer, module);
}

void analyze_expression(Expression* expression, AstData* data, Reporter* reporter) {
    Analyzer analyzer = {
        .source = data->source,
        .reporter = reporter,
        .types = &data->types,
        .bindings = &data->bindings,
        .scope_location = scope_end_location(data->bindings, ROOT_SCOPE_ID),
        .expressions = data->expressions.data,
        .function_variable_space = NULL,
        .function_id = 0,
        .storage = &data->storage,
    };
    analyze_expression2(&analyzer, expression);
}

static void analyze_module2(Analyzer* parent, Module* module) {
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
        .source_definition = constant_def->range,
    };
    BindingMut slot;
    if (!insert_value_binding(
        analyzer->bindings,
        analyzer->scope_location.scope_id,
        binding,
        &slot
    )) {
        unavailable_name(analyzer, constant_def->name, slot);
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
            TypeId type_id = get_or_register_function_type(analyzer->types, type, analyzer->storage);
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
        expected_constant(analyzer, *value);
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
    analyze_expression2(analyzer, expression);
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
        constant_function_missing_explicit_return_type(analyzer, *function);
        return;
    }
    resolve_type(analyzer, function->output_type_name, &function->output_type);
}

static void analyze_pattern(Analyzer* parent, Pattern* pattern) {
    ScopeLocation scope = scope_new(parent->bindings, parent->scope_location);
    pattern->scope = scope.scope_id;
    Analyzer analyzer = with_scope_location(*parent, scope);

    TypeId inner_type = TYPE_INVALID;
    Range inner_range;
    switch (pattern->kind) {
    case PATTERN_VARIABLE:
        analyze_variable_def(&analyzer, &pattern->as.variable);
        inner_type = pattern->as.variable.type;
        inner_range = pattern->as.variable.range;
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
        // pattern is explicitly typed: see conditional above
        // range is set as we parsed an inner pattern
        mismatched_types(
            &analyzer,
            pattern->type,
            true,
            pattern->type_name.range,
            inner_type,
            inner_range
        );
        if (!pattern->explicitly_typed) {
            pattern->type = inner_type;
        }
    }
}

static void analyze_variable_def(Analyzer* analyzer, VariableDef* variable_def) {
    if (!variable_def->explicitly_typed) {
        // currently, all variables are parsed as having an explicit type
        log_error(
            "variable `%.*s` wasn't explicity types (internal error)",
            (int)variable_def->name.string.len, variable_def->name.string.data
        );
        exit(-1);
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
        .source_definition = variable_def->range,
    };
    BindingMut binding_entry;
    if (!push_value_binding(
        analyzer->bindings,
        &analyzer->scope_location,
        binding,
        &binding_entry
    )) {
        unavailable_name(analyzer, variable_def->name, binding_entry);
        return;
    }
    variable_def->binding = binding_entry.id;
}

static void analyze_expression2(Analyzer* analyzer, Expression* expression) {
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
        TypeId type_id = get_or_register_function_type(analyzer->types, type, analyzer->storage);
        expression->type = type_id;
        break;

    case EXPRESSION_LITERAL_BOOL:
        expression->type = TYPE_BOOL;
        break;
    case EXPRESSION_LITERAL_UINT:
        expression->type = TYPE_UINT;
        break;
    case EXPRESSION_LITERAL_INT:
        expression->type = TYPE_INT;
        break;
    case EXPRESSION_LITERAL_FLOAT:
        expression->type = TYPE_FLOAT;
        break;

    case EXPRESSION_UNARY_OPERATION:
        analyze_unary_operation(analyzer, &expression->as.unary_operation, expression->range, &expression->type);
        break;

    case EXPRESSION_BINARY_OPERATION:
        analyze_binary_operation(analyzer, &expression->as.binary_operation, expression->range, &expression->type);
        break;
    }
}

static void analyze_unary_operation(
    Analyzer* analyzer,
    UnaryOperation* unary_operation,
    Range range,
    TypeId* dst
) {
    Expression* operand = &analyzer->expressions[unary_operation->operand];
    analyze_expression2(analyzer, operand);

    switch (unary_operation->operator) {
    case OPERATION_NOT:
        if (operand->type != TYPE_BOOL && operand->type != TYPE_INVALID) {
            mismatched_types(
                analyzer,
                TYPE_BOOL,
                true,
                range,
                operand->type,
                operand->range
            );
        }
        *dst = TYPE_BOOL;
        break;

    // only `OPERATION_NEG` is actually produced by the parser
    case OPERATION_NEG: // = `OPERATION_NEG_INT`
    case OPERATION_NEG_FLOAT:
        switch (operand->type) {
        case TYPE_INT:
            unary_operation->operator = OPERATION_NEG_INT;
            *dst = TYPE_INT;
            break;
        case TYPE_FLOAT:
            unary_operation->operator = OPERATION_NEG_FLOAT;
            *dst = TYPE_FLOAT;
            break;
        case TYPE_INVALID: break;
        default:
            // TODO: mismatched type for ambiguous operator
            exit(-1);
        }
        break;
    }
}

static void analyze_binary_operation(
    Analyzer* analyzer,
    BinaryOperation* binary_operation,
    Range range,
    TypeId* dst
) {
    Expression* first = &analyzer->expressions[binary_operation->first];
    Expression* second = &analyzer->expressions[binary_operation->second];
    analyze_expression2(analyzer, first);
    analyze_expression2(analyzer, second);

    int index;
    switch (binary_operation->operator) {
    case OPERATION_FUNCTION_CALL:;
        if (second->type == TYPE_INVALID) {
            return;
        }
        Type callee_type = get_type(*analyzer->types, second->type);
        if (callee_type.kind != TYPE_FUNCTION) {
            called_non_function(analyzer, second->type, second->range);
            return;
        }
        if (callee_type.as.function.input != first->type) {
            mismatched_types(
                analyzer,
                callee_type.as.function.input,
                false,
                (Range){0},
                first->type,
                first->range
            );
        }
        *dst = callee_type.as.function.output;
        break;

    case OPERATION_OR:
    case OPERATION_AND:
    case OPERATION_XOR:;
        *dst = type_bitwise_binary(analyzer, first, second);
        break;

    // only `OPERATION_ADD` ist actually produced by the parser
    case OPERATION_ADD: // = `OPERATION_ADD_UINT`
    case OPERATION_ADD_INT:
    case OPERATION_ADD_FLOAT:;
        *dst = type_arithmetic_binary(analyzer, first, second, &index);
        binary_operation->operator = OPERATION_ADD + index;
        break;

    // only `OPERATION_SUB` ist actually produced by the parser
    case OPERATION_SUB: // = `OPERATION_ADD_UINT`
    case OPERATION_SUB_INT:
    case OPERATION_SUB_FLOAT:;
        *dst = type_arithmetic_binary(analyzer, first, second, &index);
        binary_operation->operator = OPERATION_SUB + index;
        break;

    // only `OPERATION_MUL` ist actually produced by the parser
    case OPERATION_MUL: // = `OPERATION_ADD_UINT`
    case OPERATION_MUL_INT:
    case OPERATION_MUL_FLOAT:;
        *dst = type_arithmetic_binary(analyzer, first, second, &index);
        binary_operation->operator = OPERATION_MUL + index;
        break;

    // only `OPERATION_DIV` ist actually produced by the parser
    case OPERATION_DIV: // = `OPERATION_ADD_UINT`
    case OPERATION_DIV_INT:
    case OPERATION_DIV_FLOAT:;
        *dst = type_arithmetic_binary(analyzer, first, second, &index);
        binary_operation->operator = OPERATION_DIV + index;
        break;
    }
}

static TypeId type_bitwise_binary(
    Analyzer* analyzer,
    Expression* lhs,
    Expression* rhs
) {
    if (lhs->type != rhs->type) {
        mismatched_types(
            analyzer,
            lhs->type,
            true,
            lhs->range,
            rhs->type,
            rhs->range
        );
        return TYPE_INVALID;
    }
    if (lhs->type != TYPE_BOOL && lhs->type != TYPE_UINT && lhs->type != TYPE_INT) {
        if (lhs->type != TYPE_INVALID) {
            mismatched_types(
                analyzer,
                TYPE_BOOL,
                false,
                (Range){0},
                lhs->type,
                lhs->range
            );
        }
        return TYPE_INVALID;
    }
    return lhs->type;
}

static TypeId type_arithmetic_binary(
    Analyzer* analyzer,
    Expression* lhs,
    Expression* rhs,
    int* index  // sets to 0 for uint, 1 for int, 2 for float
) {
    if (lhs->type != rhs->type) {
        mismatched_types(
            analyzer,
            lhs->type,
            true,
            lhs->range,
            rhs->type,
            rhs->range
        );
        return TYPE_INVALID;
    }
    if (lhs->type != TYPE_UINT && lhs->type != TYPE_INT && lhs->type != TYPE_FLOAT) {
        if (lhs->type != TYPE_INVALID) {
            mismatched_types(
                analyzer,
                TYPE_BOOL,
                false,
                (Range){0},
                lhs->type,
                lhs->range
            );
        }
        return TYPE_INVALID;
    }
    int indices[] = { [TYPE_UINT] = 0, [TYPE_INT] = 1, [TYPE_FLOAT] = 2 };
    *index = indices[lhs->type];
    return lhs->type;
}

static void analyze_variable_ref(Analyzer* analyzer, VariableRef* variable_ref) {
    BindingId binding_id;
    if (!find_binding(
        *analyzer->bindings,
        analyzer->scope_location,
        variable_ref->name.string,
        &binding_id
    )) {
        binding_not_found(analyzer, variable_ref->name);
        return;
    }
    // FIXME: use the binding id directly?
    Binding binding = get_binding(*analyzer->bindings, binding_id);
    if (binding.kind != BINDING_VALUE) {
        binding_not_value(analyzer, variable_ref->name, binding);
        return;
    }
    if (binding.as.value.store.kind == VALUE_STORE_VARIABLE) {
        if (binding.as.value.store.as.variable.function_id != analyzer->function_id) {
            binding_not_found(analyzer, variable_ref->name);
            return;
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
    analyze_expression2(&analyzer, output);
    if (output->type != function->output_type && output->type != TYPE_INVALID) {
        // currently, all functions have an explicit return type
        assert(function->explicit_output_type);
        mismatched_types(
            &analyzer,
            function->output_type,
            true,
            function->output_type_name.range,
            output->type,
            output->range
        );
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
            binding_not_found(analyzer, name.as.identifier);
            return;
        }
        Binding binding = get_binding(*analyzer->bindings, binding_id);
        if (binding.kind != BINDING_TYPE) {
            binding_not_type(analyzer, name.as.identifier, binding);
            return;
        }
        *dst = binding.as.type.type;
    }
}
