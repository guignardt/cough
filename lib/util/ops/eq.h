#pragma once

#include "util/primitives/primitives.h"
#include "util/collections/string.h"

#define eq(T) eq_##T

bool eq(u8)(         u8          a,  u8          b);
bool eq(u16)(        u16         a,  u16         b);
bool eq(u32)(        u32         a,  u32         b);
bool eq(u64)(        u64         a,  u64         b);
bool eq(usize)(      usize       a,  usize       b);
bool eq(uptr)(       uptr        a,  uptr        b);
bool eq(umax)(       umax        a,  umax        b);

bool eq(i8)(         i8          a,  i8          b);
bool eq(i16)(        i16         a,  i16         b);
bool eq(i32)(        i32         a,  i32         b);
bool eq(i64)(        i64         a,  i64         b);
bool eq(isize)(      isize       a,  isize       b);
bool eq(iptr)(       iptr        a,  iptr        b);
bool eq(imax)(       imax        a,  imax        b);

bool eq(bool)(       bool        a,  bool        b);

bool eq(f32)(        f32         a,  f32         b);
bool eq(f64)(        f64         a,  f64         b);

bool eq(char)(       char        a,  char        b);
bool eq(String)(     String      a,  String      b);
bool eq(StringBuf)(  StringBuf   a,  StringBuf   b);
