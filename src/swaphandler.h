#ifndef SWAP_HANDLER
#define SWAP_HANDLER

#define SWAP_PAGE_SIZE 4096
#define SWAP_SIZE ((long)2 * 1024 * 1024 * 1024)
#define SWAP_PATH "/mnt/mount/Chunk%d"
#define SWAP_MAX_COUNT 24

#include <stdio.h>
#include <linux/limits.h>

struct MemoryChunk {
    int chunk_number;
    char* file_path;
    struct MemoryChunk* previous_chunk;
};
static struct MemoryChunk* prog_swap;
void init_dynamic_swap();

void allocate_swap();

void free_swap();

#endif