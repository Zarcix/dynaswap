#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "swap_handle.h"

#define POLL_PATH "/sys/kernel/mm/swap/pages/event"

void sig_handler(int signal) {
    printf("\nCleaning up before signal `%s`\n", strsignal(signal));

    free_swap();
    exit(0);
}

void setup_signals() {
    signal(SIGINT,  sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGHUP,  sig_handler);
    signal(SIGQUIT, sig_handler);

}

int main(int argc, char **argv) {
    setup_signals();
    int fd = open(POLL_PATH, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        perror("failed to open event path");
        return 1;
    }

    struct pollfd pfd = {
        .fd = fd,
        .events = POLLPRI | POLLERR,
    };

    printf("Dynaswap Starting...\n");

    char buf[128];

    init_swap();

    if (pread(fd, buf, sizeof(buf) - 1, 0) > 0)
        printf("Initial event: %s\n", buf);

    while (poll(&pfd, 1, -1)) {
        if (pfd.revents & (POLLPRI | POLLERR)) {

            pread(fd, buf, sizeof(buf) - 1, 0);
            char *end;
            long event = strtol(buf, &end, 10);

            if (end == buf) {
                continue;
            }

            switch (event) {
                case 0: {
                    printf("No event\n");
                    break;
                }
                case 1: {
                    printf("Expanding Swap\n");
                    expand_swap();
                    break;
                }
                case 2: {
                    printf("Truncating Swap\n");
                    truncate_swap(false);
                    break;
                }
            }
        }
        if (pfd.revents & (POLLHUP | POLLNVAL)) {
            printf("Writer closed the pipe/connection.\n");
            break;
        }
    }

    close(fd);
    return 0;
}