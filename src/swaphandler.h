#ifndef SWAP_HANDLER
#define SWAP_HANDLER

#define SWAP_SIZE 1024 * 1024 * 1024
#define SWAP_PATH "/mnt/mount/Chunk%d"

// Some magic numbers for swap of specific size 1G
#define SWAP_LAST_PAGE 4294902528
#define SWAP_SIGNATURE_OFFSET 4086

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