#pragma once

#include "util/primitives/primitives.h"
#include "util/collections/array.h"
#include "util/collections/hash_map.h"

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

    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_ASTERISK,
    TOKEN_SLASH,
    TOKEN_PERCENT,

    TOKEN_IDENTIFIER,

    TOKEN_INTEGER,
    TOKEN_FLOAT,

    TOKEN_LET,
    TOKEN_FN,
    TOKEN_FALSE,
    TOKEN_TRUE,
} TokenKind;

#define TOKEN_PUNCT_START   TOKEN_PAREN_LEFT
#define TOKEN_PUNCT_LAST    TOKEN_PERCENT
#define TOKEN_KEYWORD_START TOKEN_LET
#define TOKEN_KEYWORD_LAST  TOKEN_TRUE

typedef struct Token {
    TokenKind kind;
    usize pos;
} Token;

DECL_ARRAY_BUF(Token)

typedef struct TokenKindDescription {
    usize len; // -1 when length isn't fixed
    char const* exact_chars;
    char const* pretty_description;
} TokenKindDescription;

TokenKindDescription token_kind_description(TokenKind token_kind);

DECL_HASH_MAP(usize, usize);

typedef struct TokenStream {
    String source;
    ArrayBuf(Token) tokens;
    HashMap(usize, usize) _end_pos;
} TokenStream;

void token_stream_free(TokenStream* token_stream);

// bool token_ranges(TokenStream stream, Token token, TokenRanges* dst);
Range token_range(TokenStream stream, Token token);
Range token_range_range(TokenStream stream, Range range);
