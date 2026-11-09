#include "rubik.h"
#include <string.h>
#include "tables.h"

/* Iterative deepening with the pattern databases as the bound, ending in the
 * table of all states within 5 moves: for a bound d the search goes d - 5 moves
 * deep and looks the state up there. On a shortest path the state 5 moves
 * before the end is in the table, so the first d that succeeds is the distance.
 * The table then supplies the last moves one lookup at a time.
 */

#define RUBIK_DEPTH_MAX 11 /* diameter of the half-turn metric */

static const char *const names[9] = {"R",  "R2", "R'", "B", "B2",
                                     "B'", "D",  "D2", "D'"};
static const uint8_t face_of[9] = {0, 0, 0, 1, 1, 1, 2, 2, 2};
static const uint8_t turns_of[9] = {1, 2, 3, 1, 2, 3, 1, 2, 3};
static const uint16_t poff[3] = {0, RUBIK_NP, 2 * RUBIK_NP};
static const uint16_t toff[3] = {0, RUBIK_NT, 2 * RUBIK_NT};

/* Ranks of a valid state, or -1. */
static int parse(const char *s, unsigned *pp, unsigned *pt)
{
    static const uint16_t fact[7] = {720, 120, 24, 6, 2, 1, 0};
    uint8_t p[7];
    unsigned seen = 0, sum = 0, t = 0, r = 0;

    for (int i = 0; i < 7; i++) {
        unsigned d = (unsigned) (unsigned char) s[i] - '1';
        if (d >= 7 || ((seen >> d) & 1))
            return -1;
        seen |= 1u << d;
        p[i] = (uint8_t) d;
    }
    for (int i = 0; i < 7; i++) {
        unsigned d = (unsigned) (unsigned char) s[7 + i] - '1';
        if (d >= 3)
            return -1;
        sum += d;
        if (i < 6)
            t = (t << 1) + t + d;
    }
    if (s[14] != '\0')
        return -1;
    while (sum >= 3)
        sum -= 3;
    if (sum)
        return -1;
    for (int i = 0; i < 7; i++)
        for (int j = i + 1; j < 7; j++)
            if (p[j] < p[i])
                r += fact[i];
    *pp = r;
    *pt = t;
    return 0;
}

/* The move toward solved stored for (p, t), or -1 if it is more than 5 moves
 * away. */
static int ball_move(unsigned p, unsigned t)
{
    uint32_t key = ((p << 10) | t) << 4;
    unsigned lo = 0, hi = RUBIK_NBALL;

    while (lo < hi) {
        unsigned mid = (lo + hi) >> 1;
        if (rubik_ball[mid] < key)
            lo = mid + 1;
        else
            hi = mid;
    }
    if (lo < RUBIK_NBALL && (rubik_ball[lo] >> 4) == (key >> 4))
        return (int) (rubik_ball[lo] & 15);
    return -1;
}

static unsigned heuristic(unsigned p, unsigned t)
{
    unsigned a = rubik_pdist[p], b = rubik_tdist[t];
    return a > b ? a : b;
}

int rubik_solve(const char *state, char *out, size_t cap)
{
    unsigned p, t;
    if (parse(state, &p, &t))
        return -1;

    uint8_t path[RUBIK_DEPTH_MAX];
    int len = 0;
    if (ball_move(p, t) < 0) {
        unsigned np[7], nt[7], cp[7], ct[7];
        uint8_t next[7];
        unsigned h = heuristic(p, t);
        len = -1;
        for (int d = h > 6 ? (int) h : 6; d <= 11 && len < 0; d++) {
            int k = d - 5, g = 0;
            np[0] = p;
            nt[0] = t;
            next[0] = 0;
            while (g >= 0) {
                if (next[g] == 9) {
                    g--;
                    continue;
                }
                int m = next[g]++;
                int f = face_of[m];
                if (g > 0 && f == face_of[path[g - 1]]) {
                    next[g] = (uint8_t) (3 * f + 3);
                    continue;
                }
                if (turns_of[m] == 1) {
                    cp[g] = np[g];
                    ct[g] = nt[g];
                }
                cp[g] = rubik_pmove[poff[f] + cp[g]];
                ct[g] = rubik_tmove[toff[f] + ct[g]];
                if ((int) heuristic(cp[g], ct[g]) > d - g - 1)
                    continue;
                path[g] = (uint8_t) m;
                if (g + 1 == k) {
                    if (ball_move(cp[g], ct[g]) >= 0) {
                        len = k;
                        p = cp[g];
                        t = ct[g];
                        break;
                    }
                    continue;
                }
                g++;
                np[g] = cp[g - 1];
                nt[g] = ct[g - 1];
                next[g] = 0;
            }
        }
        if (len < 0)
            return -1;
    }

    /* Finish with the table: one lookup per move until solved. */
    while (p || t) {
        int m = ball_move(p, t);
        for (int q = 0; q < turns_of[m]; q++) {
            p = rubik_pmove[poff[face_of[m]] + p];
            t = rubik_tmove[toff[face_of[m]] + t];
        }
        path[len++] = (uint8_t) m;
    }

    char buf[RUBIK_DEPTH_MAX * 3];
    size_t n = 0;
    for (int i = 0; i < len; i++) {
        if (i)
            buf[n++] = ' ';
        for (const char *c = names[path[i]]; *c; c++)
            buf[n++] = *c;
    }
    buf[n] = '\0';
    if (n + 1 > cap) {
        if (cap)
            out[0] = '\0';
        return -2;
    }
    memcpy(out, buf, n + 1);
    return len;
}
