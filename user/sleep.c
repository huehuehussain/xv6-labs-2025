#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    int ticks;
    int i;

    if (argc < 2) {
        printf("usage: sleep n\n");
        exit(1);
    }

    ticks = atoi(argv[1]);
    if (ticks <= 0) {
        printf("sleep: n must be positive\n");
        exit(1);
    }

    for (i = 0; i < ticks; i++) {
        pause(1);
    }

    exit(0);

}

