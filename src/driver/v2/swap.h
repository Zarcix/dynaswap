#ifndef SWAP_H
#define SWAP_H

#include <linux/xarray.h>

#include "storage.h"

struct swap {
    struct storage store;
    struct xarray map;
    rwlock_t swap_lock;
};

int swap_write(struct swap *swap, sector_t virtual_slot, struct page *page);
int swap_read(struct swap *swap, sector_t virtual_slot, struct page *page);

int init_swap(struct swap *swap);
void free_swap(struct swap *swap);

#endif