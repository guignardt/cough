#pragma once

#include "util/collections/array.h"
#include "ast/type.h"
#include "ast/binding.h"
#include "ast/expression.h"
#include "ast/memory.h"

typedef struct AstData {
    String source;
    TypeRegistry types;
    BindingRegistry bindings;
    ArrayBuf(Expression) expressions;
    ArrayBuf(usize) functions;  // expression IDs, indexed by `Function::function_id`
    AstStorage storage;
} AstData;

// creates new data with default type bindings
// in a 'root' scope
AstData ast_data_new(String source);
void ast_data_free(AstData* data);

typedef struct Module {
    ScopeId global_scope;
    ArrayBuf(ConstantDef) global_constants;
} Module;
