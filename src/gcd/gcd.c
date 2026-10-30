#include "gcd.h"

/* Count trailing zeros of a nonzero x by binary search over the bit position:
 * five tests, no multiply and no table. RV32I has no ctz instruction.
 */
static unsigned ctz32(uint32_t x)
{
    unsigned n = 0;
    if (!(x & 0xFFFFu)) {
        n += 16;
        x >>= 16;
    }
    if (!(x & 0xFFu)) {
        n += 8;
        x >>= 8;
    }
    if (!(x & 0xFu)) {
        n += 4;
        x >>= 4;
    }
    if (!(x & 0x3u)) {
        n += 2;
        x >>= 2;
    }
    if (!(x & 0x1u))
        n += 1;
    return n;
}

/* Binary GCD (Stein): gcd(2a, 2b) = 2 gcd(a, b), gcd(2a, b) = gcd(a, b) for odd
 * b, and gcd(a, b) = gcd(|a - b|, min(a, b)) for odd a and b. The common power
 * of two is the trailing zeros of a | b; every factor of two left in an operand
 * is stripped in one ctz step instead of one bit at a time.
 */
uint32_t gcd_u32(uint32_t a, uint32_t b)
{
    if (a == 0)
        return b;
    if (b == 0)
        return a;
    unsigned shift = ctz32(a | b);
    a >>= ctz32(a);
    do {
        b >>= ctz32(b); /* a and b are odd from here on */
        if (a > b) {
            uint32_t t = a;
            a = b;
            b = t;
        }
        b -= a; /* even, or zero when a == b */
    } while (b != 0);
    return a << shift;
}
