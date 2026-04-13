#include <assert.h>

#include "tokens/token.h"

IMPL_ARRAY_BUF(Token)

void token_stream_free(TokenStream* token_stream) {
    array_buf_free(Token)(&token_stream->tokens);
    hash_map_free(usize, usize)(&token_stream->_end_pos);
}

static TokenKindDescription token_kind_table[] = {
    [TOKEN_PAREN_LEFT] =        {   1,  "(",        "`(`"                       },
    [TOKEN_PAREN_RIGHT] =       {   1,  ")",        "`)`"                       },
    [TOKEN_COLON] =             {   1,  ":",        "`:`"                       },
    [TOKEN_COLON_COLON] =       {   2,  "::",       "`::`"                      },
    [TOKEN_EQUAL] =             {   1,  "=",        "`=`"                       },
    [TOKEN_COLON_EQUAL] =       {   2,  ":=",       "`:=`"                      },
    [TOKEN_ARROW] =             {   2,  "->",       "`->`"                      },
    [TOKEN_DOUBLE_ARROW] =      {   2,  "=>",       "`=>`"                      },
    [TOKEN_SEMICOLON] =         {   1,  ";",        "`;`"                       },

    [TOKEN_BANG] =              {   1,  "!",        "`!`"                       },
    [TOKEN_TUBE] =              {   1,  "|",        "`|`"                       },
    [TOKEN_AMPERSAND] =         {   1,  "&",        "`&`"                       },
    [TOKEN_HAT] =               {   1,  "^",        "`^`"                       },

    [TOKEN_PLUS] =              {   1,  "+",        "`+`"                       },
    [TOKEN_MINUS] =             {   1,  "-",        "`-`"                       },
    [TOKEN_ASTERISK] =          {   1,  "*",        "`*`"                       },
    [TOKEN_SLASH] =             {   1,  "/",        "`/`"                       },
    [TOKEN_PERCENT] =           {   1,  "%",        "`%`"                       },

    [TOKEN_IDENTIFIER] =        {   -1, NULL,       "<identifier>"              },
    [TOKEN_INTEGER] =           {   -1, NULL,       "<integer>"                 },
    [TOKEN_FLOAT] =          {   -1, NULL,       "<floating-point number>"   },

    [TOKEN_LET] =               {   3,  "let",      "`let`"                     },
    [TOKEN_FN] =                {   2,  "fn",       "`fn`"                      },
    [TOKEN_FALSE] =             {   5,  "false",    "`false`"                   },
    [TOKEN_TRUE] =              {   4,  "true",     "`true`"                    },
};

TokenKindDescription token_kind_description(TokenKind token_kind) {
    return token_kind_table[token_kind];
}

Range token_range(TokenStream stream, Token token) {
    usize end_pos;
    TokenKindDescription desc = token_kind_description(token.kind);
    if (desc.len != -1) {
        end_pos = token.pos + desc.len;
    } else {
        usize const* end_pos_ptr = hash_map_get(usize, usize)(stream._end_pos, token.pos);
        assert(end_pos_ptr && "missing token range information (this is a bug; please report)");
        end_pos = *end_pos_ptr;
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
