#ifndef CONFIG
#define CONFIG

#define SWAP_PATH_KEY "SWAP_PATH"
#define SWAP_PART_SIZE_KEY "SWAP_PART_SIZE"
#define SWAP_FULL_THRESHOLD_KEY "SWAP_FULL_THRESHOLD"
#define SWAP_FREE_THRESHOLD_KEY "SWAP_FREE_THRESHOLD"
#define PSI_SOME_STRESS_KEY "PSI_SOME_STRESS"
#define PSI_FULL_STRESS_KEY "PSI_FULL_STRESS"

struct Args {
    char* conf_file;
};

extern struct Args prog_args;

struct ConfigList {
    double swap_full_threshold;
    double swap_free_threshold;
    long long psi_some_stress;
    long long psi_full_stress;
    char* swap_path;
    char* swap_size;
};

void read_args(int argc, char** argv);
void read_config(char* config_path);

#endif
