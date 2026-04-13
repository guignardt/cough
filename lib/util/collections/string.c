#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

#include "util/collections/array.h"
#include "util/collections/string.h"
#include "diagnostics/errno.h"

String string_slice(String string, Range range) {
    return (String){
        .data = string.data + range.start,
        .len = range.end - range.start,
    };
}

StringBuf string_buf_new(void) {
    return (StringBuf){ .data = NULL, .len = 0, .capacity = 0 };
}

StringBuf format(char const* fmt, ...) {
    // FIXME: error handling

    va_list args, args2;
    va_start(args, fmt);
    va_copy(args2, args);

    errno = 0;
    int len = vsnprintf(NULL, 0, fmt, args);
    if (len < 0) {
        log_errno(errno);
        exit(-1);
    }
    va_end(args);

    StringBuf buf = string_buf_new();
    string_buf_reserve(&buf, len);
    errno = 0;
    if (vsnprintf(buf.data, buf.capacity, fmt, args2) < 0) {
        log_errno(errno);
        exit(-1);
    }
    va_end(args2);
    buf.len = len;

    return buf;
}

void string_buf_free(StringBuf* string) {
    free(string->data);
}

static ArrayBuf(char) to_array(StringBuf* string) {
    return (ArrayBuf(char)){
        .data = string->data,
        .len = string->data ? string->len + 1 : 0,
        .capacity = string->capacity
    };
}

static StringBuf from_array(ArrayBuf(char)* array) {
    return (StringBuf){
        .data = array->data,
        .len = array->len != 0 ? array->len - 1 : 0,
        .capacity = array->capacity
    };
}

void string_buf_reserve(StringBuf* string, usize additional) {
    if (additional == 0) return;
    ArrayBuf(char) array = to_array(string);
    usize additional_bytes = (array.len == 0) ? additional + 1 : additional;
    array_buf_reserve(char)(&array, additional_bytes);
    if (array.len == 0) {
        array_buf_push(char)(&array, '\0');
    }
    *string = from_array(&array);
}

void string_buf_push(StringBuf* string, char c) {
    string_buf_extend_slice(string, (String){ .data = &c, .len = 1 });
}

void string_buf_extend(StringBuf* string, char const* s) {
    usize len = strlen(s);
    string_buf_extend_slice(string, (String){ .data = s, .len = len });
}

void string_buf_extend_slice(StringBuf* string, String s) {
    string_buf_reserve(string, s.len);
    ArrayBuf(char) array = to_array(string);
    if (array.len != 0) {
        array.len -= 1;
    }
    array_buf_extend(char)(&array, s.data, s.len);
    array_buf_push(char)(&array, '\0');
    
    *string = from_array(&array);
}

Errno read_file(FILE* file, StringBuf* dst) {
    usize len = 0;
    while (!feof(file)) {
        string_buf_reserve(dst, (len == 0) ? 64 : len);
        usize additional =
            fread(dst->data + dst->len, 1, dst->capacity - dst->len - 1, file);
        if (additional == 0) {
            return errno;
        }
        dst->data[dst->len + additional] = '\0';
        dst->len += additional;
    }

    return 0;
}
