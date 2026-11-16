#ifndef HANDLE_H
#define HANDLE_H

#include <stdbool.h>

#define SWAP_DIR "/swap"
#define SWAP_SIZE (256 * 1024 * 1024)

struct swap_chunk {
    char *path;
    struct swap_chunk *prev_chunk;
};

void expand_swap(void);
void truncate_swap(bool trunc_all);

void init_swap(void);
void free_swap(void);

#endif