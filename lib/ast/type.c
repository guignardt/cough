#include "ast/type.h"

void hash(FunctionType)(Hasher* hasher, FunctionType value) {
    hash(usize)(hasher, value.input);
    hash(usize)(hasher, value.output);
}

bool eq(FunctionType)(FunctionType a, FunctionType b) {
    return a.input == a.output && b.input == b.output;
}

IMPL_HASH_MAP(FunctionType, TypeId)

IMPL_ARRAY_BUF(Type)

TypeRegistry type_registry_new(void) {
    ArrayBuf(Type) types = array_buf_new(Type)();
    array_buf_push(Type)(
        &types,
        (Type){ .kind = TYPE_BOOL, .pretty_name = STRING_LITERAL("Bool") }
    );
    return (TypeRegistry){ ._types = types };
}

void type_registry_free(TypeRegistry* type_registry) {
    array_buf_free(Type)(&type_registry->_types);
}

Type get_type(TypeRegistry registry, TypeId type) {
    return registry._types.data[type];
}

TypeId register_type(TypeRegistry* registry, Type type) {
    TypeId id = registry->_types.len;
    array_buf_push(Type)(&registry->_types, type);
    if (type.kind == TYPE_FUNCTION) {
        hash_map_insert(FunctionType, TypeId)(&registry->_function_types, type.as.function, id);
    }
    return id;
}

TypeId get_or_register_function_type(
    TypeRegistry* registry,
    FunctionType type,
    AstStorage* storage
) {
    TypeId const* id = hash_map_get(FunctionType, TypeId)(registry->_function_types, type);
    if (id) {
        return *id;
    }
    Type input = get_type(*registry, type.input);
    Type output = get_type(*registry, type.output);
    StringBuf pretty = format(
        "fn %.*s -> %.*s",
        (int)input.pretty_name.len, input.pretty_name.data,
        (int)output.pretty_name.len, output.pretty_name.data
    );
    ast_store(storage, pretty.data);
    return register_type(registry, (Type){
        .kind = TYPE_FUNCTION,
        .as.function = type,
        .pretty_name = {
            .data = pretty.data,
            .len = pretty.len,
        },
    });
}
