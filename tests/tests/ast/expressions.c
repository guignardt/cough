#include "tests/common.h"

int main(int argc, char const** argv) {
    Module module;
    AstData data;
    source_to_module(STRING_LITERAL(
        "id :: fn a: Bool -> Bool => a;"
        "logical :: fn b: Bool -> Bool => !b & id(true) | false;"
    ), &module, &data);
    assert(module.global_constants.len == 2);
    ConstantDef logical_def = module.global_constants.data[1];
    assert(eq(String)(logical_def.name.string, STRING_LITERAL("logical")));
    Expression logical_expr = data.expressions.data[logical_def.value];
    assert(logical_expr.kind == EXPRESSION_FUNCTION);
    Function logical_fn = logical_expr.as.function;

    Expression or = data.expressions.data[logical_fn.output];
    assert(or.kind == EXPRESSION_BINARY_OPERATION);
    assert(or.as.binary_operation.operator == OPERATION_OR);
    
    Expression and = data.expressions.data[or.as.binary_operation.first];
    assert(and.kind == EXPRESSION_BINARY_OPERATION);
    assert(and.as.binary_operation.operator = OPERATION_AND);
    
    Expression not = data.expressions.data[and.as.binary_operation.first];
    assert(not.kind == EXPRESSION_UNARY_OPERATION);
    assert(not.as.unary_operation.operator == OPERATION_NOT);
    
    Expression b_ref = data.expressions.data[not.as.unary_operation.operand];
    assert(b_ref.kind == EXPRESSION_VARIABLE);
    assert(eq(String)(b_ref.as.variable.name.string, STRING_LITERAL("b")));

    Expression call = data.expressions.data[and.as.binary_operation.second];
    assert(call.kind == EXPRESSION_BINARY_OPERATION);
    assert(call.as.binary_operation.operator == OPERATION_FUNCTION_CALL);
    
    Expression id = data.expressions.data[call.as.binary_operation.second];
    assert(id.kind == EXPRESSION_VARIABLE);
    assert(eq(String)(id.as.variable.name.string, STRING_LITERAL("id")));

    Expression tr = data.expressions.data[call.as.binary_operation.first];
    assert(tr.kind == EXPRESSION_LITERAL_BOOL);
    assert(tr.as.literal_bool == true);

    Expression fa = data.expressions.data[or.as.binary_operation.second];
    assert(fa.kind == EXPRESSION_LITERAL_BOOL);
    assert(fa.as.literal_bool == false);

    return 0;
}
