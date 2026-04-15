#pragma once

#include "diagnostics/report.h"
#include "source/source.h"
#include "tokens/token.h"

typedef enum TokenizerError {
    CE_TOKENIZER_ERROR = 1000,
    CE_INVALID_TOKEN = 1000,
} TokenizerError;

// *dst will be replaced without being freed
// *dst may be uninitialized
void tokenize(String source, Reporter* reporter, TokenStream* dst);
