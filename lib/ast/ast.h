#pragma once

#include "util/collections/array.h"
#include "ast/type.h"
#include "ast/binding.h"
#include "ast/expression.h"
#include "ast/memory.h"

typedef struct Module {
    ScopeId global_scope;
    ArrayBuf(ConstantDef) global_constants;
} Module;

typedef struct Ast {
    String source;
    TypeRegistry types;
    BindingRegistry bindings;
    ArrayBuf(Expression) expressions;
    ArrayBuf(usize) functions;  // expression IDs
    Module root;
    AstStorage storage;
} Ast;

void ast_free(Ast* ast);
