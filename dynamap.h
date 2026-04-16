#ifndef DYNAMAP_H
#define DYNAMAP_H

#include <linux/fs.h>
#include <linux/xarray.h>
#include <linux/llist.h>
#include <linux/types.h>

#define DYNASWAP_EXTENDING 0

#define BLK_SECTOR_SIZE  512

struct slot_entry {
    struct llist_node node;
    unsigned long index;
    unsigned long virt_page;
};


struct dynamap_ctx {
    struct file *backing_file;

    struct xarray xa_virt_to_phys;

    struct llist_head free_slots;

    atomic_long_t total_slots;
    atomic_long_t active_slots;

    struct rw_semaphore mapping_rwsem;
    unsigned long flags;
};

// IO Operations
void dynaswap_write(struct dynamap_ctx *ctx, sector_t sector, struct page *page);
void dynaswap_read(struct dynamap_ctx *ctx, sector_t sector, struct page *page);
void dynaswap_discard(struct dynamap_ctx *ctx, sector_t sector_start, sector_t sector_end);

// Setup/Teardown
int dynamap_init(struct dynamap_ctx *ctx, const char *path);
void dynamap_cleanup(struct dynamap_ctx *ctx);


#endif