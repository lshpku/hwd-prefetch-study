#include <stdio.h>
#include <stdlib.h>
#include <x86intrin.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE (128 * 1024 * 1024)
#endif

#define LINE_SIZE 64
#define N (ARRAY_SIZE / sizeof(size_t))

void loop(size_t *arr) {
    volatile size_t *ptr = arr;
    for (int i = 0; i < N; i++) {
        ptr = (size_t *)*ptr;
    }
}

int main()
{
    size_t *arr = calloc(ARRAY_SIZE, 1);

    for (int i = 0; i < N; i++) {
        arr[i] = (size_t)(arr + i);
    }

    // random permutation
    for (int i = 0; i < N; i++) {
        int j = rand() % (N - i);
        if (j > 0) {
            size_t tmp = arr[i];
            arr[i] = arr[i + j];
            arr[i + j] = tmp;
        }
    }

    int nloop = 128 * 1024 * 1024 / N;
    if (nloop < 10) {
        nloop = 10;
    }
    printf("LOOP = %d\n", nloop);

    // warmup
    for (int i = 0; i < 3; i++) {
        loop(arr);
    }

    // access
    uint64_t start = _rdtsc();
    for (int i = 0; i < 10; i++) {
        loop(arr);
    }
    uint64_t end = _rdtsc();
    double cpi = (double)(end - start) / (N * 10);
    printf("CPL = %f\n", cpi);
}
