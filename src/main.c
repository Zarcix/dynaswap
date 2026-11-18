#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "include/log.h"

#include "args.h"
#include "chunk.h"

#define POLL_PATH "/sys/kernel/mm/swap/pages/event"

enum swap_capacity_event {
    SWAP_CAPACITY_EVENT_NONE = 0,
    SWAP_CAPACITY_EVENT_EXTEND = 1,
    SWAP_CAPACITY_EVENT_TRUNC = 2
};

int event_loop() {
    int event_fd = open(POLL_PATH, O_RDONLY | O_NONBLOCK);
    if (event_fd < 0) {
        log_error("failed to open %s for polling", POLL_PATH);
        return 1;
    }

    struct pollfd pfd = {
        .fd = event_fd,
        .events = POLLPRI | POLLERR,
    };

    char buf[128];
    while (poll(&pfd, 1, -1)) {
        if (pfd.revents & (POLLPRI | POLLERR)) {
            pread(event_fd, buf, sizeof(buf) - 1, 0);
            char *end;
            enum swap_capacity_event event = strtol(buf, &end, 10);

            if (end == buf) {
                continue;
            }

            switch (event) {
                case 0: {
                    log_debug("seeing some system load");
                    break;
                }
                case 1: {
                    log_debug("seeing high system load");
                    expand_swap();
                    break;
                }
                case 2: {
                    log_debug("seeing low system load");
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

    close(event_fd);
    return 0;
}

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

int main(int argc, const char **argv) {
    parse_args(argc, argv);
    setup_signals();
    init_swap();

    event_loop();
    return 0;
}