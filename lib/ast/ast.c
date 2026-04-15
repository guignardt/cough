#include "ast/ast.h"

AstData ast_data_new(String source) {
    AstData data = {
        .source = source,
        .types = type_registry_new(),
        .bindings = binding_registry_new(),
        .expressions = array_buf_new(Expression)(),
        .functions = array_buf_new(usize)(),
        .storage = ast_storage_new(),
    };
    TypeBinding bool_binding = {
        .name = STRING_LITERAL("Bool"),
        .type = TYPE_BOOL,
    };
    TypeBinding uint_binding = {
        .name = STRING_LITERAL("UInt"),
        .type = TYPE_UINT,
    };
    TypeBinding int_binding = {
        .name = STRING_LITERAL("Int"),
        .type = TYPE_INT,
    };
    TypeBinding float_binding = {
        .name = STRING_LITERAL("Float"),
        .type = TYPE_FLOAT,
    };
    insert_type_binding(&data.bindings, ROOT_SCOPE_ID, bool_binding, NULL);
    insert_type_binding(&data.bindings, ROOT_SCOPE_ID, uint_binding, NULL);
    insert_type_binding(&data.bindings, ROOT_SCOPE_ID, int_binding, NULL);
    insert_type_binding(&data.bindings, ROOT_SCOPE_ID, float_binding, NULL);
    return data;
}

void ast_data_free(AstData* data) {
    ast_storage_free(&data->storage);
}

