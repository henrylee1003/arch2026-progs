/* Usage: ./host_check [jobs]
 * Solve all 3,674,160 states with rubik_solve on the host and check each
 * answer against an independent model: the length equals the breadth-first
 * distance, and applying the text reaches the solved cube.
 * Build: gcc -O2 -I. mytests/rubik/host_check.c src/rubik/rubik.c
 *        src/rubik/tables.c
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "src/rubik/rubik.h"

enum { C = 7, NS = 5040 * 729 };

typedef struct {
    uint8_t p[C], o[C];
} cube_t;

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
static const char *const names[9] = {"R",  "R2", "R'", "B", "B2",
                                     "B'", "D",  "D2", "D'"};

static cube_t turn(cube_t c, int m)
{
    for (int q = 0; q <= m % 3; q++) {
        cube_t n;
        for (int i = 0; i < C; i++) {
            int from = source[m / 3][i];
            n.p[i] = c.p[from];
            n.o[i] = (uint8_t) ((c.o[from] + twist[m / 3][i]) % 3);
        }
        c = n;
    }
    return c;
}

static uint32_t rank(const cube_t *c)
{
    static const unsigned fact[C] = {720, 120, 24, 6, 2, 1, 1};
    uint32_t p = 0, o = 0;
    for (int i = 0; i < C; i++)
        for (int j = i + 1; j < C; j++)
            if (c->p[j] < c->p[i])
                p += fact[i];
    for (int i = 0; i < 6; i++)
        o = o * 3 + c->o[i];
    return p * 729 + o;
}

static cube_t unrank(uint32_t r)
{
    static const unsigned fact[C] = {720, 120, 24, 6, 2, 1, 1};
    cube_t c;
    uint8_t left[C] = {0, 1, 2, 3, 4, 5, 6};
    unsigned p = r / 729, o = r % 729, sum = 0, n = C;
    for (int i = 0; i < C; i++) {
        unsigned q = p / fact[i];
        p %= fact[i];
        c.p[i] = left[q];
        memmove(left + q, left + q + 1, n - q - 1);
        n--;
    }
    for (int i = 5; i >= 0; i--) {
        c.o[i] = (uint8_t) (o % 3);
        o /= 3;
        sum += c.o[i];
    }
    c.o[6] = (uint8_t) ((3 - sum % 3) % 3);
    return c;
}

static int is_solved(const cube_t *c)
{
    for (int i = 0; i < C; i++)
        if (c->p[i] != i || c->o[i])
            return 0;
    return 1;
}

int main(int argc, char **argv)
{
    int jobs = argc > 1 ? atoi(argv[1]) : 8;
    uint8_t *dist = malloc(NS);
    uint32_t *queue = malloc(NS * sizeof *queue), head = 0, tail = 1;
    memset(dist, 0xFF, NS);
    cube_t solved = unrank(0);
    dist[rank(&solved)] = 0;
    queue[0] = rank(&solved);
    while (head < tail) {
        uint32_t s = queue[head++];
        cube_t c = unrank(s);
        for (int m = 0; m < 9; m++) {
            cube_t n = turn(c, m);
            uint32_t r = rank(&n);
            if (dist[r] == 0xFF) {
                dist[r] = (uint8_t) (dist[s] + 1);
                queue[tail++] = r;
            }
        }
    }

    int fds[64][2];
    if (jobs < 1 || jobs > 64)
        jobs = 8;
    for (int j = 0; j < jobs; j++) {
        if (pipe(fds[j]))
            return 2;
        pid_t pid = fork();
        if (pid < 0)
            return 2;
        if (pid > 0) {
            close(fds[j][1]);
            continue;
        }
        close(fds[j][0]);
        unsigned long count[13] = {0}; /* [12] = errors */
        for (uint32_t r = (uint32_t) j; r < NS; r += (uint32_t) jobs) {
            cube_t c = unrank(r);
            char s[15], out[64];
            for (int i = 0; i < C; i++) {
                s[i] = (char) ('1' + c.p[i]);
                s[i + C] = (char) ('1' + c.o[i]);
            }
            s[14] = '\0';
            int n = rubik_solve(s, out, sizeof out), moves = 0;
            for (const char *tok = strtok(out, " "); n >= 0 && tok;
                 tok = strtok(NULL, " ")) {
                int m = 0;
                while (m < 9 && strcmp(tok, names[m]))
                    m++;
                if (m == 9)
                    break;
                c = turn(c, m);
                moves++;
            }
            count[dist[r]]++;
            if (n != dist[r] || moves != n || !is_solved(&c)) {
                if (count[12]++ < 5)
                    fprintf(stderr, "state %s: got %d, distance %u\n", s, n,
                            dist[r]);
            }
        }
        if (write(fds[j][1], count, sizeof count) != (ssize_t) sizeof count)
            _exit(3);
        _exit(0);
    }
    unsigned long total[13] = {0};
    for (int j = 0; j < jobs; j++) {
        unsigned long count[13];
        if (read(fds[j][0], count, sizeof count) != (ssize_t) sizeof count)
            return 3;
        for (int k = 0; k < 13; k++)
            total[k] += count[k];
    }
    while (wait(NULL) > 0)
        ;
    unsigned long states = 0;
    for (int d = 0; d < 12; d++) {
        printf("distance %2d: %7lu\n", d, total[d]);
        states += total[d];
    }
    printf("states %lu, errors %lu\n", states, total[12]);
    return total[12] != 0 || states != NS;
}
