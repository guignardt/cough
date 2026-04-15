#include "tests/common.h"

int main(int argc, char const *argv[]) {
    char const source[] = 
        "identity :: fn x: Bool -> Bool => x;\n"
    ;
    Module module;
    AstData data;
    source_to_module(STRING_LITERAL(source), &module, &data);

    // we don't check everything -- a more thorough test is conducted
    // by `ast/constant_fn`.
    
    assert(module.global_constants.len == 1);
    ConstantDef identity_def = module.global_constants.data[0];

    TypeId fn_bool_bool_type = get_or_register_function_type(
        &data.types,
        (FunctionType){ .input = TYPE_BOOL, .output = TYPE_BOOL },
        &data.storage
    );
    assert(identity_def.type == fn_bool_bool_type);

    Expression value = data.expressions.data[identity_def.value];
    assert(value.kind == EXPRESSION_FUNCTION);
    Function identity = value.as.function;

    assert(identity.input.kind == PATTERN_VARIABLE);
    BindingId x_binding_id = identity.input.as.variable.binding;
    Binding x_binding = get_binding(data.bindings, x_binding_id);
    assert(x_binding.kind == BINDING_VALUE);
    assert(eq(String)(x_binding.as.value.name, STRING_LITERAL("x")));
    assert(x_binding.as.value.type == TYPE_BOOL);
    assert(x_binding.as.value.store.kind == VALUE_STORE_VARIABLE);

    Expression output = data.expressions.data[identity.output];
    assert(output.kind == EXPRESSION_VARIABLE);
    assert(eq(String)(output.as.variable.name.string, STRING_LITERAL("x")));
    assert(output.as.variable.binding == x_binding_id);

    return 0;
}
