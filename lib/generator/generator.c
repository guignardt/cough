#include <assert.h>

#include "generator/generator.h"

typedef struct Generator {
    Emitter* emitter;
    Expression const* expressions;
    BindingRegistry bindings;
} Generator;

static Generator generate_functions(AstData data, Emitter* emitter);
static void generate_function(Generator gen, Function function);
static void generate_pattern_match(Generator gen, Pattern pattern);
static void generate_expression2(Generator gen, Expression expression);
static void generate_unary_operation(Generator gen, UnaryOperation unary_operation);
static void generate_binary_operation(Generator gen, BinaryOperation binary_operation);
static void generate_conditional(Generator gen, Conditional conditional);

// FIXME: change this, since we don't use the module?
void generate_module(Module module, AstData data, Emitter* emitter) {
    generate_functions(data, emitter);
}

static Generator generate_functions(AstData data, Emitter* emitter) {
    emit_many_new_symbols(emitter, data.functions.len);
    Generator generator = {
        .emitter = emitter,
        .expressions = data.expressions.data,
        .bindings = data.bindings,
    };
    for (size_t i = 0; i < data.functions.len; i++) {
        Function function = data.expressions.data[data.functions.data[i]].as.function;
        generate_function(generator, function);
    }
    return generator;
}

static void generate_function(Generator gen, Function function) {
    // function_id == symbol
    emit_symbol_location(gen.emitter, function.function_id);
    if (function.variable_space > 0) {
        emit(res)(gen.emitter, function.variable_space);
    }
    generate_pattern_match(gen, function.input);
    generate_expression2(gen, gen.expressions[function.output]);
    emit(ret)(gen.emitter);
}

static void generate_pattern_match(Generator gen, Pattern pattern) {
    switch (pattern.kind) {
    case PATTERN_VARIABLE:;
        BindingId binding_id = pattern.as.variable.binding;
        Binding binding = get_binding(gen.bindings, binding_id);
        assert(binding.kind == BINDING_VALUE);
        assert(binding.as.value.store.kind == VALUE_STORE_VARIABLE);
        usize variable_index = binding.as.value.store.as.variable.index;
        emit(set)(gen.emitter, variable_index);
        break;
    }
}

static void generate_expression2(Generator gen, Expression expression) {
    switch (expression.kind) {
    case EXPRESSION_VARIABLE:;
        BindingId binding_id = expression.as.variable.binding;
        Binding binding = get_binding(gen.bindings, binding_id);
        assert(binding.kind == BINDING_VALUE);
        switch (binding.as.value.store.kind) {
        case VALUE_STORE_CONSTANT:;
            Expression value = gen.expressions[binding.as.value.store.as.constant];
            switch (value.kind) {
            case EXPRESSION_FUNCTION:
                // function_id == symbol
                emit(loc)(gen.emitter, value.as.function.function_id);
                break;
            default:
                // internal error
                log_error("unsupported constant expression (internal error)\n");
                exit(-1);
                return;
            }
            break;
        case VALUE_STORE_VARIABLE:
            emit(var)(gen.emitter, binding.as.value.store.as.variable.index);
            return;
        }
        break;
    
    case EXPRESSION_FUNCTION:
        // function_id == symbol
        emit(loc)(gen.emitter, expression.as.function.function_id);
        break;

    case EXPRESSION_LITERAL_BOOL:
        // all 0s for false, all 1s for true
        // this ensures `not` & friends work correctly as the VM doesn't use a dedicated
        // boolean type, and instead uses `UInt`.
        emit(sca)(
            gen.emitter,
            (Word){ .as_uint = -expression.as.literal_bool }
        );
        break;

    case EXPRESSION_LITERAL_UINT:
        // all 0s for false, all 1s for true
        // this ensures `not` & friends work correctly as the VM doesn't use a dedicated
        // boolean type, and instead uses `UInt`.
        emit(sca)(
            gen.emitter,
            (Word){ .as_uint = expression.as.literal_uint }
        );
        break;

    case EXPRESSION_LITERAL_INT:
        // all 0s for false, all 1s for true
        // this ensures `not` & friends work correctly as the VM doesn't use a dedicated
        // boolean type, and instead uses `UInt`.
        emit(sca)(
            gen.emitter,
            (Word){ .as_int = expression.as.literal_int }
        );
        break;
            
    case EXPRESSION_LITERAL_FLOAT:
        // all 0s for false, all 1s for true
        // this ensures `not` & friends work correctly as the VM doesn't use a dedicated
        // boolean type, and instead uses `UInt`.
        emit(sca)(
            gen.emitter,
            (Word){ .as_float = expression.as.literal_float }
        );
        break;

    case EXPRESSION_UNARY_OPERATION:
        generate_unary_operation(gen, expression.as.unary_operation);
        break;

    case EXPRESSION_BINARY_OPERATION:
        generate_binary_operation(gen, expression.as.binary_operation);
        break;

    case EXPRESSION_CONDITIONAL:
        generate_conditional(gen, expression.as.conditional);
        break;
    }
}

static void generate_unary_operation(Generator gen, UnaryOperation unary_operation) {
    Expression operand = gen.expressions[unary_operation.operand];
    generate_expression2(gen, operand);
    switch (unary_operation.operator) {
    case OPERATION_NOT:
        emit(not)(gen.emitter);
        break;

    case OPERATION_NEG_INT:
        emit(ngi)(gen.emitter);
        break;

    case OPERATION_NEG_FLOAT:
        emit(ngi)(gen.emitter);
        break;
    }
}

static void generate_binary_operation(Generator gen, BinaryOperation binary_operation) {
    Expression first = gen.expressions[binary_operation.first];
    Expression second = gen.expressions[binary_operation.second];

    generate_expression2(gen, first);
    generate_expression2(gen, second);

    switch (binary_operation.operator) {
    case OPERATION_FUNCTION_CALL:
        emit(cal)(gen.emitter);
        break;

    case OPERATION_OR:
        emit(lor)(gen.emitter);
        break;
    case OPERATION_AND:
        emit(and)(gen.emitter);
        break;
    case OPERATION_XOR:
        emit(xor)(gen.emitter);
        break;

    case OPERATION_ADD_UINT:
        emit(adu)(gen.emitter);
        break;
    case OPERATION_SUB_UINT:
        emit(sbu)(gen.emitter);
        break;
    case OPERATION_MUL_UINT:
        emit(mlu)(gen.emitter);
        break;
    case OPERATION_DIV_UINT:
        emit(dvu)(gen.emitter);
        break;

    case OPERATION_ADD_INT:
        emit(adi)(gen.emitter);
        break;
    case OPERATION_SUB_INT:
        emit(sbi)(gen.emitter);
        break;
    case OPERATION_MUL_INT:
        emit(mli)(gen.emitter);
        break;
    case OPERATION_DIV_INT:
        emit(dvi)(gen.emitter);
        break;

    case OPERATION_ADD_FLOAT:
        emit(adf)(gen.emitter);
        break;
    case OPERATION_SUB_FLOAT:
        emit(sbf)(gen.emitter);
        break;
    case OPERATION_MUL_FLOAT:
        emit(mlf)(gen.emitter);
        break;
    case OPERATION_DIV_FLOAT:
        emit(dvf)(gen.emitter);
        break;
    }
}

static void generate_conditional(Generator gen, Conditional conditional) {
    SymbolIndex if_false_symbol = emit_new_symbol(gen.emitter);
    SymbolIndex after_else_symbol = -1;
    generate_expression2(gen, gen.expressions[conditional.condition]);
    emit(jze)(gen.emitter, if_false_symbol);
    generate_expression2(gen, gen.expressions[conditional.if_true]);
    if (conditional.has_else) {
        after_else_symbol = emit_new_symbol(gen.emitter);
        emit(jmp)(gen.emitter, after_else_symbol);
    }
    emit_symbol_location(gen.emitter, if_false_symbol);
    if (conditional.has_else) {
        generate_expression2(gen, gen.expressions[conditional.if_false]);
        emit_symbol_location(gen.emitter, after_else_symbol);
    }
}
