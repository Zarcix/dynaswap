#ifndef PROG_CONSTANTS
#define PROG_CONSTANTS

#define DEBUG

#ifdef DEBUG
    #define log_debug(...) fprintf(stdout, __VA_ARGS__)
#else
    #define log_debug(...) ((void)0)
#endif

#define COLOR_BOLD  "\e[1m"
#define COLOR_OFF   "\e[m"

// Swap File Modifiers
extern const char* SWAP_PATH;
extern long long SWAP_SIZE;

// Swap Thresholds
extern double SWAP_FULL_THRESHOLD;
extern double SWAP_FREE_THRESHOLD;

// PSI Stress Thresholds
extern long long PSI_SOME_STRESS;
extern long long PSI_FULL_STRESS;

#endif