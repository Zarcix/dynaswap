#ifndef DYNAMAP_H
#define DYNAMAP_H

#define ENTRIES_PER_PAGE (PAGE_SIZE / sizeof(unsigned long))
#define SECTOR_INDEX_SHIFT (PAGE_SHIFT - SECTOR_SHIFT)

#define SECTOR_SIZE  512

struct dynamap_ctx {
    struct file *backing_file;

    unsigned long **virt_to_phys_map;
    unsigned long **phys_to_virt_map;
    unsigned long map_entries;

    unsigned long *slot_bitmap;
    unsigned long slot_hint;
    unsigned long slot_max_count;

    atomic_long_t total_slots;
    atomic_long_t active_slots;

    struct workqueue_struct *wq;
    struct work_struct extend_work;

    struct rw_semaphore map_rwsem;
};

// Workqueue Operations
void dynaswap_extend(struct work_struct *work);

// IO Operations
blk_status_t dynaswap_write(struct dynamap_ctx *ctx, sector_t sector, struct page *page);
blk_status_t dynaswap_read(struct dynamap_ctx *ctx, sector_t sector, struct page *page);
blk_status_t dynaswap_discard(struct dynamap_ctx *ctx, sector_t sector_start, sector_t sector_end);

// Setup/Teardown
int dynamap_init(struct dynamap_ctx *ctx, const char *path, size_t total_capacity);
void dynamap_cleanup(struct dynamap_ctx *ctx);


#endif