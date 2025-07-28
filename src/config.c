#include <libconfig.h>
#include <signal.h>
#include <stdlib.h>

#include "dynaswap.h"
#include "config.h"

//-- Main Functions --//
void parse_config(char* config_path) {
    config_t conf_file;
    config_init(&conf_file);

    if (!config_read_file(&conf_file, config_path)) {
        fprintf(stderr, "Failed to read config | %s:%d - %s\n", config_error_file(&conf_file), config_error_line(&conf_file), config_error_text(&conf_file));
        config_destroy(&conf_file);
        exit(SIGABRT);
    }

    if (!config_lookup_float(&conf_file, SWAP_FULL_THRESHOLD_KEY, &SWAP_FULL_THRESHOLD)) {
        fprintf(stderr, "Could not read 'SWAP_FULLTHRESHOLD_KEY' from configuration file\n");
        config_destroy(&conf_file);
        exit(SIGABRT);
    }

    if (!config_lookup_float(&conf_file, SWAP_FREE_THRESHOLD_KEY, &SWAP_FREE_THRESHOLD)) {
        fprintf(stderr, "Could not read 'SWAP_FREE_THRESHOLD_KEY' from configuration file\n");
        config_destroy(&conf_file);
        exit(SIGABRT);
    }

    if (!config_lookup_int64(&conf_file, PSI_SOME_STRESS_KEY, &PSI_SOME_STRESS)) {
        fprintf(stderr, "Could not read 'PSI_SOME_STRESS_KEY' from configuration file\n");
        config_destroy(&conf_file);
        exit(SIGABRT);
    }
    if (!config_lookup_int64(&conf_file, PSI_FULL_STRESS_KEY, &PSI_FULL_STRESS)) {
        fprintf(stderr, "Could not read 'PSI_FULL_STRESS_KEY' from configuration file\n");
        config_destroy(&conf_file);
        exit(SIGABRT);
    }

    config_destroy(&conf_file);
}

