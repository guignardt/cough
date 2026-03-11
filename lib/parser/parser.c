#include <ctype.h>
#include <math.h>

#include "parser/parser.h"

typedef struct Parser {
    String source;
    TokenStream tokens;
    usize pos;
    Reporter* reporter;
    ArrayBuf(Expression) expressions;
    ArrayBuf(usize) functions;
    AstStorage storage;
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
    report_start(parser->reporter, SEVERITY_ERROR, CE_UNCLOSED_PAREN);
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
    usize id = parser->expressions.len;
    array_buf_push(Expression)(&parser->expressions, expression);
    return id;
}

typedef enum Precedence {
    PRECEDENCE_ANY,
    PRECEDENCE_BITWISE_BINARY,
    PRECEDENCE_PREFIX,
    PRECEDENCE_FUNCTION_CALL,
    PRECEDENCE_PRIMARY,
} Precedence;

static Result parse_module(Parser* parser, Module* dst);
static Result parse_constant(Parser* parser, ConstantDef* dst);
// range may be NULL
static Result parse_expression(Parser* parser, ExpressionId* dst, Range* range);
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
static void parse_number(Parser* parser, Expression* dst);
// head token must be `fn`
static Result parse_function(Parser* parser, Function* dst, Range* range);
static Result parse_pattern(Parser* parser, Pattern* dst);
static Result parse_type_name(Parser* parser, TypeName* dst);
static Result parse_identifier(Parser* parser, Identifier* dst);

bool parse(
    TokenStream tokens,
    Reporter* reporter,
    Ast* dst
) {
    Parser parser = {
        .source = tokens.source,
        .tokens = tokens,
        .pos = 0,
        .reporter = reporter,
        .expressions = array_buf_new(Expression)(),
        .functions = array_buf_new(usize)(),
        .storage = ast_storage_new(),
    };
    Module module;
    parse_module(&parser, &module);
    ast_store(&parser.storage, parser.expressions.data);
    ast_store(&parser.storage, parser.functions.data);
    *dst = (Ast){
        .bindings = binding_registry_new(),
        .types = type_registry_new(),
        .expressions = parser.expressions,
        .functions = parser.functions,
        .root = module,
        .storage = parser.storage,
    };
    return reporter_error_count(reporter) == 0;
}

static Result parse_module(Parser* parser, Module* dst) {
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
        .global_constants = global_constants
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
    if (parse_expression(parser, &value, NULL) != OK) {
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

// represents an `integer`.`fractional` * 2^`exponent` numeric value.
typedef struct Magnitude {
    u64 integer;
    u64 fractional;
    u64 exponent;
} Magnitude;

// digits specified in big-endian
// returns if product if nonzero
static bool mul2(u8* digits, usize len, u8 base, u8* p_carry) {
    u8 carry = 0;
    bool nonzero = false;
    for (isize i = len - 1; i >= 0; i--) {
        u8 d = digits[i];
        u8 r = d * 2 + carry;
        digits[i] = r % base;
        if (digits[i] != 0) nonzero = true;
        carry = r / base;
    }
    *p_carry = carry;
    return nonzero;
}

// digits specified in big-endian
// returns remainder
static bool div2(u8* digits, usize len, u8 base, u8* p_remainder) {
    u8 remainder = 0;
    bool nonzero = false;
    for (usize i = 0; i < len; i++) {
        u8 d = digits[i];
        u8 a = (base * remainder + d);
        digits[i] = a / 2;
        if (digits[i] != 0) nonzero = true;
        remainder = a % 2;
    }
    *p_remainder = remainder;
    return nonzero;
}

DECL_ARRAY_BUF(u8);
IMPL_ARRAY_BUF(u8);

static ArrayBuf(u8) read_digits(char const* text) {
    ArrayBuf(u8) buf = array_buf_new(u8)();
    char c;
    while (c = *(text++), isdigit(c)) {
        array_buf_push(u8)(&buf, c - '0');
    }
    return buf;
}

static Expression build_integer_expression(Parser* parser, i8 sign, u64 magnitude, Range range) {
    switch (sign){
    case 1:
        // check if `magnitude` can be converted to an `i64`
        if (magnitude & (1ull << 63)) {
            integer_literal_overflow(parser, range, true);
            magnitude = 0;
        }
        return (Expression){
            .kind = EXPRESSION_LITERAL_INT,
            .as.literal_int = (i64)magnitude,
            .range = range,
            .type = TYPE_INT,
        };

    case -1:;
        // compute `-magnitude`
        i64 value;
        if (magnitude == (1ull << 63)) {
            // treat this case separately to avoid signed overflow (UB)
            value = 1ull << 63;
        } else if (magnitude & (1ull << 63)) {
            integer_literal_overflow(parser, range, true);
            value = 0;
        } else {
            value = -(i64)magnitude;
        }
        return (Expression){
            .kind = EXPRESSION_LITERAL_INT,
            .as.literal_int = value,
            .range = range,
            .type = TYPE_INT,
        };
    
    // unsigned case
    default:
        return (Expression){
            .kind = EXPRESSION_LITERAL_UINT,
            .as.literal_uint = magnitude,
            .range = range,
            .type = TYPE_UINT,
        };
    }
}

static Expression build_float_expression(i8 sign, u64 magnitude, i64 shift, Range range) {
    // encode as an IEEE 754 binary64.
    // see https://en.wikipedia.org/wiki/IEEE_754-1985
    
    if (magnitude == 0) {
        f64 value = (sign == -1) ? (-0.0) : (+0.0);
        return (Expression){
            .kind = EXPRESSION_LITERAL_FLOAT,
            .as.literal_float = value,
            .range = range,
            .type = TYPE_FLOAT
        };
    }

    // shift to have `number` = 1.`fractional` * 2^`exponent`.
    u64 leading_zeros = __builtin_clzg(magnitude);
    u64 fraction = magnitude << (leading_zeros + 1);
    i64 exponent = shift + 63 - leading_zeros;

    #define EXPONENT_BIAS (1023)
    #define EXPONENT_NORMAL_MIN (-1022)
    #define EXPONENT_NORMAL_MAX (1023)

    if (exponent < EXPONENT_NORMAL_MIN) {
        // denormalized number (except in case of rounding)
        u64 fraction_shift = EXPONENT_NORMAL_MIN - exponent;
        fraction >>= fraction_shift;
        fraction &= (1ull << 63);
        exponent = EXPONENT_NORMAL_MIN - 1;
    }

    #define FRACTION_WIDTH (52)

    u64 fraction_bits = fraction >> (63 - FRACTION_WIDTH);
    u64 fraction_rounding_bit = fraction_bits & 1;
    fraction_bits >>= 1;
    // this has the effect of rounding
    fraction_bits += fraction_rounding_bit;
    // if rounding overflows
    if (fraction_bits & (1ull << FRACTION_WIDTH)) {
        // this means fraction is was 11...1 before rounding
        // - if the number is normalized, the number rounds to a power of two one order of
        // magnitude higher. So we increment the exponent and set `fraction` to zero.
        // - if the number is denormalized, the number rounds to the smallest normalized power of
        // two. So we also increment the exponent and set `fraction` to zero.
        exponent++;
        fraction_bits = 0;
    }

    if (exponent > EXPONENT_NORMAL_MAX) {
        // round to infinity
        f64 value = (sign == -1) ? -INFINITY : INFINITY;
        return (Expression){
            .kind = EXPRESSION_LITERAL_FLOAT,
            .as.literal_float = value,
            .range = range,
            .type = TYPE_FLOAT
        };
    }

    u64 exponent_bits = (exponent + EXPONENT_BIAS) << FRACTION_WIDTH;
    u64 sign_bits = (sign == -1) ? (1ull << 63) : 0;
    union {
        u64 bits;
        f64 value;
    } cast = { .bits = sign_bits | exponent_bits | fraction_bits };
    return (Expression){
        .kind = EXPRESSION_LITERAL_FLOAT,
        .as.literal_float = cast.value,
        .range = range,
        .type = TYPE_FLOAT,
    };
}

static void parse_number(Parser* parser, Expression* dst) {
    // TOKEN_NUMBER
    Token token = parser->tokens.tokens.data[(parser->pos)++];
    // nonempty
    Range range = token_range(parser->tokens, token);
    String text = string_slice(parser->source, range);

    i8 sign;
    // position within range
    size_t pos;
    switch (text.data[0]) {
    case '+': sign = 1; pos = 1; break;
    case '-': sign = -1; pos = 1; break;
    default: sign = 0; pos = 0; break;
    }

    u64 magnitude = 0;
    i64 shift = 0;

    // parse the integer part
    ArrayBuf(u8) integer_buf = read_digits(text.data + pos);
    pos += integer_buf.len;
    u64 mask = 1;
    while (true) {
        u8 bit;
        bool nonzero = div2(integer_buf.data, integer_buf.len, 10, &bit);
        if (mask == 0) {
            magnitude >>= 1;
            magnitude |= (-bit) & (1ull << 63);
            shift += 1;
        } else {
            magnitude |= (-bit) & mask;
            mask <<= 1;
        }
        if (!nonzero) break;
    }
    array_buf_free(u8)(&integer_buf);

    if (text.data[pos] != '.') {
        // we have an integer literal
        if (shift > 0) {
            // we have overflowed
            integer_literal_overflow(parser, range, sign != 0);
            // we do continue anyway: we emit an integer of the appropriate type,
            // which helps type analysis. However, as we errored, the numeric value won't actually
            // be used.
            // we don't have to report `ERROR` as all errors are handled here and the parser is
            // in a valid state.
            magnitude = 0; // this avoids other (signed) overflow error
        }
        *dst = build_integer_expression(parser, sign, magnitude, range);
        return;
    }

    // we have a floating-point literal
    pos++;

    // parse the fractional part
    ArrayBuf(u8) fractional_buf = read_digits(text.data + pos);
    pos += fractional_buf.len;
    while (true) {
        if (magnitude & (1ull << 63)) break;
        u8 bit;
        bool nonzero = mul2(fractional_buf.data, fractional_buf.len, 10, &bit);
        magnitude = (magnitude << 1) | bit;
        shift -= 1;
        if (!nonzero) break;
    }
    array_buf_free(u8)(&fractional_buf);

    *dst = build_float_expression(sign, magnitude, shift, range);
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
    if (parse_expression(parser, &output, NULL) != OK) {
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

static Result parse_expression(Parser* parser, ExpressionId* dst, Range* dst_range) {
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
        operator_precedence = PRECEDENCE_BITWISE_BINARY;
        new_parser_pos++;
        break;
    case TOKEN_AMPERSAND:
        operator = OPERATION_AND;
        operator_precedence = PRECEDENCE_BITWISE_BINARY;
        new_parser_pos++;
        break;
    case TOKEN_HAT:
        operator = OPERATION_XOR,
        operator_precedence = PRECEDENCE_BITWISE_BINARY;
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
    expr = box_expression(parser, (Expression){
        .kind = EXPRESSION_BINARY_OPERATION,
        .as.binary_operation = {
            .operator = operator,
            .operand_left = lhs,
            .operand_right = rhs,
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
        parse_expression(parser, dst, NULL);
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

    case TOKEN_NUMBER:
        parse_number(parser, &expr);
        break;

    default:
        // FIXME: report error
        return ERROR;
    }

    ExpressionId id = parser->expressions.len;
    if (expr.kind == EXPRESSION_FUNCTION) {
        expr.as.function.function_id = parser->functions.len;
        array_buf_push(usize)(&parser->functions, id);
    }
    array_buf_push(Expression)(&parser->expressions, expr);

    *dst = id;
    *dst_range = expr.range;

    return OK;
}
