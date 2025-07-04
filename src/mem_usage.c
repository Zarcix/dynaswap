#include "sys/sysinfo.h"

struct sysinfo memInfo;

float poll_mem_usage() {
    sysinfo(&memInfo);

    unsigned long total_mem = memInfo.totalram + memInfo.totalswap;
    unsigned long used_mem = memInfo.totalram + memInfo.totalswap - memInfo.freeram - memInfo.freeswap;

    return (float)used_mem / (float)total_mem;
}