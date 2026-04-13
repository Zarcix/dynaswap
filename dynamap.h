#ifndef DYNAMAP_H
#define DYNAMAP_H

#define DEFLATE_THRESHOLD 0.5
#define DEFLATE_AMOUNT 0.7

#include <linux/fs.h>
#include <linux/xarray.h>
#include <linux/list.h>
#include <linux/types.h>

struct slot_entry {
    unsigned long index;
    struct list_head list;
};


struct dynamap_ctx {
    struct file *backing_file;

    struct xarray xa_virt_to_phys;
    struct xarray xa_phys_to_virt;

    struct list_head free_slots;
    unsigned long total_slots;
    atomic_t active_slots;
    
    struct mutex lock;
};

// Allocation & Metadata
unsigned long dynaswap_get_slot(struct dynamap_ctx *ctx);
void dynaswap_put_slot(struct dynamap_ctx *ctx, unsigned long slot);

// Sizing Logic
void dynaswap_shrink(struct dynamap_ctx *ctx); // The "Two-Pointer" Compaction logic
int dynaswap_move_block(struct dynamap_ctx *ctx, unsigned long old_slot, unsigned long new_slot);

// IO Operations
void dynaswap_write(struct dynamap_ctx *ctx, sector_t sector, struct page *page);
void dynaswap_read(struct dynamap_ctx *ctx, sector_t sector, struct page *page);
void dynaswap_discard(struct dynamap_ctx *ctx, sector_t sector, unsigned int nr_sectors);

// Setup/Teardown
int dynamap_init(struct dynamap_ctx *ctx, const char *path);
void dynamap_cleanup(struct dynamap_ctx *ctx);


#endif