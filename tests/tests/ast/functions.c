#include "tests/common.h"

int main(int argc, char const** argv) {
    Module module;
    AstData data;
    source_to_module(STRING_LITERAL(
        "wrap :: fn x: Bool -> Bool => identity(x);\n"
        "identity :: fn y: Bool -> Bool => y;\n"
    ), &module, &data);
    
    assert(module.global_constants.len == 2);

    // we don't repeat tests conducted in ast/identity_fn

    ConstantDef wrap_def = module.global_constants.data[0];
    assert(eq(String)(wrap_def.name.string, STRING_LITERAL("wrap")));
    Expression wrap_value = data.expressions.data[wrap_def.value];
    assert(wrap_value.kind == EXPRESSION_FUNCTION);
    Function wrap_fn = wrap_value.as.function;
    assert(wrap_fn.input.type == TYPE_BOOL);
    assert(wrap_fn.output_type == TYPE_BOOL);
    Expression output = data.expressions.data[wrap_fn.output];
    assert(output.kind == EXPRESSION_BINARY_OPERATION);
    BinaryOperation call = output.as.binary_operation;
    assert(call.operator == OPERATION_FUNCTION_CALL);
    Expression callee = data.expressions.data[call.second];
    assert(callee.kind == EXPRESSION_VARIABLE);
    Binding callee_binding = get_binding(data.bindings, callee.as.variable.binding);
    assert(callee_binding.kind == BINDING_VALUE);
    assert(eq(String)(callee_binding.as.value.name, STRING_LITERAL("identity")));
    Expression argument = data.expressions.data[call.first];
    assert(argument.kind == EXPRESSION_VARIABLE);
    Binding argument_binding = get_binding(data.bindings, argument.as.variable.binding);
    assert(eq(String)(argument_binding.as.value.name, STRING_LITERAL("x")));

    return 0;
}
