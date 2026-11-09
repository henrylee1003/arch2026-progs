/* Bare-metal sweep for the rubik instruction budget: solve every one of the
 * 3,674,160 states with rubik_solve_asm, record the instructions retired per
 * call, and report the maximum and a histogram. The 20 most expensive states
 * are then measured with rubik_solve as well. Built and run by
 * mytests/rubik/worst.sh; not part of the graded sources.
 */
#include <stddef.h>
#include <stdint.h>

#include "print.h"
#include "src/rubik/rubik.h"

uint64_t get_instret(void);

enum { TOP = 20 };

static char top_state[TOP][16];
static uint32_t top_cost[TOP];

static void note(const char *s, uint32_t cost)
{
    int i = TOP - 1;
    if (cost <= top_cost[i])
        return;
    while (i > 0 && top_cost[i - 1] < cost) {
        top_cost[i] = top_cost[i - 1];
        for (int k = 0; k < 15; k++)
            top_state[i][k] = top_state[i - 1][k];
        i--;
    }
    top_cost[i] = cost;
    for (int k = 0; k < 15; k++)
        top_state[i][k] = s[k];
}

/* Next permutation in lexicographic order; 0 after the last one. */
static int next_perm(uint8_t *p)
{
    int i = 5;
    while (i >= 0 && p[i] > p[i + 1])
        i--;
    if (i < 0)
        return 0;
    int j = 6;
    while (p[j] < p[i])
        j--;
    uint8_t x = p[i];
    p[i] = p[j];
    p[j] = x;
    for (int a = i + 1, b = 6; a < b; a++, b--) {
        x = p[a];
        p[a] = p[b];
        p[b] = x;
    }
    return 1;
}

int main(void)
{
    uint8_t p[7] = {0, 1, 2, 3, 4, 5, 6};
    char s[16], out[64];
    uint32_t max = 0, hist[16] = {0};
    unsigned long long total = 0, states = 0, bad = 0;

    s[14] = '\0';
    do {
        for (int i = 0; i < 7; i++)
            s[i] = (char) ('1' + p[i]);
        uint8_t o[6] = {0};
        for (int k = 0; k < 729; k++) {
            unsigned sum = 0;
            for (int i = 0; i < 6; i++) {
                s[7 + i] = (char) ('1' + o[i]);
                sum += o[i];
            }
            while (sum >= 3)
                sum -= 3;
            s[13] = (char) ('1' + (sum ? 3 - sum : 0));
            uint64_t t0 = get_instret();
            int n = rubik_solve_asm(s, out, sizeof out);
            uint32_t cost = (uint32_t) (get_instret() - t0);
            if (n < 0 || n > 11)
                bad++;
            total += cost;
            states++;
            if (cost > max)
                max = cost;
            hist[cost >> 19 < 15 ? cost >> 19 : 15]++;
            note(s, cost);
            for (int i = 5; i >= 0; i--) {
                if (++o[i] < 3)
                    break;
                o[i] = 0;
            }
        }
    } while (next_perm(p));

    bm_printf("states %llu, bad %llu, max %u, total %llu\n", states, bad, max,
              total);
    for (int i = 0; i < 16; i++)
        if (hist[i])
            bm_printf("  [%u, %u) M/2: %u states\n", i, i + 1, hist[i]);
    for (int i = 0; i < TOP; i++) {
        uint64_t t0 = get_instret();
        int n = rubik_solve(top_state[i], out, sizeof out);
        uint32_t c = (uint32_t) (get_instret() - t0);
        bm_printf("%s moves %d asm %u c %u\n", top_state[i], n, top_cost[i], c);
    }
    return bad != 0;
}
