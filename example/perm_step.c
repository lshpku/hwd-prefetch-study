#include <stdlib.h>
#include <stdio.h>
#include "util.h"

#define LINE_SIZE 16
#define ARRAY_BASE 0x88000000
#define PERM_SIZE 16
#define TEST_SIZE (2 * 1024 * 1024)
#define PAGE_SIZE 4096

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

    void *p = malloc(TEST_SIZE);

    for (int i = 0; i < TEST_SIZE / PAGE_SIZE; i++) {
        *(int *)(p + i * PAGE_SIZE) = i;
    }

    uint64_t cycle0 = get_cycle();

    for (int n = 0; n < TEST_SIZE / (PERM_SIZE * LINE_SIZE); n++) {
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
    printf("cycle: %llu\n", (cycle1 - cycle0) & ((1ULL << 40) - 1));
}
