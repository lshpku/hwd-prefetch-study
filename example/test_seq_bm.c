#include <stdio.h>
#include <stdlib.h>
#include "util.h"

#define N (1024 * 32)
#define L 32

int main()
{
    volatile void *p = (void *)0x88000000;
    volatile l2ctl_t *l2ctl = (void *)L2CTL_BASE;
    l2ctl->prefetch.args[0] = 1;
    l2ctl->prefetch.sel = SEL_BOP;

    l2perf_t perf0 = l2ctl->perf;
    uint64_t cycle0 = get_cycle();

    for (int k = 0; k < N / L; k++) {
        for (int i = 0; i < L; i++)
            DONT_TOUCH(((char *)p)[i]);

        if (k % 3 == 0)
            p += L;
        else if (k % 3 == 1)
            p -= L * 4;
        else
            p += L * 2;
    }

    uint64_t cycle1 = get_cycle();
    l2perf_t perf1 = l2ctl->perf;
    printf("cycle: %llu\n", (cycle1 - cycle0) & ((1ULL << 40) - 1));
    printf("train    : %llu\n", perf1.train - perf0.train);
    printf("trainHit : %llu\n", perf1.trainHit - perf0.trainHit);
    printf("trainMiss: %llu\n", perf1.trainMiss - perf0.trainMiss);
    printf("trainLate: %llu\n", perf1.trainLate - perf0.trainLate);
    printf("pred     : %llu\n", perf1.pred - perf0.pred);
    printf("predGrant: %llu\n", perf1.predGrant - perf0.predGrant);
}
