#pragma once

#include <stdlib.h>

#include "primitives/primitives.h"
#include "alloc/buf.h"

typedef struct Range {
    usize start;
    usize end;
} Range;

#define ArrayBuf(T) ArrayBuf_##T

#define array_buf_new(T)        array_buf_new_##T
#define array_buf_free(T)       array_buf_free_##T
#define array_buf_reserve(T)    array_buf_reserve_##T
#define array_buf_push(T)       array_buf_push_##T
#define array_buf_extend(T)     array_buf_extend_##T
#define array_buf_pop(T)        array_buf_pop_##T

#define DECL_ARRAY_BUF(T)                                                       \
    typedef struct ArrayBuf(T) {                                                \
        T* data;                                                                \
        usize len;                                                              \
        usize capacity;                                                         \
    } ArrayBuf(T);                                                              \
                                                                                \
    ArrayBuf(T)                                                                 \
    array_buf_new(T)(void);                                                     \
                                                                                \
    void                                                                        \
    array_buf_free(T)(                                                          \
        ArrayBuf(T)* array                                                      \
    );                                                                          \
                                                                                \
    void                                                                        \
    array_buf_reserve(T)(                                                       \
        ArrayBuf(T)* array,                                                     \
        usize additional                                                        \
    );                                                                          \
                                                                                \
    void                                                                        \
    array_buf_push(T)(                                                          \
        ArrayBuf(T)* array,                                                     \
        T value                                                                 \
    );                                                                          \
                                                                                \
    void                                                                        \
    array_buf_extend(T)(                                                        \
        ArrayBuf(T)* array,                                                     \
        T const* values,                                                        \
        usize len                                                               \
    );                                                                          \
    \
    T   \
    array_buf_pop(T)(       \
        ArrayBuf(T)* array  \
    );                      \

DECL_ARRAY_BUF(i32);
DECL_ARRAY_BUF(usize);
DECL_ARRAY_BUF(char);

#define IMPL_ARRAY_BUF_NEW(T)                                                   \
    ArrayBuf(T)                                                                 \
    array_buf_new(T)(void) {                                                    \
        return (ArrayBuf(T)){                                                   \
            .data = NULL,                                                       \
            .len = 0,                                                           \
            .capacity = 0                                                       \
        };                                                                      \
    }                                                                           \

#define IMPL_ARRAY_BUF_FREE(T)                                                  \
    void                                                                        \
    array_buf_free(T)(                                                          \
        ArrayBuf(T)* array                                                      \
    ) {                                                                         \
        free(array->data);                                                      \
    }                                                                           \

#define IMPL_ARRAY_BUF_RESERVE(T)                                               \
    void                                                                        \
    array_buf_reserve(T)(                                                       \
        ArrayBuf(T)* array,                                                     \
        usize additional                                                        \
    ) {                                                                         \
        Buf buf = {                                                             \
            .data = (void*)array->data,                                         \
            .size = array->len * sizeof(T),                                     \
            .capacity = array->capacity * sizeof(T),                            \
        };                                                                      \
        buf_reserve(&buf, additional * sizeof(T));                              \
        array->data = (T*)buf.data;                                             \
        array->len = buf.size / sizeof(T);                                      \
        array->capacity = buf.capacity / sizeof(T);                             \
    }                                                                           \

#define IMPL_ARRAY_BUF_PUSH(T)                                                  \
    void                                                                        \
    array_buf_push(T)(                                                          \
        ArrayBuf(T)* array,                                                     \
        T value                                                                 \
    ) {                                                                         \
        array_buf_extend(T)(array, &value, 1);                                  \
    }                                                                           \

#define IMPL_ARRAY_BUF_EXTEND(T)                                                \
    void                                                                        \
    array_buf_extend(T)(                                                        \
        ArrayBuf(T)* array,                                                     \
        T const* values,                                                        \
        usize len                                                               \
    ) {                                                                         \
        Buf buf = {                                                             \
            .data = (void*)array->data,                                         \
            .size = array->len * sizeof(T),                                     \
            .capacity = array->capacity * sizeof(T),                            \
        };                                                                      \
        buf_extend_or_grow(&buf, values, len * sizeof(T), 1);                   \
        array->data = (T*)buf.data;                                             \
        array->len = buf.size / sizeof(T);                                      \
        array->capacity = buf.capacity / sizeof(T);                             \
    }                                                                           \

#define IMPL_ARRAY_BUF_POP(T)                                                   \
    T                                                                           \
    array_buf_pop(T)(                                                           \
        ArrayBuf(T)* array                                                      \
    ) {                                                                         \
        return array->data[--(array->len)];                                     \
    }

#define IMPL_ARRAY_BUF(T)                                                       \
    IMPL_ARRAY_BUF_NEW(T)                                                       \
    IMPL_ARRAY_BUF_FREE(T)                                                      \
    IMPL_ARRAY_BUF_RESERVE(T)                                                   \
    IMPL_ARRAY_BUF_PUSH(T)                                                      \
    IMPL_ARRAY_BUF_EXTEND(T)    \
    IMPL_ARRAY_BUF_POP(T)                                                \
