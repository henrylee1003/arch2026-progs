/* Usage: ./gen_tables <dir>
 * Writes <dir>/tables.c and <dir>/tables.S for src/rubik: quarter-turn move
 * tables of the permutation and twist coordinates, their distance tables, and
 * the sorted table of every state within 5 moves with a move toward solved.
 * The cube model is the one of tests/trubik.c (sysprog21/minirubik).
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { C = 7, NP = 5040, NT = 729, NS = NP * NT, DEPTH = 5 };

static const uint8_t source[3][C] = {
    {1, 4, 2, 0, 3, 5, 6},
    {0, 1, 2, 4, 5, 6, 3},
    {0, 2, 5, 3, 1, 4, 6},
};
static const uint8_t twist[3][C] = {
    {1, 2, 0, 2, 1, 0, 0},
    {0, 0, 0, 1, 2, 1, 2},
    {0, 0, 0, 0, 0, 0, 0},
};
static const unsigned fact[C] = {720, 120, 24, 6, 2, 1, 1};

static unsigned rank_p(const uint8_t *p)
{
    unsigned r = 0;
    for (int i = 0; i < C; i++)
        for (int j = i + 1; j < C; j++)
            if (p[j] < p[i])
                r += fact[i];
    return r;
}

static void unrank_p(unsigned r, uint8_t *p)
{
    uint8_t left[C] = {0, 1, 2, 3, 4, 5, 6};
    int n = C;
    for (int i = 0; i < C; i++) {
        unsigned q = r / fact[i];
        r %= fact[i];
        p[i] = left[q];
        memmove(left + q, left + q + 1, (size_t) (n - q - 1));
        n--;
    }
}

static unsigned rank_t(const uint8_t *o)
{
    unsigned r = 0;
    for (int i = 0; i < 6; i++)
        r = r * 3 + o[i];
    return r;
}

static void unrank_t(unsigned r, uint8_t *o)
{
    unsigned sum = 0;
    for (int i = 5; i >= 0; i--) {
        o[i] = (uint8_t) (r % 3);
        r /= 3;
        sum += o[i];
    }
    o[6] = (uint8_t) ((3 - sum % 3) % 3);
}

static uint16_t pmove[3][NP], tmove[3][NT];
static uint8_t pdist[NP], tdist[NT];

static unsigned apply_p(unsigned p, int m)
{
    for (int q = 0; q <= m % 3; q++)
        p = pmove[m / 3][p];
    return p;
}

static unsigned apply_t(unsigned t, int m)
{
    for (int q = 0; q <= m % 3; q++)
        t = tmove[m / 3][t];
    return t;
}

static void bfs1(uint8_t *dist, unsigned n, int perm)
{
    unsigned *queue = malloc(n * sizeof *queue), head = 0, tail = 1;
    memset(dist, 0xFF, n);
    dist[0] = 0;
    queue[0] = 0;
    while (head < tail) {
        unsigned x = queue[head++];
        for (int m = 0; m < 9; m++) {
            unsigned y = perm ? apply_p(x, m) : apply_t(x, m);
            if (dist[y] == 0xFF) {
                dist[y] = (uint8_t) (dist[x] + 1);
                queue[tail++] = y;
            }
        }
    }
    free(queue);
}

static int cmp_u32(const void *a, const void *b)
{
    uint32_t x = *(const uint32_t *) a, y = *(const uint32_t *) b;
    return (x > y) - (x < y);
}

static FILE *open_out(const char *dir, const char *name)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *f = fopen(path, "w");
    if (!f) {
        perror(path);
        exit(1);
    }
    return f;
}

static void c_array16(FILE *f, const char *decl, const uint16_t *v, unsigned n)
{
    fprintf(f, "%s = {\n", decl);
    for (unsigned i = 0; i < n; i++)
        fprintf(f, "%s%u,%s", (i % 12) ? " " : "    ", v[i],
                i % 12 == 11 || i + 1 == n ? "\n" : "");
    fprintf(f, "};\n\n");
}

static void c_array8(FILE *f, const char *decl, const uint8_t *v, unsigned n)
{
    fprintf(f, "%s = {\n", decl);
    for (unsigned i = 0; i < n; i++)
        fprintf(f, "%s%u,%s", (i % 24) ? " " : "    ", v[i],
                i % 24 == 23 || i + 1 == n ? "\n" : "");
    fprintf(f, "};\n\n");
}

static void s_data(FILE *f,
                   const char *dir,
                   const uint32_t *v,
                   unsigned n,
                   unsigned per_line)
{
    for (unsigned i = 0; i < n; i++)
        fprintf(f, "%s%u%s", (i % per_line) ? ", " : dir, v[i],
                i % per_line == per_line - 1 || i + 1 == n ? "\n" : "");
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s <dir>\n", argv[0]);
        return 2;
    }
    for (int f = 0; f < 3; f++) {
        for (unsigned r = 0; r < NP; r++) {
            uint8_t p[C], n[C];
            unrank_p(r, p);
            for (int i = 0; i < C; i++)
                n[i] = p[source[f][i]];
            pmove[f][r] = (uint16_t) rank_p(n);
        }
        for (unsigned r = 0; r < NT; r++) {
            uint8_t o[C], n[C];
            unrank_t(r, o);
            for (int i = 0; i < C; i++)
                n[i] = (uint8_t) ((o[source[f][i]] + twist[f][i]) % 3);
            tmove[f][r] = (uint16_t) rank_t(n);
        }
    }
    bfs1(pdist, NP, 1);
    bfs1(tdist, NT, 0);

    /* Breadth-first search to depth 5 over full states. The move stored for a
     * state undoes the move that first reached it. */
    uint8_t *dist = malloc(NS), *back = malloc(NS);
    uint32_t *queue = malloc(NS * sizeof *queue), head = 0, tail = 1;
    memset(dist, 0xFF, NS);
    dist[0] = 0;
    back[0] = 15;
    queue[0] = 0;
    while (head < tail) {
        uint32_t s = queue[head++];
        if (dist[s] == DEPTH)
            continue;
        unsigned p = s / NT, t = s % NT;
        for (int m = 0; m < 9; m++) {
            uint32_t n = apply_p(p, m) * NT + apply_t(t, m);
            if (dist[n] == 0xFF) {
                dist[n] = (uint8_t) (dist[s] + 1);
                back[n] = (uint8_t) (m / 3 * 3 + 2 - m % 3);
                queue[tail++] = n;
            }
        }
    }
    unsigned nball = tail;
    uint32_t *cball = malloc(nball * 4), *sball = malloc(nball * 4);
    for (unsigned i = 0; i < nball; i++) {
        uint32_t s = queue[i], p = s / NT, t = s % NT, m = back[s];
        cball[i] = ((p << 10 | t) << 4) | m;
        sball[i] = ((2 * p << 11 | 2 * t) << 4) | m;
    }
    qsort(cball, nball, 4, cmp_u32);
    qsort(sball, nball, 4, cmp_u32);

    /* Self-check: from every entry the stored moves reach solved in at most
     * DEPTH steps, one step per lookup. */
    for (unsigned i = 0; i < nball; i++) {
        uint32_t s = queue[i];
        for (int k = 0; s; k++) {
            if (k == DEPTH || back[s] > 8) {
                fprintf(stderr, "bad entry %u\n", queue[i]);
                return 1;
            }
            int m = back[s];
            s = apply_p(s / NT, m) * NT + apply_t(s % NT, m);
            if (dist[s] == 0xFF) {
                fprintf(stderr, "left the ball at %u\n", queue[i]);
                return 1;
            }
        }
    }

    FILE *f = open_out(argv[1], "tables.c");
    fprintf(f,
            "/* Generated by mytests/rubik/gen_tables.c. Do not edit. */\n"
            "#include \"tables.h\"\n\n");
    c_array16(f, "const uint16_t rubik_pmove[3 * RUBIK_NP]", &pmove[0][0],
              3 * NP);
    c_array16(f, "const uint16_t rubik_tmove[3 * RUBIK_NT]", &tmove[0][0],
              3 * NT);
    c_array8(f, "const uint8_t rubik_pdist[RUBIK_NP]", pdist, NP);
    c_array8(f, "const uint8_t rubik_tdist[RUBIK_NT]", tdist, NT);
    fprintf(f, "const uint32_t rubik_ball[RUBIK_NBALL] = {\n");
    for (unsigned i = 0; i < nball; i++)
        fprintf(f, "%s%uu,%s", (i % 6) ? " " : "    ", cball[i],
                i % 6 == 5 || i + 1 == nball ? "\n" : "");
    fprintf(f, "};\n");
    fclose(f);

    f = open_out(argv[1], "tables.S");
    fprintf(
        f,
        "# Generated by mytests/rubik/gen_tables.c. Do not edit.\n"
        "# The assembly copy of the tables: coordinates are stored doubled\n"
        "# (byte offsets into the halfword tables), the distance tables are\n"
        "# indexed by the doubled coordinate, and a ball entry is\n"
        "# (2p << 11 | 2t) << 4 | move.\n\n"
        "    .section .rodata\n"
        "    .globl  rubik_asm_pmove\n"
        "    .globl  rubik_asm_tmove\n"
        "    .globl  rubik_asm_pdist\n"
        "    .globl  rubik_asm_tdist\n"
        "    .globl  rubik_asm_ball\n"
        "    .align  2\n");
    uint32_t *tmp = malloc(2 * NP * 4);
    fprintf(f, "rubik_asm_pmove:\n");
    for (int g = 0; g < 3; g++) {
        for (unsigned i = 0; i < NP; i++)
            tmp[i] = 2u * pmove[g][i];
        s_data(f, "    .half ", tmp, NP, 16);
    }
    fprintf(f, "rubik_asm_tmove:\n");
    for (int g = 0; g < 3; g++) {
        for (unsigned i = 0; i < NT; i++)
            tmp[i] = 2u * tmove[g][i];
        s_data(f, "    .half ", tmp, NT, 16);
    }
    fprintf(f, "rubik_asm_pdist:\n");
    for (unsigned i = 0; i < 2 * NP; i++)
        tmp[i] = i % 2 ? 0 : pdist[i / 2];
    s_data(f, "    .byte ", tmp, 2 * NP, 32);
    fprintf(f, "rubik_asm_tdist:\n");
    for (unsigned i = 0; i < 2 * NT; i++)
        tmp[i] = i % 2 ? 0 : tdist[i / 2];
    s_data(f, "    .byte ", tmp, 2 * NT, 32);
    fprintf(f, "    .align  2\nrubik_asm_ball:\n");
    s_data(f, "    .word ", sball, nball, 8);
    fprintf(f, "    .word 0xffffffff  # sentinel above every key\n");
    fclose(f);

    unsigned pmax = 0, tmax = 0;
    for (unsigned i = 0; i < NP; i++)
        pmax = pdist[i] > pmax ? pdist[i] : pmax;
    for (unsigned i = 0; i < NT; i++)
        tmax = tdist[i] > tmax ? tdist[i] : tmax;
    printf("ball entries: %u, pdist max %u, tdist max %u\n", nball, pmax, tmax);
    return 0;
}
