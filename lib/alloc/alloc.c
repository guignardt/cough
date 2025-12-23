#include <string.h>

#include "alloc/alloc.h"
#include "diagnostics/errno.h"

Errno try_malloc(usize size, void** dst) {
    errno = 0;
    void* ptr = malloc(size);
    if (ptr == NULL) {
        return errno ? errno : DUMMY_ERRNO;
    }
    *dst = ptr;
    return 0;
}

void* malloc_or_exit(usize size) {
    void* ptr;
    log_errno_or(try_malloc(size, &ptr), "memory allocation failed");
    return ptr;
}

Errno try_realloc(void** ptr, usize new_size) {
    errno = 0;
    void* new_ptr = realloc(*ptr, new_size);
    if (new_ptr == NULL) {
        return errno ? errno : DUMMY_ERRNO;
    }
    *ptr = new_ptr;
    return 0;
}

void* realloc_or_exit(void* ptr, usize size) {
    log_errno_or(try_realloc(&ptr, size), "memory allocation failed");
    return ptr;
}
