#include "tests/common.h"

int main(int argc, char const** argv) {
    Ast ast = source_to_ast(STRING_LITERAL(
        "id :: fn a: Bool -> Bool => a;"
        "logical :: fn b: Bool -> Bool => !b & id(true) | false;"
    ));
    assert(ast.root.global_constants.len == 2);
    ConstantDef logical_def = ast.root.global_constants.data[1];
    assert(eq(String)(logical_def.name.string, STRING_LITERAL("logical")));
    Expression logical_expr = ast.expressions.data[logical_def.value];
    assert(logical_expr.kind == EXPRESSION_FUNCTION);
    Function logical_fn = logical_expr.as.function;

    Expression or = ast.expressions.data[logical_fn.output];
    assert(or.kind == EXPRESSION_BINARY_OPERATION);
    assert(or.as.binary_operation.operator == OPERATION_OR);
    
    Expression and = ast.expressions.data[or.as.binary_operation.first];
    assert(and.kind == EXPRESSION_BINARY_OPERATION);
    assert(and.as.binary_operation.operator = OPERATION_AND);
    
    Expression not = ast.expressions.data[and.as.binary_operation.first];
    assert(not.kind == EXPRESSION_UNARY_OPERATION);
    assert(not.as.unary_operation.operator == OPERATION_NOT);
    
    Expression b_ref = ast.expressions.data[not.as.unary_operation.operand];
    assert(b_ref.kind == EXPRESSION_VARIABLE);
    assert(eq(String)(b_ref.as.variable.name.string, STRING_LITERAL("b")));

    Expression call = ast.expressions.data[and.as.binary_operation.second];
    assert(call.kind == EXPRESSION_BINARY_OPERATION);
    assert(call.as.binary_operation.operator == OPERATION_FUNCTION_CALL);
    
    Expression id = ast.expressions.data[call.as.binary_operation.second];
    assert(id.kind == EXPRESSION_VARIABLE);
    assert(eq(String)(id.as.variable.name.string, STRING_LITERAL("id")));

    Expression tr = ast.expressions.data[call.as.binary_operation.first];
    assert(tr.kind == EXPRESSION_LITERAL_BOOL);
    assert(tr.as.literal_bool == true);

    Expression fa = ast.expressions.data[or.as.binary_operation.second];
    assert(fa.kind == EXPRESSION_LITERAL_BOOL);
    assert(fa.as.literal_bool == false);

    return 0;
}
