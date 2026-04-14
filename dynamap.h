#ifndef DYNAMAP_H
#define DYNAMAP_H

#define DYNAMAP_CHUNK_SIZE (1 * 1024 * 1024) // 1MB
#define SLOTS_PER_CHUNK (DYNAMAP_CHUNK_SIZE / PAGE_SIZE)

#define DEFLATE_THRESHOLD 0.5
#define DEFLATE_AMOUNT 0.7

#include <linux/fs.h>
#include <linux/xarray.h>
#include <linux/llist.h>
#include <linux/types.h>

struct slot_entry {
    unsigned long index;
    struct llist_node node;
};


struct dynamap_ctx {
    struct file *backing_file;

    struct xarray xa_virt_to_phys;
    struct xarray xa_phys_to_virt;

    struct llist_head free_slots;
    unsigned long total_slots;
    atomic_t active_slots;
    
    struct mutex lock;
};

// Helpers
void dynaswap_shrink(struct dynamap_ctx *ctx); // The "Two-Pointer" Compaction logic
int dynaswap_move_block(struct dynamap_ctx *ctx, unsigned long old_slot, unsigned long new_slot);
unsigned long dynaswap_acquire_slot(struct dynamap_ctx *ctx, unsigned int page_idx);
int dynaswap_extend(struct dynamap_ctx *ctx);

// IO Operations
void dynaswap_write(struct dynamap_ctx *ctx, sector_t sector, struct page *page);
void dynaswap_read(struct dynamap_ctx *ctx, sector_t sector, struct page *page);
void dynaswap_discard(struct dynamap_ctx *ctx, sector_t sector_start, sector_t sector_end);

// Setup/Teardown
int dynamap_init(struct dynamap_ctx *ctx, const char *path);
void dynamap_cleanup(struct dynamap_ctx *ctx);


#endif