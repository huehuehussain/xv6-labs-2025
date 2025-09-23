// user/sixfive.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

static const char seps[] = " -\r\t\n./,";

static void
process_fd(int fd)
{
    char c;
    char numbuf[32];
    int idx = 0;
    int digit_only = 1;
    int n;

    while ((n = read(fd, &c, 1)) > 0) {
        if (strchr(seps, c)) {
            if (idx > 0 && digit_only) {
                numbuf[idx] = '\0';
                int val = atoi(numbuf);
                if (val % 5 == 0 || val % 6 == 0) {
                    printf("%d\n", val);    // print numeric value (no leading zeros)
                }
            }
            idx = 0;
            digit_only = 1;
        } else {
            if (idx < (int)sizeof(numbuf) - 1) {
                numbuf[idx++] = c;
            } else {
                /* token too long: keep reading but mark non-digit so it won't be printed */
                digit_only = 0;
            }
            if (c < '0' || c > '9') {
                digit_only = 0;
            }
        }
    }

    /* EOF acts like a separator — flush any trailing token */
    if (idx > 0 && digit_only) {
        numbuf[idx] = '\0';
        int val = atoi(numbuf);
        if (val % 5 == 0 || val % 6 == 0) {
            printf("%d\n", val);
        }
    }
}

int
main(int argc, char *argv[])
{
    int i;

    if (argc < 2) {
        printf("usage: sixfive file\n");
        exit(1);
    }

    for (i = 1; i < argc; i++) {
        int fd = open(argv[i], O_RDONLY);
        if (fd < 0) {
            printf("sixfive: cannot open %s\n", argv[i]);
            continue;   // keep processing the other files
        }
        process_fd(fd);
        close(fd);
    }

    exit(0);
}

