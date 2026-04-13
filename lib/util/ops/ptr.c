#include "util/ops/ptr.h"

const void* align_down(const void* ptr, usize alignment) {
    return (const void*)((uptr)ptr & -alignment);
}

void* align_down_mut(void* ptr, usize alignment) {
    return (void*)((uptr)ptr & -alignment);
}

usize align_down_size(usize size, usize alignment) {
    return size & -alignment;
}

const void* align_up(const void* ptr, usize alignment) {
    uptr addr = (uptr)ptr;
    uptr algn = alignment;
    addr = (addr + (algn - 1)) & -algn;
    return (const void*)addr;
}

void* align_up_mut(void* ptr, usize alignment) {
    uptr addr = (uptr)ptr;
    uptr algn = alignment;
    addr = (addr + (algn - 1)) & -algn;
    return (void*)addr;
}

usize align_up_size(usize size, usize alignment) {
    return (size + (alignment - 1)) & -alignment;
}
