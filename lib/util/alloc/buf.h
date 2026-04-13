#pragma once

#include "util/primitives/primitives.h"

typedef struct Buf {
    void* data;
    usize size;
    usize capacity;
} Buf;

Buf buf_new(usize capacity);
void buf_free(Buf* buf);

bool buf_has_capacity_for(Buf* buf, usize additional);
void buf_reserve(Buf* buf, usize additional);

bool buf_align(Buf* buf, usize alignment);
void buf_align_or_grow(Buf* buf, usize alignment);

void* buf_alloc(Buf* buf, usize size, usize alignment);
void* buf_alloc_or_grow(Buf* buf, usize size, usize alignment);

void* buf_extend(Buf* buf, void const* src, usize size, usize alignment);
void* buf_extend_or_grow(Buf* buf, void const* src, usize size, usize alignment);

void buf_cut(Buf* buf, void* dst, usize size);
