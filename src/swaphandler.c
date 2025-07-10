#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistdio.h>
#include <sys/swap.h>
#include <errno.h>
#include <linux/limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <byteswap.h>
#include <uuid/uuid.h>

#include "swaphandler.h"
#include "swapheader.h"

void init_dynamic_swap() {
    prog_swap.swap_chunks = NULL;
    prog_swap.chunk_number = 0;
}

void mkswap(int swapFD) {
    struct swap_header_v1_2 swapHeader;

    for (int i = 0; i < sizeof(swapHeader.bootbits); i++) {
        swapHeader.bootbits[i] = '\0';
    }
    write(swapFD, swapHeader.bootbits, sizeof(swapHeader.bootbits));

    uint32_t swapver[1] = { SWAP_VERSION };
    write(swapFD, swapver, sizeof(uint32_t));

    uint32_t lastPage[1] = { __bswap_constant_32(SWAP_LAST_PAGE) };
    write(swapFD, lastPage, sizeof(uint32_t));

    uint32_t badPages[1] = { 0 };
    write(swapFD, badPages, sizeof(uint32_t));

    uuid_generate(swapHeader.uuid);
    write(swapFD, swapHeader.uuid, sizeof(swapHeader.uuid));

    snprintf(swapHeader.volume_name, SWAP_LABEL_LENGTH, "%d", prog_swap.chunk_number);
    write(swapFD, swapHeader.volume_name, sizeof(swapHeader.volume_name));

    lseek(swapFD, SWAP_SIGNATURE_OFFSET, SEEK_SET);
    write(swapFD, SWAP_SIGNATURE, SWAP_SIGNATURE_SZ);
}

void allocate_swap() {
    int new_chunk = prog_swap.chunk_number + 1;
    char fileName[PATH_MAX];
    snprintf(fileName, sizeof(fileName), SWAP_PATH, new_chunk);
    
    char* filePath = malloc(sizeof(char) * strlen(fileName));
    strncpy(filePath, fileName, strlen(fileName));

    FILE* swapFile = fopen(filePath, "w+");
    int swapFD = fileno(swapFile);

    fallocate(swapFD, 0, 0, SWAP_SIZE);
    mkswap(swapFD);

    fclose(swapFile);

    int swapon_stat = swapon(filePath, SWAP_FLAG_PREFER);
    if (swapon_stat != 0) {
        printf("Swap allocation ran into an error: %d\n", errno);
    }

    struct MemoryChunk* swap_info = malloc(sizeof(struct MemoryChunk));
    swap_info->file_path = filePath;
    swap_info->file_handler = NULL;
    swap_info->previous_chunk = prog_swap.swap_chunks;

    prog_swap.chunk_number = new_chunk;
    prog_swap.swap_chunks = swap_info;

}

void free_swap() {

}