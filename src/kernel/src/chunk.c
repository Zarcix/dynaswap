#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <sys/swap.h>
#include <errno.h>
#include <linux/limits.h>
#include <fcntl.h>
#include <unistd.h>

#include "include/log.h"

#include "chunk.h"
#include "swap.h"

int swap_count = 0; // If we ever decide to multi thread this, this needs to be atomic
struct swap_chunk *current_swap = NULL;

void expand_swap(void) {
    char *path = malloc(sizeof(char) * PATH_MAX);
    if (!path) {
        log_error("cannot expand swap, out of memory");
        return;
    }

    snprintf(path, PATH_MAX, "%s/chunk_%d", SWAP_DIR, swap_count);
    if (create_swap(path, SWAP_SIZE) != 0) {
        log_error("failed to create swap %s", path);
        return;
    }

    errno = 0;
    int swapon_stat = swapon(path, SWAP_FLAG_PREFER);
    if (swapon_stat != 0) {
        log_error("failed to swap on %s", path);
        return;
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

    if (remove_swap(current_swap->path) != 0) {
        log_error("failed to truncate swap %s", current_swap->path);
        return;
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