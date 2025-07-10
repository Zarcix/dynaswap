#include <stdbool.h>

#include "dynaswap.h"
#include "mem_usage.h"
#include "psi.h"
#include "swaphandler.h"

int main() {
    init_dynamic_swap();

    while (true) {
        float current_usage = poll_mem_usage();
        printf("Above Usage: %f\n", current_usage);
        if (current_usage > USAGE_SWAPPING_THRESHOLD) {
        }
    }
    return 0;
}