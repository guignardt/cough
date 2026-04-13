#include <string.h>

#include "util/alloc/alloc.h"
#include "util/alloc/buf.h"
#include "util/ops/ptr.h"

Buf buf_new(usize capacity) {
    if (capacity == 0) {
        return (Buf){ .data = NULL, .size = 0, .capacity = 0 };
    }
    void* ptr = malloc_or_exit(capacity);
    return (Buf){ .data = ptr, .size = 0, .capacity = capacity };
}

void buf_free(Buf* buf) {
    free(buf->data);
}

bool buf_has_capacity_for(Buf* buf, usize additional) {
    return buf->size + additional <= buf->capacity;
}

static void buf_reserve_for(Buf* buf, usize min_capacity) {
    if (min_capacity <= buf->capacity) return;
    usize next_capacity = (buf->capacity <= 5) ? 8 : buf->capacity * 1.5;
    usize new_capacity = (min_capacity >= next_capacity) ? min_capacity : next_capacity;
    buf->data = realloc_or_exit(buf->data, new_capacity);
    buf->capacity = new_capacity;
}

void buf_reserve(Buf* buf, usize additional) {
    buf_reserve_for(buf, buf->size + additional);
}

bool buf_align(Buf* buf, usize alignment) {
    usize new_size = align_up_size(buf->size, alignment);
    if (new_size >= buf->capacity) return false;
    buf->size = new_size;
    return true;
}

void buf_align_or_grow(Buf* buf, usize alignment) {
    usize new_size = align_up_size(buf->size, alignment);
    buf_reserve_for(buf, new_size);
    buf->size = new_size;
}

void* buf_alloc(Buf* buf, usize size, usize alignment) {
    if (!buf_align(buf, alignment)) return NULL;
    if (!buf_has_capacity_for(buf, size)) return NULL;
    void* ptr = buf->data + buf->size;
    buf->size += size;
    return ptr;
}

void* buf_alloc_or_grow(Buf* buf, usize size, usize alignment) {
    buf_align_or_grow(buf, alignment);
    buf_reserve(buf, size);
    void* ptr = buf->data + buf->size;
    buf->size += size;
    return ptr;
}

void* buf_extend(Buf* buf, void const* src, usize size, usize alignment) {
    void* ptr = buf_alloc(buf, size, alignment);
    if (ptr == NULL) return NULL;
    memcpy(ptr, src, size);
    return ptr;
}

void* buf_extend_or_grow(Buf* buf, void const* src, usize size, usize alignment) {
    void* ptr = buf_alloc_or_grow(buf, size, alignment);
    memcpy(ptr, src, size);
    return ptr;
}

void buf_cut(Buf* buf, void* dst, usize size) {
    if (size == 0) return;
    usize new_size = buf->size - size;
    if (dst != NULL) {
        void const* src = buf->data + new_size;
        memcpy(dst, src, size);
    }
    buf->size = new_size;
}
