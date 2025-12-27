#pragma once

#include "collections/array.h"
#include "collections/hash_map.h"
#include "ast/memory.h"

typedef usize TypeId;
#define TYPE_INVALID ((TypeId)(-1))

typedef struct FunctionType {
    TypeId input;
    TypeId output;
} FunctionType;

void hash(FunctionType)(Hasher* hasher, FunctionType);
bool eq(FunctionType)(FunctionType, FunctionType);
DECL_HASH_MAP(FunctionType, TypeId)

typedef enum TypeKind {
    TYPE_BOOL,
    TYPE_UINT,
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_FUNCTION,
} TypeKind;

typedef struct Type {
    TypeKind kind;
    union {
        FunctionType function;
    } as;
    String pretty_name;
} Type;

DECL_ARRAY_BUF(Type);

typedef struct TypeRegistry {
    ArrayBuf(Type) _types;
    HashMap(FunctionType, TypeId) _function_types;
} TypeRegistry;

TypeRegistry type_registry_new(void);
void type_registry_free(TypeRegistry* type_registry);

Type get_type(TypeRegistry registry, TypeId type);
TypeId register_type(TypeRegistry* registry, Type type);
TypeId get_or_register_function_type(
    TypeRegistry* registry,
    FunctionType type,
    AstStorage* storage
);
