#define DEBUG

#include <linux/blkdev.h>
#include "dynamap.h"

#define FILE_OPEN_FLAGS O_DIRECT | O_RDWR | O_CREAT | O_TRUNC | O_LARGEFILE

#pragma region DynaSwap Parameters

#pragma region Scaling Thresholds

/* Scaling Constants */
static u8 inflate_threshold_percent = 80;
static u8 deflate_threshold_percent = 30;

module_param_named(inflate_threshold, inflate_threshold_percent, byte, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(
    inflate_threshold,
    "Percent where DynaSwap's backing file will self inflate (default: 80%)"
);
module_param_named(deflate_threshold, deflate_threshold_percent, byte, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(
    deflate_threshold,
    "Percent where DynaSwap backing file will self deflate (default: 50%)"
);

#pragma endregion

#pragma region Scaling Behavior

/* IO Constants */
static u8 deflate_amount_percent = 70;

module_param_named(deflate_target, deflate_amount_percent, byte, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(
    deflate_target,
    "The target utilization percentage to reach after a deflation event (default 70%)"
);

#pragma endregion

#pragma region Memory Layout

#define DYNAMAP_DEFAULT_CHUNK_SIZE (256 * 1024 * 1024)

static u32 chunk_size = DYNAMAP_DEFAULT_CHUNK_SIZE;
module_param_named(chunk_size, chunk_size, uint, S_IRUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(chunk_size, "Size of DynaSwap chunks in bytes (default: 256MB)");

static u32 slots_per_chunk;

#pragma endregion

#pragma endregion

#pragma region Helper Functions

/**
 * This function returns true when one of three conditions are true.
 * 1. The total amount of slots is 0
 * 2. There are no more free slots left
 * 3. When the usage percent of slots is at or above the INFLATE_THRESHOLD_PERCENT
 */
static int dynaswap_should_extend(struct dynamap_ctx *ctx) {
    unsigned long total = atomic_long_read(&ctx->total_slots);
    unsigned long active = atomic_long_read(&ctx->active_slots);

    if (total == 0 || llist_empty(&ctx->free_slots)) {
        return 1;
    }

    unsigned long usage_percent = (active * 100) / total;
    return usage_percent >= inflate_threshold_percent;
}

static int dynaswap_extend(struct dynamap_ctx *ctx) {
    int ret = 0;

    if (test_and_set_bit(DYNASWAP_EXTENDING, &ctx->flags)) {
        return 0;
    }

    pr_debug("dynaswap: beginning to extend storage\n");
    unsigned long slot_count = atomic_long_read(&ctx->total_slots);
    loff_t current_end = (loff_t)slot_count * PAGE_SIZE;

    ret = vfs_fallocate(ctx->backing_file, 0, current_end, chunk_size);
    if (ret) {
        pr_err("dynamswap: could not fallocate space (err: %d)\n", ret);
        goto release_bit;
    }

    down_write(&ctx->mapping_rwsem);

    if (atomic_long_read(&ctx->total_slots) > slot_count) {
        goto unlock;
    }

    for (int i = slots_per_chunk; i > 0; i--) {
        struct slot_entry *entry = kmalloc(sizeof(*entry), GFP_NOIO);
        if (!entry) break;

        entry->index = slot_count + i;
        llist_add(&entry->node, &ctx->free_slots);
    }

    atomic_long_add(slots_per_chunk, &ctx->total_slots);

    pr_info("dynaswap: expanded storage (%lu total slots)\n", atomic_long_read(&ctx->total_slots));

    unlock:
        up_write(&ctx->mapping_rwsem);
    release_bit:
        clear_bit(DYNASWAP_EXTENDING, &ctx->flags);
        return ret;
}

static unsigned long dynaswap_acquire_slot(struct dynamap_ctx *ctx, unsigned int page_idx) {
    void *entry_ptr = xa_load(&ctx->xa_virt_to_phys, page_idx);

    // 1. Existing mapping check (no change here)
    if (entry_ptr && xa_is_value(entry_ptr)) 
        return xa_to_value(entry_ptr);

    if (dynaswap_should_extend(ctx)) {
        up_read(&ctx->mapping_rwsem);
        dynaswap_extend(ctx);
        down_read(&ctx->mapping_rwsem);
    }

    struct llist_node *node = llist_del_first(&ctx->free_slots);
    if (!node) {
        pr_err("dynaswap: tried writing to an invalid slot");
        return 0;
    }

    struct slot_entry *entry = llist_entry(node, struct slot_entry, node);
    unsigned long p_slot = entry->index;
    kfree(entry);

    xa_store(&ctx->xa_virt_to_phys, page_idx, xa_mk_value(p_slot), GFP_NOIO);
    // xa_store(&ctx->xa_phys_to_virt, p_slot, xa_mk_value(page_idx), GFP_NOIO);
    atomic_long_inc(&ctx->active_slots);

    return p_slot;
}

#pragma endregion

#pragma region Main Functions

void dynaswap_read(struct dynamap_ctx *ctx, sector_t sector, struct page *page) {
    unsigned long page_index = sector >> (PAGE_SHIFT - 9);

    down_read(&ctx->mapping_rwsem);

    void *entry = xa_load(&ctx->xa_virt_to_phys, page_index);
    if (!entry || !xa_is_value(entry)) {
        // If it isn't a value, just memset the whole thing to 0
        pr_debug("dynaswap: tried to read from an empty page index (idx: %lu)\n", page_index);
        void *vaddr = kmap_local_page(page);
        memset(vaddr, 0, PAGE_SIZE);
        kunmap_local(vaddr);
        goto out;
    }

    unsigned long p_slot = xa_to_value(entry);
    loff_t bd_offset = (p_slot - 1) * PAGE_SIZE;

    void *vaddr = kmap_local_page(page);
    kernel_read(ctx->backing_file, vaddr, PAGE_SIZE, &bd_offset);
    kunmap_local(vaddr);

    out:
        up_read(&ctx->mapping_rwsem);
}

void dynaswap_write(struct dynamap_ctx *ctx, sector_t sector, struct page *page) {
    unsigned long page_index = sector >> (PAGE_SHIFT - 9);
    unsigned int flags = memalloc_noio_save();
    down_read(&ctx->mapping_rwsem);

    unsigned long p_slot = dynaswap_acquire_slot(ctx, page_index);
    if (!p_slot) {
        goto out;
    }

    loff_t bd_offset = (loff_t)(p_slot - 1) * PAGE_SIZE;

    unsigned long active_count = atomic_long_read(&ctx->active_slots);
    unsigned long total_slots = atomic_long_read(&ctx->total_slots);

    void *page_addr = kmap_local_page(page);
    kernel_write(ctx->backing_file, page_addr, PAGE_SIZE, &bd_offset);
    kunmap_local(page_addr);

    out:
        up_read(&ctx->mapping_rwsem);
        memalloc_noio_restore(flags);
}

void dynaswap_discard(struct dynamap_ctx *ctx, sector_t sector_start, sector_t sector_end) {
    pr_debug("dynaswap: running discard (idx %llu - idx %llu)", sector_start, sector_end);
}

#pragma endregion

#pragma region Setup and Teardown

int dynamap_init(struct dynamap_ctx *ctx, const char *path) {
    if (!IS_ALIGNED(chunk_size, PAGE_SIZE)) {
        pr_err("dynaswap: chunk_size must be a multiple of PAGE_SIZE (%lu)\n", PAGE_SIZE);
        return -EINVAL;
    }

    slots_per_chunk = chunk_size / PAGE_SIZE;

    xa_init(&ctx->xa_virt_to_phys);
    // xa_init(&ctx->xa_phys_to_virt);

    init_llist_head(&ctx->free_slots);
    atomic_long_set(&ctx->total_slots, 0);
    atomic_long_set(&ctx->active_slots, 0);

    ctx->backing_file = filp_open(path, FILE_OPEN_FLAGS, 0600);
    if (IS_ERR(ctx->backing_file)) {
        int err = (int)PTR_ERR(ctx->backing_file);
        pr_err("dynaswap: Failed to open backing file %s (error %d)\n", path, err);
        return err;
    }

    init_rwsem(&ctx->mapping_rwsem);

    pr_info("dynaswap: initialization of dynaswap backing finished\n");
    return 0;
}

void dynamap_cleanup(struct dynamap_ctx *ctx) {
    if (!ctx) return;

    pr_info("dynaswap: starting cleanup of backing storage\n");

    if (ctx->backing_file && !IS_ERR(ctx->backing_file)) {
        filp_close(ctx->backing_file, NULL);
        ctx->backing_file = NULL;

        pr_debug("dynaswap: backing file closed\n");
    }

    xa_destroy(&ctx->xa_virt_to_phys);
    // xa_destroy(&ctx->xa_phys_to_virt);
    pr_debug("dynaswap: mapping xarrays destroyed\n");

    struct llist_node *node, *next;
    struct slot_entry *entry;

    node = llist_del_all(&ctx->free_slots);
    llist_for_each_safe(node, next, node) {
        entry = llist_entry(node, struct slot_entry, node);
        kfree(entry);
    }

    pr_debug("dynaswap: free slot list cleared\n");

    atomic_long_set(&ctx->total_slots, 0);
    atomic_long_set(&ctx->active_slots, 0);

    pr_info("dynaswap: cleanup finished\n");
}

#pragma endregion
