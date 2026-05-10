#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <libproc2/meminfo.h>

#include "sysstate.h"
#include "constants.h"

struct MemState direct_memory_state;
struct PSIState psi_state;

/* Direct Memory */
void init_direct_memory() {
    procps_meminfo_new(&direct_memory_state.meminfo);
}

float poll_mem_usage() {
    struct meminfo_info* meminfo = direct_memory_state.meminfo;
    struct meminfo_result* res = NULL;

    res = procps_meminfo_get(meminfo, MEMINFO_MEM_TOTAL);
    unsigned long total_vram = res->result.ul_int;
    res = procps_meminfo_get(meminfo, MEMINFO_SWAP_TOTAL);
    total_vram += res->result.ul_int;

    res = procps_meminfo_get(meminfo, MEMINFO_MEM_AVAILABLE);
    unsigned long vram_used = res->result.ul_int;
    res = procps_meminfo_get(meminfo, MEMINFO_SWAP_FREE);
    vram_used += res->result.ul_int;

    return 1.0 - ((float)vram_used / (float)total_vram);
}

float poll_swap_usage() {
    struct meminfo_info* meminfo = direct_memory_state.meminfo;
    struct meminfo_result* res = NULL;

    res = procps_meminfo_get(meminfo, MEMINFO_SWAP_TOTAL);
    unsigned long total_swap = res->result.ul_int;

    res = procps_meminfo_get(meminfo, MEMINFO_SWAP_FREE);
    unsigned long swap_used = res->result.ul_int;

    return 1.0 - ((float)swap_used / (float)total_swap);
}

void free_direct_memory() {
    procps_meminfo_unref(&direct_memory_state.meminfo);
}

/* PSI */
void init_psi() {
    unsigned char trigger_max = 128;
    char psi_trigger[128];
    snprintf(psi_trigger, trigger_max, "some %d %d", PSI_SOME_AVG10, PSI_SOME_AVG60);

    psi_state.pfd.fd = open("/proc/pressure/memory", O_RDWR);
    if (psi_state.pfd.fd < 0) {
            fprintf(stderr, "/proc/pressure/memory open error: %s\n", strerror(errno));
            return;
    }
    psi_state.pfd.events = POLLPRI;

    if (write(psi_state.pfd.fd, psi_trigger, strlen(psi_trigger) + 1) < 0) {
            fprintf(stderr, "/proc/pressure/memory write error: %s\n", strerror(errno));
            return;
    }
}

enum PSIPollStatus poll_psi() {
    log_debug("Polling PSI\n");

    if (poll(&psi_state.pfd, 1, PSI_TIMEOUT) < 0) {
        perror("Failed to poll PSI: poll_psi poll()");
        exit(EXIT_FAILURE);
    }

    if (psi_state.pfd.revents & POLLERR) {
        fprintf(stderr, "poll_psi: POLLERR (PSI event source gone)\n");
        exit(EXIT_FAILURE);
    }

    if (psi_state.pfd.revents & POLLPRI) {
        return STRAINED;
    } else {
        return RELAXED;
    }
}

struct PSIMetrics read_psi() {
    int buf_size = 256;
    char buf[buf_size];
    lseek(psi_state.pfd.fd, 0, SEEK_SET);
    ssize_t n = read(psi_state.pfd.fd, buf, buf_size - 1);
    if (n <= 0) {
        perror("Reading PSI Failed");
        exit(EXIT_FAILURE);
    }
    buf[n] = '\0';

    struct PSIMetrics metrics = {0};
    char *someline = strstr(buf, "some");
    if (someline) {
        int stat = sscanf(someline, "some avg10=%f avg60=%f avg300=%f", &metrics.some_avg10, &metrics.some_avg60, &metrics.some_avg300);
        if (stat == EOF || stat < 3) {
            fprintf(stderr, "Failed to read some from PSI, args don't match\n");
        }
    }

    char *fullline = strstr(buf, "full");
    if (fullline) {
        int stat = sscanf(fullline, "full avg10=%f avg60=%f avg300=%f", &metrics.full_avg10, &metrics.full_avg60, &metrics.full_avg300);
        if (stat == EOF || stat < 3) {
            fprintf(stderr, "Failed to read full from PSI, args don't match\n");
        }
    }

    return metrics;
}

void free_psi() {
    close(psi_state.pfd.fd);
}