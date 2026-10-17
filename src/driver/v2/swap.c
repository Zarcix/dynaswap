#define DEBUG

#include <linux/slab.h>
#include <linux/blkdev.h>

#include "common.h"
#include "swap.h"

int swap_write(struct swap *swap, sector_t virtual_slot, struct page *page) {
    log_debug("swap write to %llu", virtual_slot);

    void *entry = kmalloc(PAGE_SIZE, GFP_NOIO);
    if (!entry)
        return -ENOMEM;

    void *src = kmap_local_page(page);
    memcpy(entry, src, PAGE_SIZE);
    kunmap_local(src);

    void *old = xa_store(&swap->map, virtual_slot, entry, GFP_NOIO);
    if (xa_is_err(old)) {
        kfree(entry);
        return xa_err(old);
    }

    kfree(old);
    return 0;
}

int swap_read(struct swap *swap, sector_t virtual_slot, struct page *page) {
    log_debug("swap read from %llu", virtual_slot);
    void *entry = xa_load(&swap->map, virtual_slot);

    if (!entry) {
        clear_highpage(page);
        return 0;
    }

    void *dest = kmap_local_page(page);
    memcpy(dest, entry, PAGE_SIZE);
    kunmap_local(dest);

    return 0;
}

int init_swap(struct swap *swap) {
    int status;

    status = init_storage(&swap->store);
    if (status) return status;

    xa_init_flags(&swap->map, GFP_NOIO);

    rwlock_init(&swap->swap_lock);

    log_debug("initialized swap");

    extend_storage(&swap->store);

    return 0;
}

void free_swap(struct swap *swap) {
    unsigned long index;
    void *entry;
    xa_for_each(&swap->map, index, entry) {
        kfree(entry);
        xa_erase(&swap->map, index);
    }

    xa_destroy(&swap->map);

    free_storage(&swap->store);

    log_debug("freed swap");
}