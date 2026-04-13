#include <stdalign.h>

#include "util/primitives/primitives.h"

const void* align_down(const void* ptr, usize alignment);
void* align_down_mut(void* ptr, usize alignment);
usize align_down_size(usize size, usize alignment);

const void* align_up(const void* ptr, usize alignment);
void* align_up_mut(void* ptr, usize alignment);
usize align_up_size(usize size, usize alignment);
