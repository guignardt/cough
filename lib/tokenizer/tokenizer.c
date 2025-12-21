#include <ctype.h>
#include <string.h>

#include "tokenizer/tokenizer.h"

typedef struct Tokenizer {
    String source;
    usize pos;
    Reporter* reporter;
    TokenStream* dst;
    bool error;
} Tokenizer;

static void invalid_token(Tokenizer* tokenizer, usize start) {
    Reporter* reporter = tokenizer->reporter;
    // FIXME: error code
    report_start(reporter, SEVERITY_ERROR, 0);
    report_message(reporter, format("invalid token"));
    report_source_code(reporter, (Range){ start, tokenizer->pos });
    report_end(reporter);
    tokenizer->error = true;
}

typedef struct ExactToken {
    char pattern[8];
    TokenKind kind;
} ExactToken;

static ExactToken punctuation[] = {
    { "(", TOKEN_PAREN_LEFT },
    { ")", TOKEN_PAREN_RIGHT },
    { ":", TOKEN_COLON },
    { "::", TOKEN_COLON_COLON },
    { "=", TOKEN_EQUAL },
    { ":=", TOKEN_COLON_EQUAL },
    { "->", TOKEN_ARROW },
    { "=>", TOKEN_DOUBLE_ARROW },
    { ";", TOKEN_SEMICOLON },

    { "!", TOKEN_BANG },
    { "&", TOKEN_AMPERSAND },
    { "|", TOKEN_TUBE },
    { "^", TOKEN_HAT },
};

static void tokenize_punctuation(Tokenizer* tokenizer) {
    usize max_len = 0;
    TokenKind kind;
    for (usize i = 0; i < sizeof(punctuation) / sizeof(ExactToken); i++) {
        ExactToken pattern = punctuation[i];
        if (!strncmp(
            pattern.pattern,
            tokenizer->source.data + tokenizer->pos,
            strlen(pattern.pattern)
        )) {
            usize len = strlen(pattern.pattern);
            if (len > max_len) {
                max_len = len;
                kind = pattern.kind;
            }
        }
    }
    if (max_len == 0) {
        usize start = tokenizer->pos++;
        invalid_token(tokenizer, start);
        return;
    } else {
        Token token = { .pos = tokenizer->pos, .kind = kind };
        array_buf_push(Token)(&tokenizer->dst->tokens, token);
    }
    tokenizer->pos += max_len;
};

static ExactToken keywords[] = {
    { .pattern = "let", .kind = TOKEN_LET },
    { .pattern = "fn", .kind = TOKEN_FN },
    { .pattern = "true", .kind = TOKEN_TRUE },
    { .pattern = "false", .kind = TOKEN_FALSE },
};

// first character must be alphabetic or `_`
// return whether an identifier as been tokenized
static void tokenize_identifier_or_keyword(Tokenizer* tokenizer) {
    usize start = tokenizer->pos;
    // skip first character
    while (++tokenizer->pos < tokenizer->source.len) {
        char c = tokenizer->source.data[tokenizer->pos];
        if (!isalnum(c) && c != '_') {
            break;
        }
    }
    char const* p_start = tokenizer->source.data + start;
    usize len = tokenizer->pos - start;
    TokenKind kind = TOKEN_IDENTIFIER;
    for (usize i = 0; i < sizeof(keywords) / sizeof(ExactToken); i++) {
        ExactToken pattern = keywords[i];
        if (!strncmp(p_start, pattern.pattern, len)) {
            kind = pattern.kind;
            break;
        }
    }
    if (kind == TOKEN_IDENTIFIER) {
        hash_map_insert(usize, usize)(
            &tokenizer->dst->_end_pos,
            start,
            tokenizer->pos
        );
    }
    Token token = { .kind = kind, .pos = start };
    array_buf_push(Token)(&tokenizer->dst->tokens, token);
}

static void skip_whitespace(Tokenizer* p_tokenizer) {
    Tokenizer tokenizer = *p_tokenizer;
    while (tokenizer.pos < tokenizer.source.len) {
        if (isspace(tokenizer.source.data[tokenizer.pos])) {
            tokenizer.pos++;
            continue;
        }
        break;
    }
    *p_tokenizer = tokenizer;
}

bool tokenize_one(Tokenizer* tokenizer) {
    skip_whitespace(tokenizer);
    if (tokenizer->pos == tokenizer->source.len) {
        return false;
    }
    char c = tokenizer->source.data[tokenizer->pos];
    if (ispunct(c) && c != '_') {
        tokenize_punctuation(tokenizer);
    } else if (isalpha(c) || c == '_') {
        tokenize_identifier_or_keyword(tokenizer);
    }
    return true;
}

bool tokenize(String source, Reporter* reporter, TokenStream* dst) {
    TokenStream stream = {
        .source = source,
        .tokens = array_buf_new(Token)(),
        ._end_pos = hash_map_new(usize, usize)(),
    };
    Tokenizer tokenizer = {
        .source = source,
        .pos = 0,
        .reporter = reporter,
        .dst = &stream,
        .error = false,
    };
    while (tokenize_one(&tokenizer));
    if (tokenizer.error) {
        return false;
    }
    *dst = stream;
    return true;
}
