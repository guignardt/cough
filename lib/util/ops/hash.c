#include "util/ops/hash.h"

Hasher new_hasher(void) {
    return (Hasher){ .state = 0 };
}

u64 finish_hash(Hasher hasher) {
    return hasher.state;
}

#define PI ((u64)0x517cc1b727220a95ULL)

#define IMPL_HASH_INTEGRAL(T)                   \
    void hash(T)(Hasher* hasher, T val) {       \
        u64 s = hasher->state;                  \
        s = (s << 5) | (s >> 59);               \
        hasher->state = (s ^ (u64)val) * PI;    \
    }

IMPL_HASH_INTEGRAL(u8)
IMPL_HASH_INTEGRAL(u16)
IMPL_HASH_INTEGRAL(u32)
IMPL_HASH_INTEGRAL(u64)
IMPL_HASH_INTEGRAL(usize)
IMPL_HASH_INTEGRAL(uptr)
IMPL_HASH_INTEGRAL(umax)

IMPL_HASH_INTEGRAL(i8)
IMPL_HASH_INTEGRAL(i16)
IMPL_HASH_INTEGRAL(i32)
IMPL_HASH_INTEGRAL(i64)
IMPL_HASH_INTEGRAL(isize)
IMPL_HASH_INTEGRAL(iptr)
IMPL_HASH_INTEGRAL(imax)

IMPL_HASH_INTEGRAL(bool)

IMPL_HASH_INTEGRAL(char)

void hash(String)(Hasher* hasher, String val) {
    hash_usize(hasher, val.len);
    for (usize i = 0; i < val.len; i++) {
        hash_char(hasher, val.data[i]);
    }
}

void hash(StringBuf)(Hasher* hasher, StringBuf val) {
    hash(String)(hasher, (String){ .data = val.data, .len = val.len });
}
