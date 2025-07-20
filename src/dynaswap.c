#include <stdbool.h>

#include "dynaswap.h"
#include "sysstate.h"
#include "psi.h"
#include "swaphandler.h"

void init_dynaswap() {
    init_direct_memory();
    init_psi();
}

void free_dynaswap() {
    free_direct_memory();
    free_psi();
}

void dynaswap() {
    float memory_usage, swap_usage;
    memory_usage = poll_mem_usage();
    swap_usage = poll_swap_usage();

    bool swap_used = swap_usage == swap_usage;
    bool mem_low_swap_low = memory_usage < MEMORY_FREE_THRESHOLD && swap_usage < SWAP_FREE_THRESHOLD && swap_usage == swap_usage;
    bool mem_low_swap_high = memory_usage < MEMORY_FREE_THRESHOLD && swap_usage > SWAP_FULL_THRESHOLD;
    bool mem_high_swap_low = memory_usage > MEMORY_FULL_THRESHOLD && swap_usage < SWAP_FREE_THRESHOLD;
    bool mem_high_swap_high = memory_usage > MEMORY_FULL_THRESHOLD && (swap_usage > SWAP_FULL_THRESHOLD || swap_usage != swap_usage);

    poll_psi();

    if (mem_low_swap_low) {
        printf("Freeing Swap\n");
        printf("Current Memo: %f\n", memory_usage);
        printf("Current Swap: %f\n", swap_usage);
        free_swap();
    } else if (mem_low_swap_high) {
        printf("Freeing Swap 1\n");
        printf("Current Memo: %f\n", memory_usage);
        printf("Current Swap: %f\n", swap_usage);
        free_swap();
    } else if (mem_high_swap_low) {
        printf("Freeing Swap 2\n");
        printf("Current Memo: %f\n", memory_usage);
        printf("Current Swap: %f\n", swap_usage);
        free_swap();
    } else if (mem_high_swap_high) {
        printf("Allocating Swap\n");
        printf("Current Memo: %f\n", memory_usage);
        printf("Current Swap: %f\n", swap_usage);
        allocate_swap();
    }
}

int main() {
    init_dynaswap();

    while (true) {
        dynaswap();
    }

    free_dynaswap();
    return 0;
}