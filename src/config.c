#include <libconfig.h>
#include <stdbool.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include "constants.h"
#include "config.h"

struct Args prog_args = {0};

static struct option long_options[] = {
    {"config_file", required_argument, 0, 'c'},
    {"help", no_argument, 0, 'h'},
    {NULL, 0, NULL, 0}
};

void print_usage() {
    printf(
        "Usage: dynaswap -c <config_file>\n"
        "Options:\n"
        "\t-c, --config_file FILE   Path to the configuration file (required)\n"
        "\t-h, --help               Show this help message and exit\n"
    );
}

//-- Main Functions --//
void parse_config(char* config_path) {
    config_t conf_file;
    config_init(&conf_file);

    if (!config_read_file(&conf_file, config_path)) {
        fprintf(stderr, "Failed to read config | %s:%d - %s\n", config_error_file(&conf_file), config_error_line(&conf_file), config_error_text(&conf_file));
        config_destroy(&conf_file);
        exit(EXIT_FAILURE);
    }

    if (!config_lookup_float(&conf_file, SWAP_FULL_THRESHOLD_KEY, &SWAP_FULL_THRESHOLD)) {
        fprintf(stderr, "Could not read 'SWAP_FULLTHRESHOLD_KEY' from configuration file\n");
        config_destroy(&conf_file);
        exit(EXIT_FAILURE);
    }

    if (!config_lookup_float(&conf_file, SWAP_FREE_THRESHOLD_KEY, &SWAP_FREE_THRESHOLD)) {
        fprintf(stderr, "Could not read 'SWAP_FREE_THRESHOLD_KEY' from configuration file\n");
        config_destroy(&conf_file);
        exit(EXIT_FAILURE);
    }

    if (!config_lookup_int64(&conf_file, PSI_SOME_STRESS_KEY, &PSI_SOME_STRESS)) {
        fprintf(stderr, "Could not read 'PSI_SOME_STRESS_KEY' from configuration file\n");
        config_destroy(&conf_file);
        exit(EXIT_FAILURE);
    }
    if (!config_lookup_int64(&conf_file, PSI_FULL_STRESS_KEY, &PSI_FULL_STRESS)) {
        fprintf(stderr, "Could not read 'PSI_FULL_STRESS_KEY' from configuration file\n");
        config_destroy(&conf_file);
        exit(EXIT_FAILURE);
    }
    if (!config_lookup_string(&conf_file, SWAP_PATH_KEY, &SWAP_PATH)) {
        fprintf(stderr, "Could not read 'SWAP_PATH_KEY' from configuration file\n");
        config_destroy(&conf_file);
        exit(EXIT_FAILURE);
    } else {
        struct stat st;
        if (stat(SWAP_PATH, &st) != 0 || !S_ISDIR(st.st_mode)) {
                fprintf(stderr, "Error: '%s' is not a valid directory\n", SWAP_PATH);
                config_destroy(&conf_file);
                exit(EXIT_FAILURE);
        }
        SWAP_PATH = strdup(SWAP_PATH);
    }

    config_destroy(&conf_file);

    log_debug(
        "Configuration Settings\n"
        "\tSWAP_FULL_THRESHOLD_KEY: %f\n"
        "\tSWAP_FREE_THRESHOLD_KEY: %f\n"
        "\tPSI_FULL_STRESS_KEY: %lld\n"
        "\tPSI_SOME_STRESS_KEY: %lld\n"
        "\tSWAP_PATH_KEY: %s\n",
        SWAP_FULL_THRESHOLD,
        SWAP_FREE_THRESHOLD,
        PSI_SOME_STRESS,
        PSI_FULL_STRESS,
        SWAP_PATH
    );
}

void parse_args(int argc, char** argv) {
    int ch;
    while ((ch = getopt_long(argc, argv, "hc:", long_options, NULL)) != -1) {
        switch (ch) {
            case 'c': {
                if (access(optarg, R_OK) != 0) {
                    fprintf(stderr, "Error: Cannot access config file '%s': %s\n", optarg, strerror(errno));
                    exit(EXIT_FAILURE);
                }
                prog_args.conf_file = strdup(optarg);
                break;
            }
            case 'h': {
                print_usage();
                exit(EXIT_SUCCESS);
            }
            case '?': {
                printf("Invalid argument.\n");
                print_usage();
                exit(EXIT_FAILURE);
            }
        }
    }

    if (prog_args.conf_file == NULL) {
        printf("Error: --config_file/-c is required.\n");
        print_usage();
        exit(EXIT_FAILURE);
    }
}