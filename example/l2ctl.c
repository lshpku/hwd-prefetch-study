#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include "util.h"

const char usage[] =
    "usage: l2ctl [-h] command\n";
const char help[] =
    "Commands:\n"
    "  on       turn prefetch on\n"
    "  off      turn prefetch off\n"
    "  config   show l2 config\n"
    "  status   show l2 status\n";

#define COMMAND_ON 1
#define COMMAND_OFF 2
#define COMMAND_CONFIG 3
#define COMMAND_STATUS 4

#define IS_COMMAND(s, c) \
    else if (!strcmp(command, s)) { command_e = c; }

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
    if (optind + 1 != argc) {
        fprintf(stderr, "%sRequire exactly one command\n", usage);
        exit(-1);
    }

    char *command = argv[optind++];
    int command_e;
    if (0) {
    }
    IS_COMMAND("on", COMMAND_ON)
    IS_COMMAND("off", COMMAND_OFF)
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

    volatile l2ctl_t *const l2ctl = p;

    switch (command_e) {
    case COMMAND_ON:
        l2ctl->prefetch.enable = 1;
        if (!l2ctl->prefetch.enable) {
            fprintf(stderr, "Failed to turn on prefetch\n");
            exit(-1);
        }
        printf("prefetch on\n");
        break;

    case COMMAND_OFF:
        l2ctl->prefetch.enable = 0;
        if (l2ctl->prefetch.enable) {
            fprintf(stderr, "Failed to turn off prefetch\n");
            exit(-1);
        }
        printf("prefetch off\n");
        break;

    case COMMAND_CONFIG:
        printf("banks     : %d\n", l2ctl->banks);
        printf("ways      : %d\n", l2ctl->ways);
        printf("sets      : %d\n", 1 << l2ctl->lgSets);
        printf("blockBytes: %d\n", 1 << l2ctl->lgBlockBytes);
        break;

    case COMMAND_STATUS:
        printf("prefetch  : %s\n", l2ctl->prefetch.enable ? "on" : "off");
        printf("trains    : %llu\n", l2ctl->perf.trains);
        printf("trainHits : %llu\n", l2ctl->perf.trainHits);
        printf("preds     : %llu\n", l2ctl->perf.preds);
        printf("predGrants: %llu\n", l2ctl->perf.predGrants);
        printf("cacheables: %llu\n", l2ctl->perf.cacheables);
        printf("enables   : %llu\n", l2ctl->perf.enables);
        printf("predValids: %llu\n", l2ctl->perf.predValids);
        printf("clocks    : %llu\n", l2ctl->perf.clocks);
        printf("missAddr  : 0x%llx\n", l2ctl->perf.missAddr);
        break;
    }
}
