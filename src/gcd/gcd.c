#include "gcd.h"

/* Binary GCD (Stein), written from proc_a of Quiz 2 Problem A, which is the C
 * baseline the assignment names. It uses gcd(2a, 2b) = 2 gcd(a, b),
 * gcd(2a, b) = gcd(a, b) for odd b, and gcd(a, b) = gcd(b - a, a) for odd
 * a <= b. Each loop strips one factor of two per pass, as the quiz does; the
 * labels name the matching blocks of the quiz listing.
 */
uint32_t gcd_u32(uint32_t a, uint32_t b)
{
    if (a == 0)
        return b;
    if (b == 0)
        return a;
    unsigned shift = 0;
    while (((a | b) & 1) == 0) { /* pa_both: a shared factor of two */
        a >>= 1;
        b >>= 1;
        shift++;
    }
    while ((a & 1) == 0) /* pa_left: a odd from here on */
        a >>= 1;
    do {
        while ((b & 1) == 0) /* pa_right: b odd */
            b >>= 1;
        if (b < a) { /* pa_order: keep a <= b */
            uint32_t t = a;
            a = b;
            b = t;
        }
        b -= a; /* pa_step: even, or zero when a == b */
    } while (b != 0);
    return a << shift;
}
