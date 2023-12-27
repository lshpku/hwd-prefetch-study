#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <stdint.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include "util.h"

struct timespec time1, time2;

l2ctl_t *l2ctl = NULL;

void open_l2ctl()
{
    int memfd = open("/dev/mem", O_RDWR | O_SYNC);
    if (memfd < 0) {
        fprintf(stderr, "Failed to open /dev/mem: %s\n", strerror(errno));
        exit(-1);
    }

    void *p = NULL;
    p = mmap(p, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, memfd, L2CTL_BASE);
    if ((long)p < 0) {
        fprintf(stderr, "Failed to mmap /dev/mem: %s\n", strerror(errno));
        exit(-1);
    }
    close(memfd);

    l2ctl = p;
}

void print(FILE *file, time_t sec, long usec, l2perf_t *perf)
{
    fprintf(file, "{\"time\":%ld.%06ld", sec, usec);
    fprintf(file, ",\"train\":%llu", perf->train);
    fprintf(file, ",\"trainHit\":%llu", perf->trainHit);
    fprintf(file, ",\"trainMiss\":%llu", perf->trainMiss);
    fprintf(file, ",\"trainLate\":%llu", perf->trainLate);
    fprintf(file, ",\"pred\":%llu", perf->pred);
    fprintf(file, ",\"predGrant\":%llu}\n", perf->predGrant);
}

void sigalrm_handler(int signo)
{
    clock_gettime(CLOCK_REALTIME, &time2);
    if (time2.tv_nsec < time1.tv_nsec) {
        time2.tv_sec--;
        time2.tv_nsec += 1000000000L;
    }
    time_t sec = time2.tv_sec - time1.tv_sec;
    long usec = (time2.tv_nsec - time1.tv_nsec) / 1000;

    l2perf_t perf = l2ctl->perf;
    print(stdout, sec, usec, &perf);
    print(stderr, sec, usec, &perf);

    if (signo == SIGALRM)
        alarm(60);
}

int main(int argc, char *argv[])
{
    if (argc == 1) {
        fprintf(stderr, "usage: %s executable [args...]\n", argv[0]);
        exit(-1);
    }

    open_l2ctl();

    pid_t pid = fork();
    if (pid == 0) {
        for (int i = 1; i < argc; i++)
            argv[i - 1] = argv[i];
        argv[argc - 1] = NULL;
        execvp(argv[0], argv);
    }

    clock_gettime(CLOCK_REALTIME, &time1);
    sigalrm_handler(SIGALRM);
    signal(SIGALRM, sigalrm_handler);

    wait(NULL);
    signal(SIGALRM, SIG_DFL);
    sigalrm_handler(0);
}
