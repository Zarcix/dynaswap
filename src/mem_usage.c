#include "libproc2/meminfo.h"
#include <stdlib.h>

float poll_mem_usage() {
    struct meminfo_info* meminfo = NULL;
    struct meminfo_result* res = NULL;
    procps_meminfo_new(&meminfo);

    res = procps_meminfo_get(meminfo, MEMINFO_MEM_TOTAL);
    unsigned long total_vram = res->result.ul_int;
    res = procps_meminfo_get(meminfo, MEMINFO_SWAP_TOTAL);
    total_vram += res->result.ul_int;

    res = procps_meminfo_get(meminfo, MEMINFO_MEM_AVAILABLE);
    unsigned long vram_used = res->result.ul_int;
    res = procps_meminfo_get(meminfo, MEMINFO_SWAP_FREE);
    vram_used += res->result.ul_int;

    procps_meminfo_unref(&meminfo);

    return 1.0 - ((float)vram_used / (float)total_vram);
}

float poll_swap_usage() {
    struct meminfo_info* meminfo = NULL;
    struct meminfo_result* res = NULL;
    procps_meminfo_new(&meminfo);

    res = procps_meminfo_get(meminfo, MEMINFO_SWAP_TOTAL);
    unsigned long total_swap = res->result.ul_int;

    res = procps_meminfo_get(meminfo, MEMINFO_SWAP_FREE);
    unsigned long swap_used = res->result.ul_int;

    procps_meminfo_unref(&meminfo);

    return 1.0 - ((float)swap_used / (float)total_swap);
}