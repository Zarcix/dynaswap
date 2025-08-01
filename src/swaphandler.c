#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistdio.h>
#include <sys/swap.h>
#include <errno.h>
#include <linux/limits.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <byteswap.h>
#include <uuid/uuid.h>
#include <signal.h>

#include "swaphandler.h"
#include "swapheader.h"
#include "dynaswap.h"

struct MemoryChunk* prog_swap = NULL;

/* Helpers */

uint32_t mkswap_get_last_page(int swapFD) {
    struct stat swapst;
    if (fstat(swapFD, &swapst) != 0) {
        perror("mkswap failed: cannot read swap file !! fstat");
        exit(EXIT_FAILURE);
    }

    off_t swap_size = swapst.st_size;
    if (swap_size < SWAP_PAGE_SIZE) {
        perror("mkswap failed: file too small || swap_size");
        exit(EXIT_FAILURE);
    }

    uint32_t last_page = (swap_size / SWAP_PAGE_SIZE) - 1;

    return last_page;
}

/* Func Defs */

void init_dynamic_swap() {
    allocate_swap();
}

void mkswap(int swapFD) {
    struct swap_header_v1_2 swapHeader = {0};
    for (int i = 0; i < sizeof(swapHeader.bootbits); i++) {
        swapHeader.bootbits[i] = '\0';
    }

    uint32_t swapver[1] = { htole32(SWAP_VERSION) };
    uint32_t swapLastPage = mkswap_get_last_page(swapFD);
    uint32_t lastPage[1] = { htole32(swapLastPage) };
    uint32_t badPages[1] = { htole32(0) };
    uuid_generate(swapHeader.uuid);

    write(swapFD, swapHeader.bootbits, sizeof(swapHeader.bootbits));
    write(swapFD, swapver, sizeof(uint32_t));
    write(swapFD, lastPage, sizeof(uint32_t));
    write(swapFD, badPages, sizeof(uint32_t));
    write(swapFD, swapHeader.uuid, sizeof(swapHeader.uuid));
    write(swapFD, swapHeader.volume_name, sizeof(swapHeader.volume_name));

    lseek(swapFD, SWAP_SIGNATURE_OFFSET, SEEK_SET);
    write(swapFD, SWAP_SIGNATURE, SWAP_SIGNATURE_SZ);
}

void allocate_swap() {
    int new_chunk = 0;
    if (prog_swap != NULL) {
        new_chunk = prog_swap->chunk_number + 1;
    }

    if (new_chunk >= SWAP_MAX_COUNT) {
        perror("Maximum swaps allocated. Cannot allocate any more swaps.");
        return;
    }

    char fileName[PATH_MAX];
    snprintf(fileName, sizeof(fileName), "%s/Chunk%d", SWAP_PATH, new_chunk);
    char* filePath = strdup(fileName);

    FILE* swapFile = fopen(filePath, "w+");
    int swapFD = fileno(swapFile);

    fallocate(swapFD, 0, 0, SWAP_SIZE);
    mkswap(swapFD);

    fclose(swapFile);

    errno = 0;
    int swapon_stat = swapon(filePath, SWAP_FLAG_PREFER);
    if (swapon_stat != 0) {
        printf("Swap allocation ran into an error: %d\n", errno);
    }

    struct MemoryChunk* swap_chunk = malloc(sizeof(struct MemoryChunk));
    swap_chunk->chunk_number = new_chunk;
    swap_chunk->previous_chunk = prog_swap;
    swap_chunk->file_path = filePath;

    prog_swap = swap_chunk;
}

void free_swap() {
    if (prog_swap == NULL) {
        return;
    }

    struct MemoryChunk* current_chunk = prog_swap;

    errno = 0;
    int swapoff_stat = swapoff(current_chunk->file_path);
    if (swapoff_stat != 0) {
        printf("Swap Freeing ran into an error: %d\n", errno);
    }

    errno = 0;
    int rm_stat = remove(current_chunk->file_path);
    if (rm_stat != 0) {
        printf("Removing swap file ran into an error: %d\n", errno);
    }

    struct MemoryChunk* previous_chunk = current_chunk->previous_chunk;

    prog_swap = previous_chunk;

    free(current_chunk->file_path);
    free(current_chunk);
}

void free_dynamic_swap() {
    while (prog_swap != NULL) {
        free_swap();
    }
}