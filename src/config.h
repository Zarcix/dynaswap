#ifndef CONFIG
#define CONFIG

#define SWAP_FULL_THRESHOLD_KEY "SWAP_FULL_THRESHOLD"
#define SWAP_FREE_THRESHOLD_KEY "SWAP_FREE_THRESHOLD"
#define PSI_SOME_STRESS_KEY "PSI_SOME_STRESS"
#define PSI_FULL_STRESS_KEY "PSI_FULL_STRESS"
#define SWAP_PATH_KEY "SWAP_PATH"

#define DEBUG

struct Args {
    char* conf_file;
};

extern struct Args prog_args;

void parse_config(char* config_path);
void parse_args(int argc, char** argv);

#endif
