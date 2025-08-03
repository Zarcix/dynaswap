#include "constants.h"

// Swap File Modifiers
const char* SWAP_PATH = "/swap";
long long SWAP_SIZE = ((long)1 * 1024 * 1024 * 1024);

// Swap Thresholds
double SWAP_FULL_THRESHOLD = 0.5;
double SWAP_FREE_THRESHOLD = 0.5;

// PSI Stress Thresholds
long long PSI_SOME_STRESS = 10;
long long PSI_FULL_STRESS = 15;
