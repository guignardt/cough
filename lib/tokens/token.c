#include "tokens/token.h"

IMPL_ARRAY_BUF(Token)

void token_stream_free(TokenStream* token_stream) {
    array_buf_free(Token)(&token_stream->tokens);
    hash_map_free(usize, usize)(&token_stream->_end_pos);
}

static usize token_len[] = {
    [TOKEN_PAREN_LEFT] = 1,
    [TOKEN_PAREN_RIGHT] = 1,
    [TOKEN_COLON] = 1,
    [TOKEN_COLON_COLON] = 2,
    [TOKEN_EQUAL] = 1,
    [TOKEN_COLON_EQUAL] = 2,
    [TOKEN_ARROW] = 2,
    [TOKEN_DOUBLE_ARROW] = 2,
    [TOKEN_SEMICOLON] = 1,

    [TOKEN_BANG] = 1,
    [TOKEN_AMPERSAND] = 1,
    [TOKEN_TUBE] = 1,

    [TOKEN_IDENTIFIER] = -1,

    [TOKEN_FN] = 2,
    [TOKEN_FALSE] = 5,
    [TOKEN_TRUE] = 4,
};

Range token_range(TokenStream stream, Token token) {
    usize end_pos;
    switch (token.kind) {
    case TOKEN_IDENTIFIER:;
        end_pos = *hash_map_get(usize, usize)(stream._end_pos, token.pos);
        break;

    default:
        end_pos = token.pos + token_len[token.kind];
        break;
    }
    return (Range){ token.pos, end_pos };
}

Range token_range_range(TokenStream stream, Range range) {
    usize start = stream.tokens.data[range.start].pos;
    if (range.end <= range.start) {
        return (Range){ start, start };
    }
    usize end = token_range(stream, stream.tokens.data[range.end - 1]).end;
    return (Range){ start, end };
}
