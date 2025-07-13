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

    if (memory_usage > MEMORY_FULL_THRESHOLD && (swap_usage > SWAP_USED_THRESHOLD || swap_usage != swap_usage)) {
        printf("Allocating Swap\n");
        printf("Current Memo: %f\n", memory_usage);
        printf("Current Swap: %f\n", swap_usage);
        allocate_swap();
    } else if (swap_usage == swap_usage && (memory_usage < MEMORY_FREE_THRESHOLD || swap_usage < SWAP_FREE_THRESHOLD)) {
        free_swap();
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