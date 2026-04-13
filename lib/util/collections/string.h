#pragma once

#include <stdio.h>

#include "util/primitives/primitives.h"
#include "util/collections/array.h"
#include "diagnostics/errno.h"

typedef struct String {
    char const* data;
    usize len;
} String;

#define STRING_LITERAL(str) ((String){ .data = str, .len = sizeof(str) - 1 })

String string_slice(String string, Range range);

typedef struct StringBuf {
    char* data;
    usize len;              // not including NUL terminator
    usize capacity;         // including NUL terminator
} StringBuf;

StringBuf string_buf_new(void);
StringBuf format(char const* restrict fmt, ...);
void string_buf_free(StringBuf* string);

void string_buf_reserve(StringBuf* string, usize additional);
void string_buf_push(StringBuf* string, char c);
void string_buf_extend(StringBuf* string, char const* s);
void string_buf_extend_slice(StringBuf* string, String s);

Errno read_file(FILE* file, StringBuf* dst);
