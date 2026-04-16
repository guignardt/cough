#include <ctype.h>
#include <math.h>

#include "parser/parser.h"
#include "float_parser_data.h"

typedef struct Parser {
    String source;
    TokenStream tokens;
    usize pos;
    Reporter* reporter;
    AstData data;
} Parser;

static void unexpected_token(Parser* parser, TokenKind expected) {
    report_start(parser->reporter, SEVERITY_ERROR, CE_UNEXPECTED_TOKEN);
    String found;
    bool eof = parser->pos == parser->tokens.tokens.len;
    Range found_range;
    if (eof) {
        found = STRING_LITERAL("end-of-file");
    } else {
        Token token = parser->tokens.tokens.data[parser->pos];
        found_range = token_range(parser->tokens, token);
        found = string_slice(parser->source, found_range);
    }
    StringBuf message = format(
        "unexpected token: expected %s, found %.*s",
        token_kind_description(expected).pretty_description,
        (int)found.len, found.data
    );
    report_message(parser->reporter, message);
    if (!eof) {
        report_source_code(parser->reporter, found_range);
    }
    report_end(parser->reporter);
}

static void unclosed_paren(Parser* parser, usize opening) {
    report_start(parser->reporter, SEVERITY_ERROR, CE_UNCLOSED_PARENTHESIS);
    report_message(parser->reporter, format("expected closing parenthesis"));
    report_source_code(
        parser->reporter,
        token_range(parser->tokens, parser->tokens.tokens.data[parser->pos])
    );
    report_message(parser->reporter, format("note: opening parenthesis"));
    report_source_code(
        parser->reporter,
        token_range(parser->tokens, parser->tokens.tokens.data[opening])
    );
    report_end(parser->reporter);
}

static void integer_literal_overflow(Parser* parser, Range range, bool is_signed) {
    report_start(parser->reporter, SEVERITY_ERROR, CE_INTEGER_LITERAL_OVERFLOWED);
    StringBuf message = (is_signed)
        ? format("signed integer literal overflowed `SINT`")
        : format("unsigned integer literal overflowed `UINT`");
    report_message(parser->reporter, message);
    report_source_code(parser->reporter, range);
    report_end(parser->reporter);
}

static bool parser_match(Parser* parser, TokenKind kind, Token* dst) {
    ArrayBuf(Token) tokens = parser->tokens.tokens;
    if (tokens.len == parser->pos) {
        return false;
    }
    if (tokens.data[parser->pos].kind != kind) {
        return false;
    }
    if (dst) {
        *dst = tokens.data[parser->pos];
    }
    parser->pos++;
    return true;
}

static void parser_skip_until(Parser* parser, TokenKind kind) {
    TokenStream tokens = parser->tokens;
    usize paren_depth = 0;
    while (parser->pos != tokens.tokens.len) {
        Token token = tokens.tokens.data[parser->pos];
        if (paren_depth == 0 && token.kind == kind) {
            return;
        }
        if (token.kind == TOKEN_PAREN_LEFT) {
            paren_depth++;
        }
        if (token.kind == TOKEN_PAREN_RIGHT && paren_depth > 0) {
            paren_depth--;
        }
        parser->pos++;
    }
}

static ExpressionId box_expression(Parser* parser, Expression expression) {
    usize id = parser->data.expressions.len;
    array_buf_push(Expression)(&parser->data.expressions, expression);
    return id;
}

typedef enum Precedence {
    PRECEDENCE_ANY,
    PRECEDENCE_BITWISE_INFIX,
    PRECEDENCE_PREFIX,
    PRECEDENCE_FUNCTION_CALL,
    PRECEDENCE_PRIMARY,
} Precedence;

static Result parse_module2(Parser* parser, Module* dst);
static Result parse_constant(Parser* parser, ConstantDef* dst);
// range may be NULL
static Result parse_expression2(Parser* parser, ExpressionId* dst, Range* range);
// range may be NULL
static Result parse_expression_precedence(Parser* parser, Precedence precedence, ExpressionId* dst, Range* range);
static Result parse_expression_head(Parser* parser, ExpressionId* dst, Range* dst_range);
static Result parse_expression_primary(Parser* parser, ExpressionId* dst, Range* dst_range);
static Result try_parse_expression_continue(
    Parser* parser,
    ExpressionId lhs,
    Range lhs_range,
    Precedence prev_precedence,
    ExpressionId* dst,
    Range* dst_range,
    bool* parsed
);
static void parse_integer(Parser* parser, Expression* dst);
static void parse_float(Parser* parser, Expression* dst);
// head token must be `fn`
static Result parse_function(Parser* parser, Function* dst, Range* range);
static Result parse_conditional(Parser* parser, Conditional* dst, Range* range);
static Result parse_pattern(Parser* parser, Pattern* dst);
static Result parse_type_name(Parser* parser, TypeName* dst);
static Result parse_identifier(Parser* parser, Identifier* dst);

void parse_module(
    TokenStream tokens,
    AstData* data,
    Reporter* reporter,
    Module* dst
) {
    Parser parser = {
        .source = tokens.source,
        .tokens = tokens,
        .pos = 0,
        .reporter = reporter,
        .data = *data,
    };
    parse_module2(&parser, dst);
    ast_store(&parser.data.storage, parser.data.expressions.data);
    ast_store(&parser.data.storage, parser.data.functions.data);
    *data = parser.data;
}

void parse_expression(
    TokenStream tokens,
    AstData* data,
    Reporter* reporter,
    ExpressionId* dst
) {
    Parser parser = {
        .source = tokens.source,
        .tokens = tokens,
        .pos = 0,
        .reporter = reporter,
        .data = *data,
    };
    parse_expression2(&parser, dst, NULL);
    ast_store(&parser.data.storage, parser.data.expressions.data);
    ast_store(&parser.data.storage, parser.data.functions.data);
    *data = parser.data;
}

static Result parse_module2(Parser* parser, Module* dst) {
    TokenStream tokens = parser->tokens;
    ArrayBuf(ConstantDef) global_constants = array_buf_new(ConstantDef)();
    while (parser->pos != tokens.tokens.len) {
        ConstantDef constant;
        if (parse_constant(parser, &constant) != OK) {
            parser_skip_until(parser, TOKEN_SEMICOLON);
            parser->pos++;
            continue;
        }
        array_buf_push(ConstantDef)(&global_constants, constant);
    }
    *dst = (Module){
        .global_constants = global_constants,
        // `global_scope` is set by the analyzer
    };
    return OK;
}

static Result parse_constant(Parser* parser, ConstantDef* dst) {
    Identifier name;
    if (parse_identifier(parser, &name) != OK) {
        return ERROR;
    }
    if (!parser_match(parser, TOKEN_COLON_COLON, NULL)) {
        unexpected_token(parser, TOKEN_COLON_COLON);
        return ERROR;
    }
    ExpressionId value;
    if (parse_expression2(parser, &value, NULL) != OK) {
        return ERROR;
    }
    if (!parser_match(parser, TOKEN_SEMICOLON, NULL)) {
        unexpected_token(parser, TOKEN_SEMICOLON);
        return ERROR;
    }
    Range range = { name.range.start, parser->pos };
    *dst = (ConstantDef){
        .name = name,
        .explicitly_typed = false,
        .type = TYPE_INVALID,
        .value = value,
        .binding = BINDING_ID_INVALID,
        .range = range,
    };
    return OK;
}

// on overflow, returns -1
// text must be nonempty
static int parse_integer_sign_magnitude(String text, usize* p_len, i8* p_sign, u64* p_magnitude) {
    usize pos = 0;

    i8 sign;
    switch (text.data[pos]) {
    case '+': sign = 1; pos++; break;
    case '-': sign = -1; pos++; break;
    default: sign = 0; break;
    }

    u64 magnitude = 0;
    bool overflowed = false;
    for (; pos < text.len; pos++) {
        char c = text.data[pos];
        if (!isdigit(c)) {
            break;
        }
        u64 d = c - '0';
        if (!overflowed) {
            if (__builtin_mul_overflow(magnitude, 10, &magnitude)) {
                overflowed = true;
                break;
            }
            if (__builtin_add_overflow(magnitude, d, &magnitude)) {
                overflowed = true;
                break;
            }
        }
    }

    if (p_len) *p_len = pos;
    if (p_sign) *p_sign = sign;
    if (p_magnitude) *p_magnitude = magnitude;
    return -overflowed;
}

static void parse_integer(Parser* parser, Expression* dst) {
    Token token = parser->tokens.tokens.data[parser->pos++]; // TOKEN_INTEGER
    Range range = token_range(parser->tokens, token);
    String text = string_slice(parser->source, range);

    i8 sign;
    u64 magnitude;
    bool overflowed = parse_integer_sign_magnitude(text, NULL, &sign, &magnitude);

    if (sign == 0) {
        if (overflowed) {
            integer_literal_overflow(parser, range, false);
            magnitude = 0;
        }
        *dst = (Expression){
            .kind = EXPRESSION_LITERAL_UINT,
            .as.literal_uint = magnitude,
            .range = range,
            .type = TYPE_INVALID,
        };
        return;
    }

    i64 value;
    if (sign == 1) {
        if (magnitude >= (1ull << 63)) {
            overflowed = true;
            value = 0;
        } else {
            value = magnitude;
        }
    } else {
        if (magnitude > (1ull << 63)) {
            overflowed = true;
            value = 0;
        } else if (magnitude == (1ull << 63)) {
            value = 1ull << 63;
        } else {
            value = -magnitude;
        }
    }

    if (overflowed) {
        integer_literal_overflow(parser, range, true);
    }

    *dst = (Expression){
        .kind = EXPRESSION_LITERAL_INT,
        .as.literal_int = value,
        .range = range,
        .type = TYPE_INVALID,
    };
}

// Parsing floats is based on the following article:
// <https://research.swtch.com/fp>

// expects EXP10_MIN <= x <= EXP10_MAX
static i64 log2_exp10(i64 x) {
    return (x * 108853) >> 15;
}

typedef unsigned __int128 u128;

// stores a nonnegative number x with
// - floor(x) in bits 2 to 63,
// - frac(x) >= 0.5 in bit 1,
// - frac(x) != 0 && frac(x) != 0.5 in bit 0.
typedef u64 Unrounded;

// computes a * 10**x * 2**-y, i.e., finds b such that
// b * 2**y approximates a * 10**x
// EXP10_MIN <= x <= EXP10_MAX must be satisfied
// a must have its highest bit set
static Unrounded rescale(u64 a, i64 x, i64 y) {
    IntFracExp ex = exp10_table[x - EXP10_MIN];
    u128 aex_hi = (u128)a * (u128)ex.mantissa_int;
    u128 aex_lo = (u128)a * (u128)ex.mantissa_frac;
    u128 aex_int = (aex_hi + (aex_lo >> 64));
    u64 shift = y - ex.exp2 - 2;
    bool sticky = aex_int & (((u128)1 << shift) - 1);
    return (aex_int >> shift) | sticky;
}

// breaks ties to even
static u64 uround(Unrounded u) {
    return (u + 1 + ((u >> 2) & 1)) >> 2;
}

#define EXP2_MIN (-1074)
#define EXP2_MAX (971)
#define EXP2_BIAS (1075)

// encodes a value a * 10**x with 0 <= a < 10**19 and with the specified sign
static f64 build_positive_f64(u64 a, i64 x) {
    if (a == 0 || x < EXP10_MIN) {
        return +0.0;
    }

    if (x > EXP10_MAX) {
        return +INFINITY;
    }

    // Find 2**52 <= b < 2**53 and y with b * 2**y approximating a * 10**x
    // for numbers in the normal range.
    // This may slightly underestimate y, in which case b is slightly too much to the left.
    // This gets fixed later.
    i64 a_lz = __builtin_clzg(a);
    i64 y = 11 - a_lz + log2_exp10(x);
    // For subnormals, we clamp y appropriately.
    if (y < EXP2_MIN) {
        y = EXP2_MIN;
    }

    Unrounded u = rescale(a << a_lz, x, y + a_lz);
    u64 b = uround(u);
    // We now fix b if needed.
    if (b >= (1ull << 53)) {
        b = uround((u >> 1) | (u & 1));
        y++;
    }

    if (y > EXP2_MAX) {
        return +INFINITY;
    }

    union {
        f64 value;
        u64 bits;
    } cast;
    if ((b & (1ull << 52)) == 0) {
        cast.bits = b;
    } else {
        cast.bits = ((y + EXP2_BIAS) << 52) | (b & ((1ull << 52) - 1));
    }
    return cast.value;
}

static void parse_float(Parser* parser, Expression* dst) {
    Token token = parser->tokens.tokens.data[parser->pos++]; // TOKEN_INTEGER
    Range range = token_range(parser->tokens, token);
    String text = string_slice(parser->source, range);

    usize pos = 0;

    i8 sign;
    switch (text.data[pos]) {
    case '+': sign = 1; pos++; break;
    case '-': sign = -1; pos++; break;
    default: sign = 1; break;
    }

    // parse integer and fractional part
    u64 mantissa = 0;
    i64 exp10 = 0;
    bool saw_point = false;
    usize digits_read = 0;
    for (; pos < text.len && digits_read < 19; pos++) {
        char c = text.data[pos];
        if (c == '.') {
            saw_point = true;
            continue;
        }
        if (!isdigit(c)) {
            break;
        }
        // can't overflow since we read at most 19 characters
        mantissa = 10 * mantissa + (c - '0');
        if (saw_point) {
            exp10 -= 1;
        }
        digits_read++;
    }

    // parse exponent
    bool exp10_extreme = false;
    if (pos < text.len && text.data[pos] == 'e') {
        pos++;
        i8 bias_sign;
        u64 bias_magnitude;
        exp10_extreme = parse_integer_sign_magnitude(
            string_slice(text, (Range){ pos, text.len }),
            NULL,
            &bias_sign,
            &bias_magnitude
        );
        exp10_extreme |= bias_magnitude >= INT64_MAX / 2;
        if (!exp10_extreme) {
            exp10 += (bias_sign >= 0) ? bias_magnitude : -bias_magnitude;
        }
    }

    f64 magnitude;
    if (exp10_extreme && mantissa > 0) {
        magnitude = (exp10 > 0) ? +INFINITY : +0.0;
    } else {
        magnitude = build_positive_f64(mantissa, exp10);
    }

    f64 value = (sign >= 0) ? magnitude : -magnitude;
    *dst = (Expression){
        .kind = EXPRESSION_LITERAL_FLOAT,
        .as.literal_float = value,
        .range = range,
        .type = TYPE_INVALID,
    };
}

static Result parse_function(Parser* parser, Function* dst, Range* range) {
    usize start = parser->pos++;
    Pattern input;
    if (parse_pattern(parser, &input) != OK) {
        return ERROR;
    }
    if (!parser_match(parser, TOKEN_ARROW, NULL)) {
        unexpected_token(parser, TOKEN_ARROW);
        return ERROR;
    }
    TypeName output_type;
    if (parse_type_name(parser, &output_type) != OK) {
        return ERROR;
    }
    usize signature_end;
    if (!parser_match(parser, TOKEN_DOUBLE_ARROW, NULL)) {
        unexpected_token(parser, TOKEN_DOUBLE_ARROW);
        return ERROR;
    }
    ExpressionId output;
    if (parse_expression2(parser, &output, NULL) != OK) {
        return ERROR;
    }
    usize end = parser->pos;
    *dst = (Function){
        .input = input,
        .explicit_output_type = true,
        .signature_range = { start, signature_end },
        .output_type_name = output_type,
        .output_type = TYPE_INVALID,
        .output = output,
    };
    *range = token_range_range(parser->tokens, (Range){ start, end });
    return OK;
}

static Result parse_conditional(Parser* parser, Conditional* dst, Range* range) {
    usize start = parser->pos++;
    if (parse_expression2(parser, &dst->condition, NULL) != OK) {
        return ERROR;
    }
    if (parser->tokens.tokens.data[parser->pos].kind != TOKEN_DOUBLE_ARROW) {
        return ERROR;
    }
    parser->pos++;
    if (parse_expression2(parser, &dst->if_true, NULL) != OK) {
        return ERROR;
    }
    dst->has_else = parser->tokens.tokens.data[parser->pos].kind == TOKEN_ELSE;
    if (dst->has_else) {
        parser->pos++;
        if (parser->tokens.tokens.data[parser->pos].kind != TOKEN_DOUBLE_ARROW) {
            return ERROR;
        }
        parser->pos++;
        if (parse_expression2(parser, &dst->if_false, NULL) != OK) {
            return ERROR;
        }
    } else {
        dst->has_else = 0;
    }
    return OK;
}

static Result parse_pattern(Parser* parser, Pattern* dst) {
    usize start = parser->pos;
    Identifier identifier;
    if (parse_identifier(parser, &identifier) != OK) {
        return ERROR;
    }
    if (!parser_match(parser, TOKEN_COLON, NULL)) {
        unexpected_token(parser, TOKEN_COLON);
        return ERROR;
    }
    TypeName type_name;
    if (parse_type_name(parser, &type_name) != OK) {
        return ERROR;
    }
    usize end = parser->pos;
    Range range = token_range_range(parser->tokens, (Range){ start, end });
    *dst = (Pattern){
        .kind = PATTERN_VARIABLE,
        .as.variable = {
            .name = identifier,
            .explicitly_typed = true,
            .type_name = type_name,
            .type = TYPE_INVALID,
            .binding = BINDING_ID_INVALID,
            .range = range,
        },
        .explicitly_typed = true,
        .type_name = type_name,
        .type = TYPE_INVALID,
        .range = range,
    };
    return OK;
}

static Result parse_type_name(Parser* parser, TypeName* dst) {
    Identifier identifier;
    if (parse_identifier(parser, &identifier) != OK) {
        return ERROR;
    }
    *dst = (TypeName){
        .kind = TYPE_NAME_IDENTIFIER,
        .as.identifier = identifier,
        .range = identifier.range,
    };
    return OK;
}

static Result parse_identifier(Parser* parser, Identifier* dst) {
    Token identifier;
    if (!parser_match(parser, TOKEN_IDENTIFIER, &identifier)) {
        unexpected_token(parser, TOKEN_IDENTIFIER);
        return ERROR;
    }
    Range range = token_range(parser->tokens, identifier);
    *dst = (Identifier){
        .string = string_slice(parser->source, range),
        .range = range,
    };
    return OK;
}

static Result parse_expression2(Parser* parser, ExpressionId* dst, Range* dst_range) {
    return parse_expression_precedence(parser, PRECEDENCE_ANY, dst, dst_range);
}

static Result parse_expression_precedence(
    Parser* parser,
    Precedence prev_precedence,
    ExpressionId* dst,
    Range* dst_range
) {
    ExpressionId expr;
    Range range;
    if (parse_expression_head(parser, &expr, &range) != OK) {
        return ERROR;
    }

    bool continued;
    if (
        try_parse_expression_continue(
            parser,
            expr,
            range,
            prev_precedence,
            &expr,
            &range,
            &continued
        ) != OK
    ) {
        return ERROR;
    }

    *dst = expr;
    if (dst_range) {
        *dst_range = range;
    }

    return OK;
}

Result try_parse_expression_continue(
    Parser* parser,
    ExpressionId lhs,
    Range lhs_range,
    Precedence prev_precedence,
    ExpressionId* dst,
    Range* dst_range,
    bool* parsed
) {
    ExpressionId expr;

    if (parser->pos == parser->tokens.tokens.len) {
        *parsed = false;
        return OK;
    }
    BinaryOperator operator;
    Precedence operator_precedence;
    usize new_parser_pos = parser->pos;
    switch (parser->tokens.tokens.data[parser->pos].kind) {
    case TOKEN_PAREN_LEFT:
        operator = OPERATION_FUNCTION_CALL;
        operator_precedence = PRECEDENCE_FUNCTION_CALL;
        break;

    case TOKEN_TUBE:
        operator = OPERATION_OR;
        operator_precedence = PRECEDENCE_BITWISE_INFIX;
        new_parser_pos++;
        break;
    case TOKEN_AMPERSAND:
        operator = OPERATION_AND;
        operator_precedence = PRECEDENCE_BITWISE_INFIX;
        new_parser_pos++;
        break;
    case TOKEN_HAT:
        operator = OPERATION_XOR,
        operator_precedence = PRECEDENCE_BITWISE_INFIX;
        new_parser_pos++;
        break;

    default:
        *parsed = false;
        return OK;
    }

    if (operator_precedence <= prev_precedence) {
        *parsed = false;
        return OK;
    }
    parser->pos = new_parser_pos;

    ExpressionId rhs;
    Range rhs_range;
    if (parse_expression_precedence(parser, operator_precedence, &rhs, &rhs_range) != OK) {
        return ERROR;
    }

    Range range = { lhs_range.start, rhs_range.end };
    if (operator == OPERATION_FUNCTION_CALL) {
        ExpressionId tmp = lhs;
        lhs = rhs;
        rhs = tmp;
    }
    expr = box_expression(parser, (Expression){
        .kind = EXPRESSION_BINARY_OPERATION,
        .as.binary_operation = {
            .operator = operator,
            .first = lhs,
            .second = rhs,
        },
        .range = range,
        .type = TYPE_INVALID
    });
    *parsed = true;
    bool continued;
    if (
        try_parse_expression_continue(
            parser,
            expr,
            range, 
            prev_precedence,
            &expr,
            &range, 
            &continued
        ) != OK
    ) {
        return ERROR;
    }

    *dst = expr;
    *dst_range = range;

    return OK;
}

static Result parse_expression_head(Parser* parser, ExpressionId* dst, Range* dst_range) {
    if (parser->pos == parser->tokens.tokens.len) {
        return ERROR;
    }

    Token head = parser->tokens.tokens.data[parser->pos];
    switch (head.kind) {
    case TOKEN_BANG:;
        parser->pos++;
        ExpressionId operand;
        Range operand_range;
        if (
            parse_expression_precedence(parser, PRECEDENCE_PREFIX, &operand, &operand_range)
            != OK
        ) {
            return ERROR;
        }
        Range range = { head.pos, operand_range.end };
        *dst = box_expression(parser, (Expression){
            .kind = EXPRESSION_UNARY_OPERATION,
            .as.unary_operation = {
                .operator = OPERATION_NOT,
                .operand = operand,
            },
            .range = range,
            .type = TYPE_INVALID,
        });
        *dst_range = range;
        return OK;
    default:
        return parse_expression_primary(parser, dst, dst_range);
    }
}

static Result parse_expression_primary(Parser* parser, ExpressionId* dst, Range* dst_range) {
    if (parser->pos == parser->tokens.tokens.len) {
        return ERROR;
    }

    Token head = parser->tokens.tokens.data[parser->pos];
    Expression expr = { .type = TYPE_INVALID };

    switch (head.kind) {
    case TOKEN_PAREN_LEFT:;
        usize start = parser->pos++;
        parse_expression2(parser, dst, NULL);
        if (!parser_match(parser, TOKEN_PAREN_RIGHT, NULL)) {
            unclosed_paren(parser, start);
            return ERROR;
        }
        *dst_range = token_range_range(parser->tokens, (Range){ start, parser->pos });
        return OK;

    case TOKEN_FN:
        expr.kind = EXPRESSION_FUNCTION;
        if (
            parse_function(parser, &expr.as.function, &expr.range)
            != OK
        ) {
            return ERROR;
        }
        break;

    case TOKEN_IF:
        expr.kind = EXPRESSION_CONDITIONAL;
        if (
            parse_conditional(parser, &expr.as.conditional, &expr.range)
            != OK
        ) {
            return ERROR;
        }
        break;

    case TOKEN_FALSE:
        expr.kind = EXPRESSION_LITERAL_BOOL;
        expr.as.literal_bool = false;
        expr.range = token_range(parser->tokens, head);
        parser->pos++;
        break;
    case TOKEN_TRUE:
        expr.kind = EXPRESSION_LITERAL_BOOL;
        expr.as.literal_bool = true;
        expr.range = token_range(parser->tokens, head);
        parser->pos++;
        break;

    case TOKEN_IDENTIFIER:
        expr.kind = EXPRESSION_VARIABLE;
        Identifier name;
        // can't fail
        parse_identifier(parser, &name);
        expr.as.variable = (VariableRef){
            .name = name,
            .binding = BINDING_ID_INVALID,
        };
        expr.range = name.range;
        break;

    case TOKEN_INTEGER:
        parse_integer(parser, &expr);
        break;

    case TOKEN_FLOAT:
        parse_float(parser, &expr);
        break;

    default:
        // FIXME: report error
        return ERROR;
    }

    ExpressionId id = parser->data.expressions.len;
    if (expr.kind == EXPRESSION_FUNCTION) {
        expr.as.function.function_id = parser->data.functions.len;
        array_buf_push(usize)(&parser->data.functions, id);
    }
    array_buf_push(Expression)(&parser->data.expressions, expr);

    *dst = id;
    *dst_range = expr.range;

    return OK;
}
