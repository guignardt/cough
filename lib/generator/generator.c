#include <assert.h>

#include "generator/generator.h"

typedef struct Generator {
    Emitter* emitter;
    Expression const* expressions;
    BindingRegistry bindings;
} Generator;

static void generate_function(Generator gen, Function function);
static void generate_pattern_match(Generator gen, Pattern pattern);
static void generate_expression(Generator gen, Expression expression);
static void generate_unary_operation(Generator gen, UnaryOperation unary_operation);
static void generate_binary_operation(Generator gen, BinaryOperation binary_operation);

void generate(Ast* ast, Emitter* emitter) {
    for (size_t i = 0; i < ast->functions.len; i++) {
        Function* function = &ast->expressions.data[ast->functions.data[i]].as.function;
        function->symbol = emit_new_symbol(emitter);
    }

    Generator generator = {
        .emitter = emitter,
        .expressions = ast->expressions.data,
        .bindings = ast->bindings,
    };

    for (size_t i = 0; i < ast->functions.len; i++) {
        Function function = ast->expressions.data[ast->functions.data[i]].as.function;
        generate_function(generator, function);
    }
}

static void generate_function(Generator gen, Function function) {
    emit_symbol_location(gen.emitter, function.symbol);
    if (function.variable_space > 0) {
        emit(res)(gen.emitter, function.variable_space);
    }
    generate_pattern_match(gen, function.input);
    generate_expression(gen, gen.expressions[function.output]);
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

static void generate_expression(Generator gen, Expression expression) {
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
                emit(loc)(gen.emitter, value.as.function.symbol);
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
        emit(loc)(gen.emitter, expression.as.function.symbol);
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
    }
}

static void generate_unary_operation(Generator gen, UnaryOperation unary_operation) {
    Expression operand = gen.expressions[unary_operation.operand];
    generate_expression(gen, operand);
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
    Expression lhs = gen.expressions[binary_operation.operand_left];
    Expression rhs = gen.expressions[binary_operation.operand_right];

    if (binary_operation.operator == OPERATION_FUNCTION_CALL) {
        // functions currently only consist of a location
        generate_expression(gen, rhs);
        generate_expression(gen, lhs);
    } else {
        generate_expression(gen, lhs);
        generate_expression(gen, rhs);
    }

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
