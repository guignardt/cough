#include <string.h>

#include "util/ops/eq.h"

#define IMPL_EQ_PRIMITIVE(T) bool eq(T)(T a, T b) { return a == b; }

IMPL_EQ_PRIMITIVE(u8)
IMPL_EQ_PRIMITIVE(u16)
IMPL_EQ_PRIMITIVE(u32)
IMPL_EQ_PRIMITIVE(u64)
IMPL_EQ_PRIMITIVE(usize)
IMPL_EQ_PRIMITIVE(uptr)
IMPL_EQ_PRIMITIVE(umax)

IMPL_EQ_PRIMITIVE(i8)
IMPL_EQ_PRIMITIVE(i16)
IMPL_EQ_PRIMITIVE(i32)
IMPL_EQ_PRIMITIVE(i64)
IMPL_EQ_PRIMITIVE(isize)
IMPL_EQ_PRIMITIVE(iptr)
IMPL_EQ_PRIMITIVE(imax)

IMPL_EQ_PRIMITIVE(bool)

IMPL_EQ_PRIMITIVE(f32)
IMPL_EQ_PRIMITIVE(f64)

IMPL_EQ_PRIMITIVE(char)

bool eq(String)(String a, String b) {
    if (a.len != b.len) return false;
    return !strncmp(a.data, b.data, a.len);
}

bool eq(StringBuf)(StringBuf a, StringBuf b) {
    return eq(String)(
        (String){ .data = a.data, .len = a.len },
        (String){ .data = b.data, .len = b.len }
    );
}
