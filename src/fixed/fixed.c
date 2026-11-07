#include "fixed.h"

/* Quiz 3 Problem C. */

uint32_t op1(uint32_t a, uint32_t b, int *n)
{
    *n = 0;
    while (b) {
        uint32_t c = (a & b) << 1;
        a = a ^ b;
        b = c;
        ++*n;
    }
    return a;
}

uint32_t op2(uint32_t a, uint32_t b)
{
    return a - b;
}

uint32_t op3(uint32_t a, uint32_t b)
{
    return (a >> 1) + (b >> 1) + (a & b & 1);
}

/* (a * b + 2^19) >> 20 without __muldi3: build the unsigned 64-bit product
 * from 16-bit halves, then subtract b << 32 for a < 0 and a << 32 for b < 0
 * to get the signed product modulo 2^64.
 */
int32_t q_mul(int32_t a, int32_t b)
{
    uint32_t ua = (uint32_t) a, ub = (uint32_t) b;
    uint32_t al = ua & 0xFFFF, ah = ua >> 16;
    uint32_t bl = ub & 0xFFFF, bh = ub >> 16;
    uint64_t p = (uint64_t) (al * bl) + ((uint64_t) (al * bh) << 16) +
                 ((uint64_t) (ah * bl) << 16) + ((uint64_t) (ah * bh) << 32);
    if (a < 0)
        p -= (uint64_t) ub << 32;
    if (b < 0)
        p -= (uint64_t) ua << 32;
    p += 1u << (Q - 1);
    return (int32_t) (uint32_t) (p >> Q);
}
