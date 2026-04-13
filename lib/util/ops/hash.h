#pragma once

#include "util/primitives/primitives.h"
#include "util/collections/string.h"

typedef struct Hasher {
    u64 state;
} Hasher;

Hasher new_hasher(void);
u64 finish_hash(Hasher hasher);

#define hash(T) hash_##T

void hash(u8)(          Hasher* hasher, u8          val);
void hash(u16)(         Hasher* hasher, u16         val);
void hash(u32)(         Hasher* hasher, u32         val);
void hash(u64)(         Hasher* hasher, u64         val);
void hash(usize)(       Hasher* hasher, usize       val);
void hash(uptr)(        Hasher* hasher, uptr        val);
void hash(umax)(        Hasher* hasher, umax        val);

void hash(i8)(          Hasher* hasher, i8          val);
void hash(i16)(         Hasher* hasher, i16         val);
void hash(i32)(         Hasher* hasher, i32         val);
void hash(i64)(         Hasher* hasher, i64         val);
void hash(isize)(       Hasher* hasher, isize       val);
void hash(iptr)(        Hasher* hasher, iptr        val);
void hash(imax)(        Hasher* hasher, imax        val);

void hash(bool)(        Hasher* hasher, bool        val);

void hash(char)(        Hasher* hasher, char        val);
void hash(String)(      Hasher* hasher, String      val);
void hash(StringBuf)(   Hasher* hasher, StringBuf   val);
