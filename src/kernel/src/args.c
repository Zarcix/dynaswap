#include <stdbool.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "include/argparse.h"
#include "include/log.h"

#include "args.h"

const char *swap_path = NULL;
int swap_size = 512;

static bool validate_swap_path() {
    struct stat st;

    if (!swap_path) {
        log_error("Missing required option --path (-p)");
        return false;
    }

    if (stat(swap_path, &st) != 0) {
        log_error("Path '%s' does not exist", swap_path);
        return false;
    }

    if (!S_ISDIR(st.st_mode)) {
        log_error("Path '%s' must be a directory", swap_path);
        return false;
    }

    return true;
}

static bool validate_swap_size() {
    if (swap_size <= 0) {
        log_error("Swap size of '%d' is invalid", swap_size);
        return false;
    }
    return true;
}

void parse_args(int argc, const char **argv) {
    struct argparse_option options[] = {
        OPT_HELP(),
        OPT_STRING('p', "path", &swap_path, "path for swap files to be under"),
        OPT_INTEGER('s', "size", &swap_size, "size of a swap file in MB (default = 512M)"),
        OPT_END(),
    };

    struct argparse argparse;
    argparse_init(&argparse, options, NULL, 0);
    argc = argparse_parse(&argparse, argc, argv);

    if (!validate_swap_path()) {
        argparse_usage(&argparse);
        exit(1);
    }

    if (!validate_swap_size()) {
        argparse_usage(&argparse);
        exit(1);
    }
}