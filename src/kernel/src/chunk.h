#ifndef HANDLE_H
#define HANDLE_H

#include <stdbool.h>

#define MB_ALLOC_SHIFT 20

struct swap_chunk {
    char *path;
    struct swap_chunk *prev_chunk;
};

void expand_swap(void);
void truncate_swap(bool trunc_all);

void init_swap(void);
void free_swap(void);

#endif