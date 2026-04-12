#include <ctype.h>
#include <string.h>

#include "tokenizer/tokenizer.h"

IMPL_HASH_MAP(usize, usize);

typedef struct Tokenizer {
    String source;
    usize pos;
    Reporter* reporter;
    TokenStream* dst;
    bool error;
} Tokenizer;

static void invalid_token(Tokenizer* tokenizer, usize start) {
    Reporter* reporter = tokenizer->reporter;
    report_start(reporter, SEVERITY_ERROR, CE_INVALID_TOKEN);
    report_message(reporter, format("invalid token"));
    report_source_code(reporter, (Range){ start, tokenizer->pos });
    report_end(reporter);
    tokenizer->error = true;
}

static void tokenize_punctuation(Tokenizer* tokenizer) {
    usize max_len = 0;
    TokenKind kind;
    // all `TokenKind` values < `TOKEN_IDENTIFIER` are 'punctuation'
    for (usize i = TOKEN_PUNCT_START; i <= TOKEN_PUNCT_LAST; i++) {
        TokenKindDescription desc = token_kind_description(i);
        if (!strncmp(
            desc.exact_chars,
            tokenizer->source.data + tokenizer->pos,
            desc.len
        )) {
            if (desc.len > max_len) {
                max_len = desc.len;
                kind = i;
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

    for (usize i = TOKEN_KEYWORD_START; i <= TOKEN_KEYWORD_LAST; i++) {
        TokenKindDescription desc = token_kind_description(i);
        if (!strncmp(p_start, desc.exact_chars, len)) {
            kind = i;
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

static bool tokenize_digits(Tokenizer* tokenizer) {
    bool parsed = false;
    while (isdigit(tokenizer->source.data[tokenizer->pos])) {
        tokenizer->pos++;
        parsed = true;
    }
    return parsed;
}

static void remove_number_sign(Tokenizer* tokenizer, usize* number_start) {
    *number_start = tokenizer->pos;
    usize n_tokens = tokenizer->dst->tokens.len;
    if (n_tokens == 0) {
        return;
    }
    Token last = tokenizer->dst->tokens.data[n_tokens - 1];
    if (last.pos + 1 != tokenizer->pos) {
        return;
    }
    if (last.kind != TOKEN_PLUS && last.kind != TOKEN_MINUS) {
        return;
    }
    array_buf_pop(Token)(&tokenizer->dst->tokens);
    *number_start = last.pos;
}

static void tokenize_number(Tokenizer* tokenizer) {
    bool error = false;

    // tokenize sign
    usize start;
    remove_number_sign(tokenizer, &start);

    // tokenize integer part
    // this is guaranteed to succeed: this function gets called with a numeric head character
    tokenize_digits(tokenizer);

    bool is_integer = true;

    // tokenize optional fractional part
    Tokenizer tokenizer2 = *tokenizer;
    if (tokenizer2.source.data[tokenizer2.pos] == '.') {
        tokenizer2.pos++;
        is_integer = false;
        if (tokenize_digits(&tokenizer2)) {
            *tokenizer = tokenizer2;
        }
    }

    // tokenize optional exponent
    if (tokenizer->source.data[tokenizer->pos] == 'e') {
        tokenizer->pos++;
        if (tokenizer->source.data[tokenizer->pos] == '-') {
            tokenizer->pos++;
        }
        if (!tokenize_digits(tokenizer)) {
            error = true;
        }
        is_integer = false;
    }

    // disallow other trailing characters
    char c;
    while (
        c = tokenizer->source.data[tokenizer->pos],
        isalnum(c) || c == '_'
    ) {
        tokenizer->pos++;
        error = true;
    }
    if (error) {
        invalid_token(tokenizer, start);
    }

    // push token & sparse info
    TokenKind kind = (is_integer) ? TOKEN_INTEGER : TOKEN_FLOATING;
    Token token = { .kind = kind, .pos = start };
    array_buf_push(Token)(&tokenizer->dst->tokens, token);
    hash_map_insert(usize, usize)(
        &tokenizer->dst->_end_pos,
        token.pos,
        tokenizer->pos
    );
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
    if (isdigit(c)) {
        tokenize_number(tokenizer);
    } else if (ispunct(c) && c != '_') {
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
    // TODO: don't quit directly when a tokenization failed
    while (tokenize_one(&tokenizer));
    if (tokenizer.error) {
        return false;
    }
    *dst = stream;
    return true;
}
