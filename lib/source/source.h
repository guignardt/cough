#pragma once

#include <stdio.h>
#include <limits.h>

#include "util/primitives/primitives.h"
#include "util/collections/array.h"
#include "util/collections/string.h"
#include "diagnostics/errno.h"

typedef struct LineColumn {
    usize line;
    usize column;
} LineColumn;

typedef struct SourceText {
    char const* path;   // NULL if stdin
    char const* text;
    ArrayBuf(usize) line_indices;
} SourceText;

Errno read_file(FILE* file, StringBuf* dst);

SourceText source_text_new(char const* path, char const* text);
void source_text_free(SourceText* source);

LineColumn source_text_position(SourceText text, usize index);
