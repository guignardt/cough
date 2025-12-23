#pragma once

#include "diagnostics/report.h"
#include "tokens/token.h"
#include "ast/ast.h"

typedef enum ParserError {
    CE_PARSER_ERROR = 2000,
    CE_UNEXPECTED_TOKEN,
    CE_UNCLOSED_PAREN,
} ParserError;

bool parse(
    TokenStream tokens,
    Reporter* reporter,
    Ast* dst
);
