#include "tests/common.h"

int main(int argc, char const* argv[]) {
    String text = STRING_LITERAL("if x => y else => y ^ 1");
    
    SourceText source = source_text_new("<test>", text.data);
    CrashingReporter reporter = crashing_reporter_new(source);

    TokenStream tokens;
    tokenize(text, &reporter.base, &tokens);

    AstData data = ast_data_default(text);
    ValueBinding x_binding = {
        .name = STRING_LITERAL("x"),
        .type = TYPE_BOOL,
    };
    BindingMut slot;
    assert(insert_value_binding(&data.bindings, ROOT_SCOPE_ID, x_binding, &slot));
    BindingId x_binding_id = slot.id;
    ValueBinding y_binding = {
        .name = STRING_LITERAL("y"),
        .type = TYPE_UINT,
    };
    assert(insert_value_binding(&data.bindings, ROOT_SCOPE_ID, y_binding, &slot));
    BindingId y_binding_id = slot.id;

    ExpressionId id;
    parse_expression(tokens, &data, &reporter.base, &id);
    Expression* expr = &data.expressions.data[id];
    analyze_expression(expr, &data, &reporter.base);

    assert(expr->kind == EXPRESSION_CONDITIONAL);
    
    Expression condition = data.expressions.data[expr->as.conditional.condition];
    assert(condition.kind == EXPRESSION_VARIABLE);
    assert(eq(String)(condition.as.variable.name.string, STRING_LITERAL("x")));
    assert(condition.as.variable.binding == x_binding_id);

    Expression if_true = data.expressions.data[expr->as.conditional.if_true];
    assert(if_true.kind == EXPRESSION_VARIABLE);
    assert(eq(String)(if_true.as.variable.name.string, STRING_LITERAL("y")));
    assert(if_true.as.variable.binding == y_binding_id);

    assert(expr->as.conditional.has_else);
    Expression if_false = data.expressions.data[expr->as.conditional.if_false];
    assert(if_false.kind == EXPRESSION_BINARY_OPERATION);

    Expression lhs = data.expressions.data[if_false.as.binary_operation.first];
    assert(lhs.kind == EXPRESSION_VARIABLE);
    assert(eq(String)(lhs.as.variable.name.string, STRING_LITERAL("y")));
    assert(lhs.as.variable.binding == y_binding_id);

    Expression rhs = data.expressions.data[if_false.as.binary_operation.second];
    assert(rhs.kind == EXPRESSION_LITERAL_UINT);
    assert(rhs.as.literal_uint == 1);

    assert(expr->type == TYPE_UINT);
    assert(condition.type == TYPE_BOOL);
    assert(if_true.type == TYPE_UINT);
    assert(if_false.type == TYPE_UINT);
    assert(lhs.type == TYPE_UINT);
    assert(rhs.type == TYPE_UINT);

    return 0;
}
