#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include "util.h"

const char usage[] =
    "usage: l2ctl [-h] command [arg ...]\n";
const char help[] =
    "Commands:\n"
    "  off      turn off prefetching\n"
    "  on       use the Next Line Prefetcher\n"
    "  bop      use the Best Offset Prefetcher\n"
    "  spp      use the Signature Path Prefetcher\n"
    "  config   show l2 config\n"
    "  status   show l2 status\n";

#define COMMAND_ON 1
#define COMMAND_OFF 2
#define COMMAND_CONFIG 3
#define COMMAND_STATUS 4
#define COMMAND_BOP 5
#define COMMAND_SPP 6

#define IS_COMMAND(s, c) \
    else if (!strcmp(command, s)) { command_e = c; }

void use_prefetcher(volatile l2ctl_t *l2ctl, int sel, int argc, char **argv,
                    const char *name)
{
    l2ctl->prefetch.sel = 0;
    printf("use the %s with args (", name);
    for (int i = 0; i < argc; i++) {
        uint64_t arg = atoll(argv[i]);
        l2ctl->prefetch.args[i] = arg;
        if (i)
            printf(", ");
        printf("%llu", arg);
    }
    printf(")\n");
    l2ctl->prefetch.sel = sel;
}

int main(int argc, char **argv)
{
    int opt;
    while ((opt = getopt(argc, argv, ":h")) != -1) {
        switch (opt) {
        case 'h':
            printf("%s\n%s", usage, help);
            return 0;
        case ':':
            printf("option needs a value\n");
            break;
        case '?':
            printf("unknown option: %c\n", optopt);
            break;
        }
    }

    if (optind == argc) {
        fprintf(stderr, "%sMissing command\n", usage);
        exit(-1);
    }

    char *command = argv[optind++];
    int command_e;
    if (0) {
    }
    IS_COMMAND("on", COMMAND_ON)
    IS_COMMAND("off", COMMAND_OFF)
    IS_COMMAND("bop", COMMAND_BOP)
    IS_COMMAND("spp", COMMAND_SPP)
    IS_COMMAND("config", COMMAND_CONFIG)
    IS_COMMAND("status", COMMAND_STATUS)
    else
    {
        fprintf(stderr, "Unknown command: %s\n", command);
        exit(-1);
    }

    // Map physical memory to user space
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

    volatile l2ctl_t *l2ctl = p;

    switch (command_e) {
    case COMMAND_OFF:
        l2ctl->prefetch.sel = 0;
        printf("prefetch off\n");
        break;
    case COMMAND_ON:
        use_prefetcher(l2ctl, 1, argc - optind, argv + optind,
                       "Next Line Prefetcher");
        break;
    case COMMAND_BOP:
        use_prefetcher(l2ctl, 2, argc - optind, argv + optind,
                       "Best Offset Prefetcher");
        break;
    case COMMAND_SPP:
        use_prefetcher(l2ctl, 3, argc - optind, argv + optind,
                       "Signature Path Prefetcher");
        break;

    case COMMAND_CONFIG:
        printf("banks     : %d\n", l2ctl->banks);
        printf("ways      : %d\n", l2ctl->ways);
        printf("sets      : %d\n", 1 << l2ctl->lgSets);
        printf("blockBytes: %d\n", 1 << l2ctl->lgBlockBytes);
        break;

    case COMMAND_STATUS:
        printf("sel  : %llu\n", l2ctl->prefetch.sel);
        printf("train    : %llu\n", l2ctl->perf.train);
        printf("trainHit : %llu\n", l2ctl->perf.trainHit);
        printf("trainMiss: %llu\n", l2ctl->perf.trainMiss);
        printf("trainLate: %llu\n", l2ctl->perf.trainLate);
        printf("pred     : %llu\n", l2ctl->perf.pred);
        printf("predGrant: %llu\n", l2ctl->perf.predGrant);
        break;
    }
}
