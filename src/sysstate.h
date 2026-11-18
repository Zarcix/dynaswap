#ifndef SYSSTATE_H
#define SYSSTATE_H

/* Variables */

// PSI
#include <sys/poll.h>
#define PSI_FILE "/proc/pressure/memory"
#define PSI_SOME_AVG10 700000 // 7% pressure | .7s / 10s
#define PSI_SOME_AVG60 1000000 // 1.6% pressure | 1s / 60s
#define PSI_TIMEOUT 5000

// Mem

/* System State Structs */
struct MemState {
    struct meminfo_info* meminfo;
};

enum PSIPollStatus {
    STRAINED,
    RELAXED
};

struct PSIState {
    struct pollfd pfd;
};

struct PSIMetrics {
    float some_avg10;
    float some_avg60;
    float some_avg300;

    float full_avg10;
    float full_avg60;
    float full_avg300;
};

/* Direct Memory Fns */
void init_direct_memory();
float poll_mem_usage();
float poll_swap_usage();
void free_direct_memory();

/* PSI Fns*/
void init_psi();
enum PSIPollStatus poll_psi();
struct PSIMetrics read_psi();
void free_psi();


#endif