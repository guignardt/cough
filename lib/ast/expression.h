#pragma once

#include "util/collections/array.h"
#include "ast/type.h"
#include "ast/binding_id.h"

typedef struct Identifier {
    String string;
    Range range;
} Identifier;

typedef enum TypeNameKind {
    TYPE_NAME_IDENTIFIER
} TypeNameKind;

typedef struct TypeName {
    TypeNameKind kind;
    union {
        Identifier identifier;
    } as;
    Range range;
} TypeName;

typedef usize ExpressionId;

typedef struct ConstantDef {
    Identifier name;
    bool explicitly_typed;
    TypeName type_name;
    TypeId type;
    ExpressionId value;
    BindingId binding;
    Range range;
} ConstantDef;

DECL_ARRAY_BUF(ConstantDef);

typedef struct VariableDef {
    Identifier name;
    bool explicitly_typed;
    TypeName type_name;
    TypeId type;
    BindingId binding;
    Range range;
} VariableDef;

typedef enum PatternKind {
    PATTERN_VARIABLE,
} PatternKind;

typedef struct Pattern {
    PatternKind kind;
    union {
        VariableDef variable;
    } as;
    bool explicitly_typed;
    TypeName type_name;
    TypeId type;
    Range range;
    ScopeId scope;
} Pattern;

typedef struct Function {
    Pattern input;
    bool explicit_output_type;
    TypeName output_type_name;
    TypeId output_type;
    Range signature_range;
    ScopeId output_scope;
    ExpressionId output;
    usize function_id;
    usize variable_space; // in words
} Function;

typedef struct VariableRef {
    Identifier name;
    BindingId binding;
} VariableRef;

typedef enum UnaryOperator {
    OPERATION_NOT,

    OPERATION_NEG,
    OPERATION_NEG_INT = OPERATION_NEG,
    OPERATION_NEG_FLOAT,
} UnaryOperator;

typedef struct UnaryOperation {
    UnaryOperator operator;
    ExpressionId operand;
} UnaryOperation;

typedef enum BinaryOperator {
    OPERATION_FUNCTION_CALL,

    OPERATION_OR,
    OPERATION_AND,
    OPERATION_XOR,

    OPERATION_ADD,
    OPERATION_SUB,
    OPERATION_MUL,
    OPERATION_DIV,

    OPERATION_ADD_UINT = OPERATION_ADD,
    OPERATION_SUB_UINT = OPERATION_SUB,
    OPERATION_MUL_UINT = OPERATION_MUL,
    OPERATION_DIV_UINT = OPERATION_DIV,

    OPERATION_ADD_INT,
    OPERATION_SUB_INT,
    OPERATION_MUL_INT,
    OPERATION_DIV_INT,

    OPERATION_ADD_FLOAT,
    OPERATION_SUB_FLOAT,
    OPERATION_MUL_FLOAT,
    OPERATION_DIV_FLOAT,
} BinaryOperator;

// first is computed (and pushed) before second
// for function calls, we want the location to be on top, so
// the first is the argument and the second is the location
typedef struct BinaryOperation {
    BinaryOperator operator;
    ExpressionId first;
    ExpressionId second;
} BinaryOperation;

typedef enum ExpressionKind {
    EXPRESSION_VARIABLE,
    EXPRESSION_FUNCTION,
    EXPRESSION_LITERAL_BOOL,
    EXPRESSION_LITERAL_UINT,
    EXPRESSION_LITERAL_INT,
    EXPRESSION_LITERAL_FLOAT,
    EXPRESSION_UNARY_OPERATION,
    EXPRESSION_BINARY_OPERATION,
} ExpressionKind;

typedef struct Expression {
    ExpressionKind kind;
    union {
        VariableRef variable;
        Function function;
        bool literal_bool;
        u64 literal_uint;
        i64 literal_int;
        f64 literal_float;
        UnaryOperation unary_operation;
        BinaryOperation binary_operation;
    } as;
    Range range;
    TypeId type;
} Expression;

DECL_ARRAY_BUF(Expression);
