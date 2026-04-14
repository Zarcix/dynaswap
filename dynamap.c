#define DEBUG

#include <linux/blkdev.h>
#include "dynamap.h"

#pragma region Helper Functions

int dynaswap_extend(struct dynamap_ctx *ctx) {
    pr_debug("dynaswap: beginning to extend storage\n");
    loff_t current_end = (loff_t)ctx->total_slots * PAGE_SIZE;
    int ret = vfs_fallocate(ctx->backing_file, 0, current_end, DYNAMAP_CHUNK_SIZE);
    if (ret) {
        pr_err("dynamswap: could not fallocate more space (err %d)\n", ret);
        return ret;
    }

    pr_debug("dynaswap: allocation finished, getting slots per chunk\n");
    for (int i = SLOTS_PER_CHUNK; i > 0; i--) {
        struct slot_entry *entry = kmalloc(sizeof(*entry), GFP_NOIO);
        if (!entry) break;

        entry->index = ctx->total_slots + i;
        llist_add(&entry->node, &ctx->free_slots);
    }

    ctx->total_slots += SLOTS_PER_CHUNK;

    pr_info("dynaswap: expanded storage (%lu total slots)\n", ctx->total_slots);
    return 0;
}

unsigned long dynaswap_acquire_slot(struct dynamap_ctx *ctx, unsigned int page_idx) {
    void *entry_ptr = xa_load(&ctx->xa_virt_to_phys, page_idx);

    // 1. Existing mapping check (no change here)
    if (entry_ptr && xa_is_value(entry_ptr)) 
        return xa_to_value(entry_ptr);

    // 2. Proactive Expansion Check (Watermark)
    unsigned long active = (unsigned long)atomic_read(&ctx->active_slots);
    
    /* * Trigger expansion if:
     * a) We have 0 slots (initial state)
     * b) We are 50% full (active >= total / 2)
     * c) We are literally out of pre-allocated llist nodes
     */
    if (ctx->total_slots == 0 || active >= (ctx->total_slots / 2) || llist_empty(&ctx->free_slots)) {
        
        /* Optional: Avoid spamming extend if we just extended but 
           haven't processed enough to drop below 50% again. 
           In a simple driver, dynaswap_extend will just bump total_slots, 
           immediately fixing the 'active >= total/2' condition. */
           
        pr_debug("dynaswap: proactive expansion (active: %lu/%lu)\n", active, ctx->total_slots);
        if (dynaswap_extend(ctx)) {
            // If we fail to extend but still have some free_slots left, we can continue.
            // If we have 0 free_slots AND extend failed, we must bail.
            if (llist_empty(&ctx->free_slots)) {
                pr_err("dynaswap: hard exhaustion, no slots available\n");
                return 0;
            }
        }
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
    xa_store(&ctx->xa_phys_to_virt, p_slot, xa_mk_value(page_idx), GFP_NOIO);
    atomic_inc(&ctx->active_slots);

    return p_slot;
}

#pragma endregion

#pragma region Main Functions

void dynaswap_read(struct dynamap_ctx *ctx, sector_t sector, struct page *page) {
    unsigned long page_index = sector >> (PAGE_SHIFT - 9);

    mutex_lock(&ctx->lock);

    void *entry = xa_load(&ctx->xa_virt_to_phys, page_index);
    if (!entry || !xa_is_value(entry)) {
        // If it isn't a value, just memset the whole thing to 0
        pr_debug("dynaswap: tried to read from an empty page index (idx: %lu)\n", page_index);
        void *vaddr = kmap_local_page(page);
        memset(vaddr, 0, PAGE_SIZE);
        kunmap_local(vaddr);

        mutex_unlock(&ctx->lock);
        return;
    }

    unsigned long p_slot = xa_to_value(entry);
    loff_t bd_offset = (p_slot - 1) * PAGE_SIZE;

    pr_debug("dynaswap: reading from dynaswap (page_idx: %lu, p_slot: %lu, file_offset: %lld)\n",
        page_index, p_slot, bd_offset);

    void *vaddr = kmap_local_page(page);
    kernel_read(ctx->backing_file, vaddr, PAGE_SIZE, &bd_offset);
    kunmap_local(vaddr);

    mutex_unlock(&ctx->lock);
}

void dynaswap_write(struct dynamap_ctx *ctx, sector_t sector, struct page *page) {
    unsigned long page_index = sector >> (PAGE_SHIFT - 9);
    unsigned int flags = memalloc_noio_save();
    mutex_lock(&ctx->lock);

    unsigned long p_slot = dynaswap_acquire_slot(ctx, page_index);
    if (!p_slot) {
        mutex_unlock(&ctx->lock);
        return;
    }

    loff_t bd_offset = (loff_t)(p_slot - 1) * PAGE_SIZE;

    unsigned long active_count = (unsigned long)atomic_read(&ctx->active_slots);

    pr_debug("dynaswap: wrote to dynaswap (page_idx: %lu, bd_offset: %lld, remaining_slots: %lu)\n", page_index, bd_offset, ctx->total_slots - active_count);

    void *page_addr = kmap_local_page(page);
    kernel_write(ctx->backing_file, page_addr, PAGE_SIZE, &bd_offset);
    kunmap_local(page_addr);

    mutex_unlock(&ctx->lock);
    memalloc_noio_restore(flags);
}

void dynaswap_discard(struct dynamap_ctx *ctx, sector_t sector_start, sector_t sector_end) {

}

#pragma endregion

#pragma region Setup and Teardown

#define FILE_OPEN_FLAGS O_RDWR | O_CREAT | O_TRUNC | O_LARGEFILE

int dynamap_init(struct dynamap_ctx *ctx, const char *path) {
    xa_init(&ctx->xa_virt_to_phys);
    xa_init(&ctx->xa_phys_to_virt);

    init_llist_head(&ctx->free_slots);
    ctx->total_slots = 0;
    atomic_set(&ctx->active_slots, 0);

    ctx->backing_file = filp_open(path, FILE_OPEN_FLAGS, 0600);
    if (IS_ERR(ctx->backing_file)) {
        pr_err("dynaswap: Failed to open backing file %s\n", path);
        return PTR_ERR(ctx->backing_file);
    }

    mutex_init(&ctx->lock);

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
    xa_destroy(&ctx->xa_phys_to_virt);
    pr_debug("dynaswap: mapping xarrays destroyed\n");

    struct llist_node *node, *next;
    struct slot_entry *entry;

    node = llist_del_all(&ctx->free_slots);
    llist_for_each_safe(node, next, node) {
        entry = llist_entry(node, struct slot_entry, node);
        kfree(entry);
    }

    pr_debug("dynaswap: free slot list cleared\n");

    ctx->total_slots = 0;
    atomic_set(&ctx->active_slots, 0);

    pr_info("dynaswap: cleanup finished\n");
}

#pragma endregion
