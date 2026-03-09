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

// static void parser_simple_error(

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
        if (token.kind == TOKEN_PAREN_RIGHT && paren_depth >= 0) {
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
        if (parse_constant(parser, &constant) != SUCCESS) {
            parser_skip_until(parser, TOKEN_SEMICOLON);
            parser->pos++;
            continue;
        }
        array_buf_push(ConstantDef)(&global_constants, constant);
    }
    *dst = (Module){
        .global_constants = global_constants
    };
    return SUCCESS;
}

static Result parse_constant(Parser* parser, ConstantDef* dst) {
    Identifier name;
    if (parse_identifier(parser, &name) != SUCCESS) {
        return ERROR;
    }
    if (!parser_match(parser, TOKEN_COLON_COLON, NULL)) {
        unexpected_token(parser, TOKEN_COLON_COLON);
        return ERROR;
    }
    ExpressionId value;
    if (parse_expression(parser, &value, NULL) != SUCCESS) {
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
    return SUCCESS;
}

static Result parse_function(Parser* parser, Function* dst, Range* range) {
    usize start =  parser->pos++;
    Pattern input;
    if (parse_pattern(parser, &input) != SUCCESS) {
        return ERROR;
    }
    if (!parser_match(parser, TOKEN_ARROW, NULL)) {
        unexpected_token(parser, TOKEN_ARROW);
        return ERROR;
    }
    TypeName output_type;
    if (parse_type_name(parser, &output_type) != SUCCESS) {
        return ERROR;
    }
    usize signature_end;
    if (!parser_match(parser, TOKEN_DOUBLE_ARROW, NULL)) {
        unexpected_token(parser, TOKEN_DOUBLE_ARROW);
        return ERROR;
    }
    ExpressionId output;
    if (parse_expression(parser, &output, NULL) != SUCCESS) {
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
    return SUCCESS;
}

static Result parse_pattern(Parser* parser, Pattern* dst) {
    usize start = parser->pos;
    Identifier identifier;
    if (parse_identifier(parser, &identifier) != SUCCESS) {
        return ERROR;
    }
    if (!parser_match(parser, TOKEN_COLON, NULL)) {
        unexpected_token(parser, TOKEN_COLON);
        return ERROR;
    }
    TypeName type_name;
    if (parse_type_name(parser, &type_name) != SUCCESS) {
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
    return SUCCESS;
}

static Result parse_type_name(Parser* parser, TypeName* dst) {
    Identifier identifier;
    if (parse_identifier(parser, &identifier) != SUCCESS) {
        return ERROR;
    }
    *dst = (TypeName){
        .kind = TYPE_NAME_IDENTIFIER,
        .as.identifier = identifier,
        .range = identifier.range,
    };
    return SUCCESS;
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
    return SUCCESS;
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
    if (parse_expression_head(parser, &expr, &range) != SUCCESS) {
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
        ) != SUCCESS
    ) {
        return ERROR;
    }

    *dst = expr;
    if (dst_range) {
        *dst_range = range;
    }

    return SUCCESS;
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
        return SUCCESS;
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
        return SUCCESS;
    }

    if (operator_precedence <= prev_precedence) {
        *parsed = false;
        return SUCCESS;
    }
    parser->pos = new_parser_pos;

    ExpressionId rhs;
    Range rhs_range;
    if (parse_expression_precedence(parser, operator_precedence, &rhs, &rhs_range) != SUCCESS) {
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
        ) != SUCCESS
    ) {
        return ERROR;
    }

    *dst = expr;
    *dst_range = range;

    return SUCCESS;
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
            != SUCCESS
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
        return SUCCESS;
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
        return SUCCESS;

    case TOKEN_FN:
        expr.kind = EXPRESSION_FUNCTION;
        if (
            parse_function(parser, &expr.as.function, &expr.range)
            != SUCCESS
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

    return SUCCESS;
}
