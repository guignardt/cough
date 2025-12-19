#include "tests/common.h"

int main(int argc, char const** argv) {
    Ast ast = source_to_ast(STRING_LITERAL(
        "id :: fn a: Bool -> Bool => a;"
        "logical :: fn b: Bool -> Bool => !b & id(true);"
    ));
    assert(ast.root.global_constants.len == 2);
    ConstantDef logical_def = ast.root.global_constants.data[1];
    assert(eq(String)(logical_def.name.string, STRING_LITERAL("logical")));
    Expression logical_expr = ast.expressions.data[logical_def.value];
    assert(logical_expr.kind == EXPRESSION_FUNCTION);
    Function logical_fn = logical_expr.as.function;
    
    Expression combo = ast.expressions.data[logical_fn.output];
    assert(combo.kind == EXPRESSION_BINARY_OPERATION);
    assert(combo.as.binary_operation.operator = OPERATION_AND);
    
    Expression not = ast.expressions.data[combo.as.binary_operation.operand_left];
    assert(not.kind == EXPRESSION_UNARY_OPERATION);
    assert(not.as.unary_operation.operator == OPERATION_NOT);
    
    Expression b_ref = ast.expressions.data[not.as.unary_operation.operand];
    assert(b_ref.kind == EXPRESSION_VARIABLE);
    assert(eq(String)(b_ref.as.variable.name.string, STRING_LITERAL("b")));

    Expression call = ast.expressions.data[combo.as.binary_operation.operand_right];
    assert(call.kind == EXPRESSION_BINARY_OPERATION);
    assert(call.as.binary_operation.operator == OPERATION_FUNCTION_CALL);
    
    Expression id = ast.expressions.data[call.as.binary_operation.operand_left];
    assert(id.kind == EXPRESSION_VARIABLE);
    assert(eq(String)(id.as.variable.name.string, STRING_LITERAL("id")));

    Expression tr = ast.expressions.data[call.as.binary_operation.operand_right];
    assert(tr.kind == EXPRESSION_LITERAL_BOOL);
    assert(tr.as.literal_bool == true);

    return 0;
}
