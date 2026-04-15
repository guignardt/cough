#pragma once

#include "diagnostics/report.h"
#include "tokens/token.h"
#include "ast/ast.h"

typedef enum ParserError {
    CE_PARSER_ERROR = 2000,
    CE_UNEXPECTED_TOKEN,
    CE_UNCLOSED_PARENTHESIS,
    CE_INTEGER_LITERAL_OVERFLOWED
} ParserError;

void parse_module(
    TokenStream tokens,
    Reporter* reporter,
    Module* dst,
    AstData* dst_data
);

void parse_expression(
    TokenStream tokens,
    Reporter* reporter,
    ExpressionId* dst,
    AstData* dst_data
);
