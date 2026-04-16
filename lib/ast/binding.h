#pragma once

#include "ast/binding_id.h"
#include "ast/type.h"
#include "ast/expression.h"

typedef struct TypeBinding {
    String name;
    TypeId type;
    Range source_definition;
} TypeBinding;

typedef enum ValueStoreKind {
    VALUE_STORE_CONSTANT,
    VALUE_STORE_VARIABLE,
} ValueStoreKind;

typedef struct VariableStore {
    usize index;
    usize size;
    usize function_id;
} VariableStore;

typedef struct ValueStore {
    ValueStoreKind kind;
    union {
        ExpressionId constant;
        VariableStore variable;
    } as;
} ValueStore;

typedef struct ValueBinding {
    String name;
    TypeId type;
    ValueStore store;
    Range source_definition;
} ValueBinding;

typedef struct ScopeLocation {
    ScopeId scope_id;
    usize _pos;
} ScopeLocation;

typedef enum BindingKind {
    BINDING_TYPE,
    BINDING_VALUE
} BindingKind;

typedef struct Binding {
    BindingId id;
    ScopeLocation location;
    BindingKind kind;
    union {
        TypeBinding type;
        ValueBinding value;
    } as;
} Binding;

typedef struct BindingMut {
    BindingId id;
    ScopeLocation location;
    BindingKind kind;
    union {
        TypeBinding* type;
        ValueBinding* value;
    } as;
} BindingMut;

typedef struct Scope {
    ScopeLocation _parent; // -1,-1 if no parent
    ArrayBuf(BindingId) _unordered_bindings;
    ArrayBuf(BindingId) _sequential_bindings;
} Scope;
DECL_ARRAY_BUF(Scope);

typedef struct TypeBindingEntry {
    TypeBinding _data;
    ScopeLocation _location;
} TypeBindingEntry;
DECL_ARRAY_BUF(TypeBindingEntry)

typedef struct ValueBindingEntry {
    ValueBinding _data;
    ScopeLocation _location;
} ValueBindingEntry;
DECL_ARRAY_BUF(ValueBindingEntry)

typedef struct BindingRegistry {
    ArrayBuf(TypeBindingEntry) _type_bindings;
    ArrayBuf(ValueBindingEntry) _value_bindings;
    ArrayBuf(Scope) _scopes;
} BindingRegistry;

BindingRegistry binding_registry_new(void);
void binding_registry_free(BindingRegistry* registry);

bool find_binding(
    BindingRegistry registry,
    ScopeLocation location,
    String name,
    BindingId* dst
);

ScopeLocation scope_new(BindingRegistry* registry, ScopeLocation parent);

ScopeLocation scope_end_location(BindingRegistry registry, ScopeId scope);

Binding get_binding(BindingRegistry registry, BindingId id);
BindingMut get_binding_mut(BindingRegistry* registry, BindingId id);

bool insert_type_binding(
    BindingRegistry* registry,
    ScopeId scope,
    TypeBinding binding,
    BindingMut* dst
);
bool insert_value_binding(
    BindingRegistry* registry,
    ScopeId scope,
    ValueBinding binding,
    BindingMut* dst
);
bool push_value_binding(
    BindingRegistry* registry,
    ScopeLocation* scope,
    ValueBinding binding,
    BindingMut* dst
);
