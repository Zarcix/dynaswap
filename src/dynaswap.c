#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "constants.h"
#include "config.h"
#include "sysstate.h"
#include "swaphandler.h"

void alloc_dynaswap() {
    log_debug("- " COLOR_BOLD "High" COLOR_OFF " Memory Usage\n");

    float swap_usage;
    swap_usage = poll_swap_usage();

    // Don't allocate more swap if swap is not really being used
    if (swap_usage == swap_usage && swap_usage < SWAP_FULL_THRESHOLD) {
        log_debug("\tSkipping, swap is not full enough\n");
        return;
    }

    log_debug("\tAllocation Swap\n");

    allocate_swap();
}

void free_dynaswap(struct PSIMetrics *metrics) {
    log_debug("- " COLOR_BOLD "Low" COLOR_OFF " Memory Usage\n");

    // Always keep some swap to not fully stall the computer
    if (prog_swap->chunk_number == 0) {
        log_debug("\t0th Chunk Number, returning\n");
        return;
    }

    // 60 second window is used here because we want to assume that long term there is no pressure
    if (metrics->some_avg60 >= PSI_SOME_STRESS || metrics->full_avg60 >= PSI_FULL_STRESS) {
        log_debug("\tStress values not high enough, returning\n");
        return;
    }

    float swap_usage;

    swap_usage = poll_swap_usage();
    if (swap_usage != swap_usage || swap_usage > SWAP_FREE_THRESHOLD) {
        return;
    }

    log_debug("\tFreeing Swap\n");

    free_swap();
}

void dynaswap() {
    enum PSIPollStatus poll_stat = poll_psi();
    struct PSIMetrics metrics = read_psi();

    log_debug(
        "PSI Metrics:\n"
        "\tsome avg10 = %.2f\n"
        "\tsome avg60 = %.2f\n"
        "\tsome avg300 = %.2f\n"
        "\tfull avg10 = %.2f\n"
        "\tfull avg60 = %.2f\n"
        "\tfull avg300 = %.2f\n",
        metrics.some_avg10,
        metrics.some_avg60,
        metrics.some_avg300,
        metrics.full_avg10,
        metrics.full_avg60,
        metrics.full_avg300
    );

    switch (poll_stat) {
        case STRAINED: {
            alloc_dynaswap();
            break;
        }
        case RELAXED: {
            free_dynaswap(&metrics);
            break;
        }
    }
}

void init_dynaswap(int argc, char** argv) {
    // Prereq Init
    if (getuid()) {
        printf("Error: This program must be run as root.\n");
        exit(EXIT_FAILURE);
    }

    log_debug("Parsing Args and Config File\n");

    read_args(argc, argv);
    read_config(prog_args.conf_file);

    // Actual Program Init
    log_debug("Initializing Dynaswap\n");

    init_direct_memory();
    init_psi();
    init_dynamic_swap();

    log_debug("Initialization Finished\n");
}

void takedown_dynaswap() {
    free_direct_memory();
    free_psi();
    free_dynamic_swap();
}

void sig_handler(int signal) {
    log_debug("\nCleaning up before signal `%s`\n", strsignal(signal));

    takedown_dynaswap();
    exit(0);
}

int main(int argc, char** argv) {
    log_debug("Debugging Enabled\n");

    init_dynaswap(argc, argv);
    signal(SIGINT, sig_handler);

    while (true) {
        dynaswap();
    }

    takedown_dynaswap();
    return 0;
}