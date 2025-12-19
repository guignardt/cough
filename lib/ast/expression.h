#pragma once

#include "collections/array.h"
#include "ast/type.h"
#include "ast/binding_id.h"
#include "emitter/emitter.h"

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
} ConstantDef;

DECL_ARRAY_BUF(ConstantDef);

typedef struct VariableDef {
    Identifier name;
    bool explicitly_typed;
    TypeName type_name;
    TypeId type;
    BindingId binding;
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
    ScopeId output_scope;
    ExpressionId output;
    SymbolIndex symbol;
    usize function_id;
    usize variable_space; // in words
} Function;

typedef struct VariableRef {
    Identifier name;
    BindingId binding;
} VariableRef;

typedef enum UnaryOperator {
    OPERATION_NOT,
} UnaryOperator;

typedef struct UnaryOperation {
    UnaryOperator operator;
    ExpressionId operand;
} UnaryOperation;

typedef enum BinaryOperator {
    OPERATION_FUNCTION_CALL,
    OPERATION_OR,
    OPERATION_AND,
} BinaryOperator;

typedef struct BinaryOperation {
    BinaryOperator operator;
    ExpressionId operand_left;
    ExpressionId operand_right;
} BinaryOperation;

typedef enum ExpressionKind {
    EXPRESSION_VARIABLE,
    EXPRESSION_FUNCTION,
    EXPRESSION_LITERAL_BOOL,
    EXPRESSION_UNARY_OPERATION,
    EXPRESSION_BINARY_OPERATION,
} ExpressionKind;

typedef struct Expression {
    ExpressionKind kind;
    union {
        VariableRef variable;
        Function function;
        bool literal_bool;
        UnaryOperation unary_operation;
        BinaryOperation binary_operation;
    } as;
    Range range;
    TypeId type;
} Expression;

DECL_ARRAY_BUF(Expression);
