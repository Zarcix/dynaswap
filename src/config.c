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

static void print_usage() {
    printf(
        "Usage: dynaswap -c <config_file>\n"
        "Options:\n"
        "\t-c, --config_file FILE   Path to the configuration file (required)\n"
        "\t-h, --help               Show this help message and exit\n"
    );
}

/* Arg Helper Functions */

static void parse_args(int argc, char* argv[]) {
    int ch;
    while ((ch = getopt_long(argc, argv, "hc:", long_options, NULL)) != -1) {
        switch (ch) {
            case 'c': {
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

static bool validate_args() {
    bool valid = true;
    if (access(prog_args.conf_file, R_OK) != 0) {
        fprintf(stderr, "Error: Cannot access config file '%s': %s\n", optarg, strerror(errno));
        valid = false;
    }
    return valid;
}

/* Config Helper Functions */

static void parse_config_float(config_t* conf, const char* key, double* out) {
    if (!config_lookup_float(conf, key, out)) {
        fprintf(stderr, "Missing float config key: %s\n", key);
        exit(EXIT_FAILURE);
    }
}

static void parse_config_int64(config_t* conf, const char* key, long long* out) {
    if (!config_lookup_int64(conf, key, out)) {
        fprintf(stderr, "Missing int64 config key: %s\n", key);
        exit(EXIT_FAILURE);
    }
}

static void parse_config_string(config_t* conf, const char* key, char **out) {
    const char* val;
    if (!config_lookup_string(conf, key, &val)) {
        fprintf(stderr, "Missing string config key: %s\n", key);
        exit(EXIT_FAILURE);
    }
    *out = strdup(val);
}

static unsigned long long parse_size_string(const char* str) {
    char* end;
    errno = 0;

    double num = strtod(str, &end);
    if (errno != 0 || end == str) {
        fprintf(stderr, "Invalid size format: '%s'\n", str);
        exit(EXIT_FAILURE);
    }

    while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r' || *end == '\v' || *end == '\f') {
        end++;
    }

    unsigned long long multiplier = 1;
    switch (*end) {
        case 'G': case 'g': {
            multiplier = 1024ULL * 1024 * 1024;
            break;
        }
        case 'M': case 'm': {
            multiplier = 1024ULL * 1024;
            break;
        }
        case 'K': case 'k': {
            multiplier = 1024ULL;
            break;
        }
        default: {
            fprintf(stderr, "Unsupported size suffix: '%c'\n", *end);
            exit(EXIT_FAILURE);
            break;
        }
    }

    return (unsigned long long)(num * multiplier);
}

static struct ConfigList parse_config(char* config_path) {
    config_t conf_file;
    struct ConfigList dynaswap_config = {0};

    config_init(&conf_file);
    if (!config_read_file(&conf_file, config_path)) {
        fprintf(stderr, "Failed to read config | %s:%d - %s\n", config_error_file(&conf_file), config_error_line(&conf_file), config_error_text(&conf_file));
        config_destroy(&conf_file);
        exit(EXIT_FAILURE);
    }

    parse_config_string(&conf_file, SWAP_PATH_KEY, &dynaswap_config.swap_path);
    parse_config_string(&conf_file, SWAP_PART_SIZE_KEY, &dynaswap_config.swap_size);

    parse_config_float(&conf_file, SWAP_FULL_THRESHOLD_KEY, &dynaswap_config.swap_full_threshold);
    parse_config_float(&conf_file, SWAP_FREE_THRESHOLD_KEY, &dynaswap_config.swap_free_threshold);
    
    parse_config_int64(&conf_file, PSI_SOME_STRESS_KEY, &dynaswap_config.psi_some_stress);
    parse_config_int64(&conf_file, PSI_FULL_STRESS_KEY, &dynaswap_config.psi_full_stress);

    config_destroy(&conf_file);

    return dynaswap_config;
}

static bool validate_config(struct ConfigList conf) {
    bool valid = true;

    if (conf.swap_full_threshold > 0 && conf.swap_full_threshold <= 1.0) {
        SWAP_FULL_THRESHOLD = conf.swap_full_threshold;
    } else {
        fprintf(stderr, "Error: Invalid swap_full_threshold: %f\n", conf.swap_full_threshold);
        valid = false;
    }

    if (conf.swap_free_threshold > 0 && conf.swap_free_threshold <= 1.0) {
        SWAP_FREE_THRESHOLD = conf.swap_free_threshold;
    } else {
        fprintf(stderr, "Error: Invalid swap_free_threshold: %f\n", conf.swap_free_threshold);
        valid = false;
    }

    if (conf.psi_some_stress > 0 && conf.psi_full_stress > 0) {
        PSI_SOME_STRESS = conf.psi_some_stress;
        PSI_FULL_STRESS = conf.psi_full_stress;
    } else {
        fprintf(stderr, "Error: PSI stress values must be non-negative\n");
        valid = false;
    }

    struct stat st;
    if (stat(conf.swap_path, &st) == 0 && S_ISDIR(st.st_mode)) {
        SWAP_PATH = conf.swap_path;
    } else {
        fprintf(stderr, "Error: '%s' is not a valid directory\n", conf.swap_path);
        valid = false;
    }

    SWAP_SIZE = parse_size_string(conf.swap_size);

    return valid;
}

//-- Main Functions --//

void read_args(int argc, char** argv) {
    parse_args(argc, argv);
    if (!validate_args()) {
        exit(EXIT_FAILURE);
    }

    log_debug(
        "Arguments Passed\n"
    
        "\tConfig File Path: %s\n",
        prog_args.conf_file
    );
}

void read_config(char* config_path) {
    struct ConfigList conf = parse_config(config_path);
    bool conf_valid = validate_config(conf);
    if (!conf_valid) {
        exit(EXIT_FAILURE);
    }

    log_debug(
        "Configuration Settings\n"
    
        "\tSWAP_FULL_THRESHOLD_KEY: %f\n"
        "\tSWAP_FREE_THRESHOLD_KEY: %f\n"
        "\tPSI_FULL_STRESS_KEY: %lld\n"
        "\tPSI_SOME_STRESS_KEY: %lld\n"
        "\tSWAP_PATH_KEY: %s\n"
        "\tSWAP_SIZE: %llu\n",
        SWAP_FULL_THRESHOLD,
        SWAP_FREE_THRESHOLD,
        PSI_SOME_STRESS,
        PSI_FULL_STRESS,
        SWAP_PATH,
        SWAP_SIZE
    );
}
