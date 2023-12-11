#include <stdlib.h>
#include <stdio.h>
#include "util.h"

#define STEP 64
#define ARRAY_BASE 0x88000000

static void dummy_work(unsigned k)
{
    const unsigned a = 0xb27ad8fa, b = 0x1507042d;
    for (int i = 0; i < 10; i++) {
        k = k * a + b;
    }
    DONT_TOUCH(k);
}

#define LOOPIT(msg, body)                           \
    do {                                            \
        uint64_t cycle0 = get_cycle();              \
        for (int i = 0; i < 1024; i++)              \
            body;                                   \
        uint64_t cycle1 = get_cycle();              \
        printf("%s: %llu\n", msg, cycle1 - cycle0); \
    } while (0)

int main()
{
    printf("begin\n");

    void *p = (void *)ARRAY_BASE;
    int a = 1;
    dummy_work(a);

    volatile l2ctl_t *l2ctl = (void *)L2CTL_BASE;
    l2ctl->prefetch.sel = SEL_NONE;

    for (int i = 0; i < 5; i++) {
        printf("-------------------\n");
        LOOPIT("dummy work (best)  ", {
            dummy_work(a);
            a = *(volatile int *)p;
        });
        p += STEP;
        LOOPIT("no prefetch (worst)", {
            dummy_work(a);
            a = *(volatile int *)p;
            p += STEP;
        });
        LOOPIT("hand prefetch +0   ", {
            l2ctl->prefetch.read = (uint64_t)p;
            dummy_work(a);
            a = *(volatile int *)p;
            p += STEP;
        });
        LOOPIT("hand prefetch +1   ", {
            l2ctl->prefetch.read = (uint64_t)p + STEP;
            dummy_work(a);
            a = *(volatile int *)p;
            p += STEP;
        });
        LOOPIT("hand prefetch +2   ", {
            l2ctl->prefetch.read = (uint64_t)p + STEP * 2;
            dummy_work(a);
            a = *(volatile int *)p;
            p += STEP;
        });
        l2ctl->prefetch.sel = SEL_NEXTLINE;
        l2ctl->prefetch.args[0] = 0;
        LOOPIT("nextline dummy     ", {
            dummy_work(a);
            a = *(volatile int *)p;
            p += STEP;
        });
        l2ctl->prefetch.args[0] = 0x1;
        LOOPIT("nextline +1        ", {
            dummy_work(a);
            a = *(volatile int *)p;
            p += STEP;
        });
        l2ctl->prefetch.args[0] = 0x2;
        LOOPIT("nextline +2        ", {
            dummy_work(a);
            a = *(volatile int *)p;
            p += STEP;
        });
        l2ctl->prefetch.args[0] = 0x4;
        LOOPIT("nextline +3        ", {
            dummy_work(a);
            a = *(volatile int *)p;
            p += STEP;
        });
        l2ctl->prefetch.args[0] = 0x20;
        LOOPIT("nextline 0/+6      ", {
            dummy_work(a);
            a = *(volatile int *)p;
            p += STEP;
            if (i % 4 == 2) l2ctl->prefetch.args[0] = 0;
            else if (i % 4 == 1) l2ctl->prefetch.args[0] = 0x20;
        });
        l2ctl->prefetch.args[0] = 0;
    }

    printf("end\n");
}
