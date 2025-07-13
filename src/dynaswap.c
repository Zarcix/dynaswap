#include <stdbool.h>

#include "dynaswap.h"
#include "mem_usage.h"
#include "psi.h"
#include "swaphandler.h"

int main() {
    init_dynamic_swap();

    while (true) {
        float current_usage, swap_usage;
        current_usage = poll_mem_usage();
        swap_usage = poll_swap_usage();
        if (current_usage > USAGE_SWAPPING_THRESHOLD && (swap_usage > 0.5 || swap_usage != swap_usage)) {
            printf("Allocating Swap\n");
            printf("Current Usage: %f\n", current_usage);
            printf("Current Swap : %f\n", swap_usage);
            allocate_swap();
        } else if (current_usage < USAGE_FREE_THRESHOLD && swap_usage == swap_usage) {
            printf("Freeing Swap\n");
            printf("Current Usage: %f\n", current_usage);
            printf("Current Swap : %f\n", swap_usage);
            free_swap();
        }
    }
    return 0;
}