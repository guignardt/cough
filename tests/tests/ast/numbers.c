#include <math.h>

#include "tests/common.h"

void test_uint(char const* text, u64 value);
void test_int(char const* text, i64 value);
void test_uint_overflow(char const* text);
void test_int_overflow(char const* text);
void test_float(char const* text, f64 value);

int main(int argc, char const* argv[]) {
    test_uint("0", 0);
    test_uint("1", 1);
    test_uint("42", 42);
    test_uint("13765", 13765);
    test_uint("18446744073709551615", 18446744073709551615ull);
    test_uint_overflow("18446744073709551616");
    test_uint_overflow("3647273479304003904280");

    test_int("+0", 0);
    test_int("-0", 0);
    test_int("+1", 1);
    test_int("-1", -1);
    test_int("-42", -42);
    test_int("+13765", 13765);
    test_int("+9223372036854775807", 9223372036854775807ll);
    test_int("-9223372036854775807", -9223372036854775807ll);
    test_int("-9223372036854775808", 1ull << 63);
    test_int_overflow("+9223372036854775808");
    test_int_overflow("-9223372036854775809");

    test_float("0.0", 0.0);
    test_float("+0.0", 0.0);
    test_float("-0.0", -0.0);
    test_float("1.0", 1.0);
    test_float("-1.0", -1.0);
    test_float("1.5", 1.5);
    test_float("1.1", 1.1);
    test_float("3.1415926535897932385", 3.1415926535897932385);
    test_float("2.7182818284590452354", 2.7182818284590452354);
    test_float("11235.8132134", 11235.8132134);
    test_float("1e5", 1e5);
    test_float("3.88e-6", 3.88e-6);
    test_float("3.88e-6", 3.88e-6);
    test_float("1.7976931348623157e+308", 1.7976931348623157e+308);
    test_float("1.9e+308", INFINITY);
    test_float("1e+1000", INFINITY);
    test_float("2.23e-308", 2.23e-308);
    test_float("1e-1000", 0.0);
    test_float("4.94e-324", 4.84e-324);
    
    return 0;
}

int parse_num(char const* text, ExpressionId* dst, AstData* dst_data) {
    TestReporter reporter = test_reporter_new();
    String source_text = (String){ .data = text, .len = strlen(text) };
    TokenStream tokens;
    tokenize(source_text, &reporter.base, &tokens);
    assert(reporter.error_codes.len == 0);
    AstData data = ast_data_default(source_text);
    parse_expression(tokens, &data, &reporter.base, dst);
    if (reporter.error_codes.len > 0) {
        assert(reporter.error_codes.len == 1);
        assert(reporter.error_codes.data[0] == CE_INTEGER_LITERAL_OVERFLOWED);
    }
    test_reporter_free(&reporter);
    *dst_data = data;
    return reporter.error_codes.len;
}

void test_uint(char const* text, u64 value) {
    ExpressionId id;
    AstData data;
    assert(!parse_num(text, &id, &data));
    Expression expr = data.expressions.data[id];
    assert(expr.kind == EXPRESSION_LITERAL_UINT);
    assert(expr.as.literal_uint == value);
    ast_data_free(&data);
}

void test_int(char const* text, i64 value) {
    ExpressionId id;
    AstData data;
    assert(!parse_num(text, &id, &data));
    Expression expr = data.expressions.data[id];
    assert(expr.kind == EXPRESSION_LITERAL_INT);
    assert(expr.as.literal_uint == value);
    ast_data_free(&data);
}

void test_uint_overflow(char const* text) {
    ExpressionId id;
    AstData data;
    assert(parse_num(text, &id, &data));
    Expression expr = data.expressions.data[id];
    assert(expr.kind == EXPRESSION_LITERAL_UINT);
    ast_data_free(&data);
}

void test_int_overflow(char const* text) {
    ExpressionId id;
    AstData data;
    assert(parse_num(text, &id, &data));
    Expression expr = data.expressions.data[id];
    assert(expr.kind == EXPRESSION_LITERAL_INT);
    ast_data_free(&data);
}

void test_float(char const* text, f64 value) {
    ExpressionId id;
    AstData data;
    assert(!parse_num(text, &id, &data));
    Expression expr = data.expressions.data[id];
    assert(expr.kind == EXPRESSION_LITERAL_FLOAT);
    typedef union Cast {
        f64 value;
        u64 bits;
    } Cast;
    Cast computed = { .value = expr.as.literal_float };
    Cast expected = { .value = value };
    assert(computed.bits == expected.bits);
    ast_data_free(&data);
}
