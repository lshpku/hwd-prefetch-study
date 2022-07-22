#include <stdlib.h>
#include <stdio.h>
#include "util.h"

#define LINE_SIZE 64
#define ARRAY_BASE 0x88000000
#define PERM_SIZE 16

unsigned next()
{
    const unsigned A = 0x50e5b68d;
    const unsigned C = 0xef23f693;
    static unsigned x = 0;
    x = (x * A) + C;
    return x;
}

int main()
{
    printf("begin\n");

    void *p = (void *)ARRAY_BASE;
    volatile l2ctl_t *l2ctl = (void *)L2CTL_BASE;

    for (int sel = 0; sel < 12; sel++) {
        l2ctl->prefetch.sel = sel % 3;
        l2ctl->prefetch.args[0] = ((sel / 3) + 1) * 4;

        l2perf_t perf0 = l2ctl->perf;
        uint64_t cycle0 = get_cycle();

        for (int n = 0; n < 100; n++) {
            int perm[PERM_SIZE];
            for (int i = 0; i < PERM_SIZE; i++) {
                perm[i] = i;
            }
            for (int i = 0; i < PERM_SIZE; i++) {
                int j = next() % (PERM_SIZE - i) + i;
                DONT_TOUCH(*(int *)(p + LINE_SIZE * perm[j]));
                perm[j] = perm[i];
            }
            p += PERM_SIZE * LINE_SIZE;
        }

        uint64_t cycle1 = get_cycle();
        l2perf_t perf1 = l2ctl->perf;

        printf("--------------------\n");
        printf("prefetch.sel: %llu\n", l2ctl->prefetch.sel);
        printf("cycle: %llu\n", (cycle1 - cycle0) & ((1ULL << 40) - 1));
        printf("trains    : %llu\n", perf1.trains - perf0.trains);
        printf("trainHits : %llu\n", perf1.trainHits - perf0.trainHits);
        printf("preds     : %llu\n", perf1.preds - perf0.preds);
        printf("predGrants: %llu\n", perf1.predGrants - perf0.predGrants);
    }
}
