#pragma once

#include <stdlib.h>

#include "util/primitives/primitives.h"
#include "diagnostics/result.h"

Errno try_malloc(usize size, void** dst);
void* malloc_or_exit(usize size);

Errno try_realloc(void** ptr, usize size);
void* realloc_or_exit(void* ptr, usize size);
