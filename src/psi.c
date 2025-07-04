#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* File Locations */

#define MEM_PRESSURE_FILE       "/proc/pressure/memory"

/* Delay Timings */

#define MEM_TRIG_MS             150
#define MEM_TRACK_MS            500

/* Program Values */

#define POLL_SOURCE_COUNT       1
#define MEM_POLL_IDX            0


struct pollfd psi_fds[POLL_SOURCE_COUNT];

void fatal_error(char* error_msg) {
    perror(error_msg);
    exit(SIGINT);
}

void setup_psi_polling() {
    // Setup up polling for memory
    psi_fds[MEM_POLL_IDX].fd = open(MEM_PRESSURE_FILE, O_RDWR | O_NONBLOCK);
    if (psi_fds[MEM_POLL_IDX].fd < 0) {
        fatal_error("Failed to open " MEM_PRESSURE_FILE);
    }

    psi_fds[MEM_POLL_IDX].events = POLLPRI;

    char mem_trigger[128];
    snprintf(mem_trigger, 128, "some %d %d", MEM_TRIG_MS * 1000, MEM_TRACK_MS * 1000);
    if (write(psi_fds[MEM_POLL_IDX].fd, mem_trigger, strlen(mem_trigger) + 1) < 0) {
        fatal_error("Failed to write " MEM_PRESSURE_FILE);
    }
}