#include <ctype.h>
#include <string.h>

#include "tokenizer/tokenizer.h"

IMPL_HASH_MAP(usize, TokenRanges);

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
        TokenRanges ranges = {
            .end_pos = tokenizer->pos
        };
        hash_map_insert(usize, TokenRanges)(
            &tokenizer->dst->_token_ranges,
            start,
            ranges
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

static void push_integer_token(Tokenizer* tokenizer, usize start, usize digits_start, i8 sign) {
    TokenKind kind = (sign != 0) ? TOKEN_SIGNED_INTEGER : TOKEN_UNSIGNED_INTEGER;
    Token token = { .kind = kind, .pos = start };
    array_buf_push(Token)(&tokenizer->dst->tokens, token);
    TokenRanges ranges = {
        .end_pos = tokenizer->pos,
        .subranges = { (Range){ digits_start, tokenizer->pos } },
    };
    hash_map_insert(usize, TokenRanges)(
        &tokenizer->dst->_token_ranges,
        token.pos,
        ranges
    );
}

static i8 number_sign(Tokenizer* tokenizer, usize* number_start) {
    *number_start = tokenizer->pos;
    usize n_tokens = tokenizer->dst->tokens.len;
    if (n_tokens == 0) {
        return 0;
    }
    Token last = tokenizer->dst->tokens.data[n_tokens - 1];
    if (last.pos + 1 != tokenizer->pos) {
        return 0;
    }
    i8 sign;
    switch (last.kind) {
    case TOKEN_PLUS: sign = 1; break;
    case TOKEN_MINUS: sign = -1; break;
    default: return 0;
    }
    array_buf_pop(Token)(&tokenizer->dst->tokens);
    *number_start = last.pos;
    return sign;
}

static void tokenize_number(Tokenizer* tokenizer) {
    usize start;
    i8 sign = number_sign(tokenizer, &start);

    usize integer_start = tokenizer->pos;
    // We check in `tokenize_one` that we start with a digit.
    tokenize_digits(tokenizer);

    if (tokenizer->source.data[tokenizer->pos] != '.') {
        push_integer_token(tokenizer, start, integer_start, sign);
        return;
    }

    Tokenizer tokenizer2 = *tokenizer;
    usize integer_end = tokenizer2.pos;
    tokenizer2.pos++;
    usize fractional_start = tokenizer2.pos;
    if (!tokenize_digits(&tokenizer2)) {
        push_integer_token(tokenizer, start, integer_start, sign);
        return;
    }

    *tokenizer = tokenizer2;
    Token token = { .kind = TOKEN_FLOATING_POINT, .pos = start };
    array_buf_push(Token)(&tokenizer->dst->tokens, token);
    TokenRanges ranges = {
        .end_pos = tokenizer->pos,
        .subranges = {
            (Range){ integer_start, integer_end },
            (Range){ fractional_start, tokenizer->pos },
        },
    };
    hash_map_insert(usize, TokenRanges)(
        &tokenizer->dst->_token_ranges,
        token.pos,
        ranges
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
        ._token_ranges = hash_map_new(usize, TokenRanges)(),
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
