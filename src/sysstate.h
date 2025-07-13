#ifndef SYSSTATE_H
#define SYSSTATE_H

/* Variables */

// PSI
#include <sys/poll.h>
#define PSI_FILE "/proc/pressure/memory"
#define PSI_SOME_AVG10 50
#define PSI_SOME_AVG60 100
#define PSI_FULL_AVG10 10
// Mem

/* System State Structs */
struct MemState {
    struct meminfo_info* meminfo;
};


struct PSIState {
    struct pollfd pfd;
};

/* Statics */

static struct MemState direct_memory_state;
static struct PSIState psi_state;

/* Direct Memory Fns */
void init_direct_memory();
float poll_mem_usage();
float poll_swap_usage();
void free_direct_memory();

/* PSI Fns*/
void init_psi();
void free_psi();


#endif