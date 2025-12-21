#pragma once

#include "primitives/primitives.h"
#include "collections/array.h"
#include "collections/hash_map.h"

typedef enum TokenKind {
    TOKEN_PAREN_LEFT,
    TOKEN_PAREN_RIGHT,
    TOKEN_COLON,
    TOKEN_COLON_COLON,
    TOKEN_EQUAL,
    TOKEN_COLON_EQUAL,
    TOKEN_ARROW,
    TOKEN_DOUBLE_ARROW,
    TOKEN_SEMICOLON,

    TOKEN_BANG,
    TOKEN_TUBE,
    TOKEN_AMPERSAND,
    TOKEN_HAT,

    TOKEN_IDENTIFIER,

    TOKEN_LET,
    TOKEN_FN,
    TOKEN_FALSE,
    TOKEN_TRUE,
} TokenKind;

typedef struct Token {
    TokenKind kind;
    usize pos;
} Token;

DECL_ARRAY_BUF(Token)

typedef struct TokenStream {
    String source;
    ArrayBuf(Token) tokens;
    HashMap(usize, usize) _end_pos;
} TokenStream;

void token_stream_free(TokenStream* token_stream);

Range token_range(TokenStream stream, Token token);
Range token_range_range(TokenStream stream, Range range);
