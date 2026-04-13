exp10_min = -343
exp10_max = 341

def gen(exp10, order, shift):
    bits = order
    while bits >= (1 << 128):
        bits >>= 1
        shift += 1
    while not (bits & (1 << 127)):
        bits <<= 1
        shift -= 1
    mi = bits >> 64
    mf = bits & ((1 << 64) - 1)
    e2 = shift
    print(f"    [{exp10 - exp10_min}] = {{ .mantissa_int = {mi}ull, .mantissa_frac = {mf}ull, .exp2 = {e2}ll }}, // 1e{exp10}")

def main():
    print(
        f"""#include "primitives/primitives.h"

#define EXP10_MIN ({exp10_min})
#define EXP10_MAX ({exp10_max})

typedef struct IntFracExp {{
    u64 mantissa_int;
    u64 mantissa_frac;
    i64 exp2;
}} IntFracExp;

static IntFracExp exp10_table[] = {{"""
    )

    order = 1 << 127
    shift = -63
    for exp10 in range(0, exp10_min - 1, -1):
        gen(exp10, order, shift)
        order *= 16
        shift -= 4
        order //= 10

    order = 10
    for exp10 in range(1, exp10_max + 1):
        gen(exp10, order, 64)
        order *= 10
    
    print("};\n")

main()
