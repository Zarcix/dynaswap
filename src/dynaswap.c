#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "dynaswap.h"
#include "sysstate.h"
#include "psi.h"
#include "swaphandler.h"

const char* SWAP_PATH = "";

double SWAP_FULL_THRESHOLD;
double SWAP_FREE_THRESHOLD;

long long PSI_SOME_STRESS;
long long PSI_FULL_STRESS;

void alloc_dynaswap(struct PSIMetrics *metrics) {
    #ifdef DEBUG
        printf("- High Memory Usage\n");
    #endif
    float memory_usage, swap_usage;
    swap_usage = poll_swap_usage();
    memory_usage = poll_mem_usage();

    // Don't allocate more swap if swap is not really being used
    if (swap_usage == swap_usage && swap_usage < SWAP_FULL_THRESHOLD) {
        return;
    }

    #ifdef DEBUG
        printf("\tAllocating Swap\n");
    #endif

    allocate_swap();
}

void free_dynaswap(struct PSIMetrics *metrics) {
    #ifdef DEBUG
        printf("- Low Memory Usage\n");
    #endif

    // Always keep some swap to not fully stall the computer
    if (prog_swap->chunk_number == 0) {
        #ifdef DEBUG
            printf("\t0th Chunk Number, returning");
        #endif
        return;
    }

    // Only free if there is no stress on the system
    if (metrics->some_avg60 >= PSI_SOME_STRESS || metrics->full_avg60 >= PSI_FULL_STRESS) {
        #ifdef DEBUG
            printf("\tStress values not high enough, returning");
        #endif
        return;
    }

    float memory_usage, swap_usage;

    swap_usage = poll_swap_usage();
    if (swap_usage != swap_usage || swap_usage > SWAP_FREE_THRESHOLD) {
        return;
    }

    memory_usage = poll_mem_usage();
    #ifdef DEBUG
        printf("\tFreeing Swap\n");
    #endif

    free_swap();
}

void dynaswap() {
    enum PSIPollStatus poll_stat = poll_psi();
    struct PSIMetrics metrics = read_psi();

    #ifdef DEBUG
        printf("PSI Metrics:\n");
        printf("\tsome avg10 = %.2f\n", metrics.some_avg10);
        printf("\tsome avg60 = %.2f\n", metrics.some_avg60);
        printf("\tsome avg300 = %.2f\n", metrics.some_avg300);
        printf("\tfull avg10 = %.2f\n", metrics.full_avg10);
        printf("\tfull avg60 = %.2f\n", metrics.full_avg60);
        printf("\tfull avg300 = %.2f\n", metrics.full_avg300);
    #endif

    switch (poll_stat) {
        case STRAINED: {
            alloc_dynaswap(&metrics);
            break;
        }
        case RELAXED: {
            free_dynaswap(&metrics);
            break;
        }
    }

    #ifdef DEBUG
        printf("\n\n");
    #endif
}

void init_dynaswap(int argc, char** argv) {
    // Prereq Init
    if (getuid()) {
        printf("Error: This program must be run as root.\n");
        exit(EXIT_FAILURE);
    }

    #ifdef DEBUG
        printf("Parsing Args and Config File\n");
    #endif

    parse_args(argc, argv);
    parse_config(prog_args.conf_file);

    // Actual Program Init
    #ifdef DEBUG
        printf("Initializing program\n");
    #endif

    init_direct_memory();
    init_psi();
    init_dynamic_swap();
}

void takedown_dynaswap() {
    free_direct_memory();
    free_psi();
    free_dynamic_swap();
}

void sig_handler(int signal) {
    #ifdef DEBUG
        printf("\nCleaning up before SIGINT\n");
    #endif

    takedown_dynaswap();
    exit(0);
}

int main(int argc, char** argv) {
    #ifdef DEBUG
        printf("Debugging Enabled\n");
    #endif
    init_dynaswap(argc, argv);
    signal(SIGINT, sig_handler);

    while (true) {
        dynaswap();
    }

    takedown_dynaswap();
    return 0;
}