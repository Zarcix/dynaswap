#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <unistd.h>

#define POLL_PATH "/sys/kernel/mm/swap/pages/event"

int main(int argc, char **argv) {
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

    if (pread(fd, buf, sizeof(buf) - 1, 0) > 0)
        printf("Initial event: %s\n", buf);

    while (poll(&pfd, 1, -1)) {
        if (pfd.revents & (POLLPRI | POLLERR)) {

            ssize_t bytes = pread(fd, buf, sizeof(buf) - 1, 0);
            if (bytes > 0) {
                buf[bytes] = '\0';
                printf("Read: %s\n", buf);
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