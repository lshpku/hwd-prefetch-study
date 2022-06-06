#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define MIN_N 1024
#define MAX_N (16 * 1024 * 1024)
#define STEP 64
#define EXPECTED_ACCESS 10000000 // 1M

inline uint64_t getcycle()
{
    uint64_t cycle;
    asm volatile("rdcycle %0"
                 : "=r"(cycle));
    return cycle;
}

static void testloop(void *a, int n)
{
    for (int i = 0; i < n; i += STEP) {
        asm volatile(""
                     :
                     : "r"(*(volatile long *)(a + i)));
    }
}

static void test(void *a, int n)
{
    for (int i = 0; i < 3; i++)
        testloop(a, n);

    int loop = (long)EXPECTED_ACCESS * STEP / n;
    if (loop < 1)
        loop = 1;
    uint64_t cycle0 = getcycle();
    for (int i = 0; i < loop; i++)
        testloop(a, n);
    uint64_t cycle1 = getcycle();

    uint64_t cycle = (cycle1 - cycle0) & ((1ULL << 40) - 1);
    printf("{\"loop\":%d,\"n\":%d,\"cycle\":%llu}\n", loop, n, cycle);
}

int main()
{
    void *a = calloc(MAX_N, 1);

    printf("begin\n");

    for (int n = MIN_N; n <= MAX_N; n *= 2) {
        test(a, n);
        if (n == MAX_N)
            break;
        if (n >= 4)
            test(a, n + n / 4);
        if (n >= 2)
            test(a, n + n / 2);
        if (n >= 4)
            test(a, n + n * 3 / 4);
    }
}
