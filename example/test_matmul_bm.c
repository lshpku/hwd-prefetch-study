#include <stdlib.h>
#include <stdio.h>
#include "util.h"

#define N 64
#define ARRAY_BASE 0x88000000

static int dummy_work(int a, int b)
{
    return a * b;
    int k = 13;
    for (int i = 0; i < 10; i++) {
        k = k * a + b;
    }
    return k;
}

int main()
{
    printf("begin\n");

    volatile int(*a)[N] = (void *)ARRAY_BASE;
    volatile int(*b)[N] = (void *)a + N * N * sizeof(int);
    volatile int(*c)[N] = (void *)b + N * N * sizeof(int);

    volatile l2ctl_t *l2ctl = (void *)L2CTL_BASE;
    l2ctl->prefetch.args[0] = 1;
    l2ctl->prefetch.sel = SEL_SPP;

    l2perf_t perf0 = l2ctl->perf;
    uint64_t cycle0 = get_cycle();

    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            int tmp = 0;
            for (int k = 0; k < N; k++)
                tmp += dummy_work(a[i][k], b[k][j]);
            c[i][j] = tmp;
        }
    }

    uint64_t cycle1 = get_cycle();
    l2perf_t perf1 = l2ctl->perf;

    printf("cycle: %llu\n", (cycle1 - cycle0) & ((1ULL << 40) - 1));
    printf("trains   : %llu\n", perf1.train - perf0.train);
    printf("trainHit : %llu\n", perf1.trainHit - perf0.trainHit);
    printf("trainMiss: %llu\n", perf1.trainMiss - perf0.trainMiss);
    printf("trainLate: %llu\n", perf1.trainLate - perf0.trainLate);
    printf("preds    : %llu\n", perf1.pred - perf0.pred);
    printf("predGrant: %llu\n", perf1.predGrant - perf0.predGrant);
}
