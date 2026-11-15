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
#include <uuid/uuid.h>

#include "swap_handle.h"
#include "include/swapheader.h"

int swap_count = 0; // If we ever decide to multi thread this, this needs to be atomic
struct swap_chunk *current_swap = NULL;

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

void _mkswap(int fd) {
    struct swap_header_v1_2 header = {0};
    for (unsigned int i = 0; i < sizeof(header.bootbits); i++) {
        header.bootbits[i] = '\0';
    }

    uint32_t swapver[1] = { htole32(SWAP_VERSION) };
    uint32_t swapLastPage = mkswap_get_last_page(fd);
    uint32_t lastPage[1] = { htole32(swapLastPage) };
    uint32_t badPages[1] = { htole32(0) };
    uuid_generate(header.uuid);

    write(fd, header.bootbits, sizeof(header.bootbits));
    write(fd, swapver, sizeof(uint32_t));
    write(fd, lastPage, sizeof(uint32_t));
    write(fd, badPages, sizeof(uint32_t));
    write(fd, header.uuid, sizeof(header.uuid));
    write(fd, header.volume_name, sizeof(header.volume_name));

    lseek(fd, SWAP_SIGNATURE_OFFSET, SEEK_SET);
    write(fd, SWAP_SIGNATURE, SWAP_SIGNATURE_SZ);
}

void expand_swap(void) {
    char *path = malloc(sizeof(char) * PATH_MAX);
    if (!path) {
        perror("Failed to allocate memory for path");
        return;
    }

    snprintf(path, PATH_MAX, "%s/chunk_%d", SWAP_DIR, swap_count);

    FILE *file = fopen(path, "w+");
    if (!file) {
        perror("Failed to open swap file");
        free(path);
        return;
    }

    int fd = fileno(file);
    if (fd == -1) {
        perror("Failed to get file descriptor");
        fclose(file);
        free(path);
        return;
    }

    if (fallocate(fd, 0, 0, SWAP_SIZE) != 0) {
        perror("fallocate failed");
        fclose(file);
        free(path);
        return;
    }

    _mkswap(fd);
    fclose(file);

    errno = 0;
    int swapon_stat = swapon(path, SWAP_FLAG_PREFER);
    if (swapon_stat != 0) {
        printf("Swap allocation ran into an error: %d\n", errno);
    }

    struct swap_chunk *chunk = malloc(sizeof(struct swap_chunk));
    chunk->path = path;
    chunk->prev_chunk = current_swap;

    current_swap = chunk;
    swap_count++;
}

void truncate_swap(bool trunc_all) {
    if (current_swap == NULL) {
        return;
    }

    if (!trunc_all && current_swap->prev_chunk == NULL) {
        return;
    }

    errno = 0;
    int swapoff_stat = swapoff(current_swap->path);
    if (swapoff_stat != 0) {
        printf("failed to swapoff: %d\n", swapoff_stat);
    }

    errno = 0;
    int rm_stat = remove(current_swap->path);
    if (rm_stat != 0) {
        printf("failed to remove file :%d\n", errno);
    }

    struct swap_chunk *old = current_swap;
    current_swap = current_swap->prev_chunk;

    free(old->path);
    free(old);

    swap_count--;
}

void init_swap(void) {
    expand_swap();
}

void free_swap(void) {
    while (current_swap != NULL) {
        truncate_swap(true);
    }
}