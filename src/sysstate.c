#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libproc2/meminfo.h>

#include "sysstate.h"

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
    int fd = open(PSI_FILE, O_WRONLY);
    if (fd < 0) {
        perror("Failed to start PSI for initialization");
        exit(SIGABRT);
    }

    int trigger_max = 128;
    char psi_trigger[128];
    snprintf(psi_trigger, trigger_max, "some avg10=%d avg60=%d full avg10=%d", PSI_SOME_AVG10, PSI_SOME_AVG60, PSI_FULL_AVG10);
    close(fd);

    int poll_fd = open(PSI_FILE, O_RDONLY);
    if (poll_fd < 0) {
        perror("Failed to init PSI for polling");
        exit(SIGABRT);
    }

    psi_state.pfd.fd = poll_fd;
    psi_state.pfd.events = POLLPRI;
}

void free_psi() {
    close(psi_state.pfd.fd);
}