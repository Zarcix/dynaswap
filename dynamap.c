#define DEBUG
#undef DEBUG

#include <linux/fs.h>
#include <linux/falloc.h>
#include <linux/blkdev.h>

#include "dynamap.h"

#define FILE_OPEN_FLAGS O_DIRECT | O_RDWR | O_CREAT | O_TRUNC | O_LARGEFILE

/* Scaling Constants */
static unsigned char expansion_trigger_percent = 80;
module_param_named(inflate_threshold, expansion_trigger_percent, byte, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(
    inflate_threshold,
    "Percent where DynaSwap's backing file will self inflate (default: 80%)"
);

/* IO Constants */
static unsigned long chunk_size = 256UL;
module_param_named(chunk_size, chunk_size, ulong, S_IRUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(chunk_size, "Size of DynaSwap chunks in bytes (default: 256MB)");

#pragma region Helper Functions

static int dynaswap_should_extend(struct dynamap_ctx *ctx) {
    unsigned long total = atomic_long_read(&ctx->total_slots);
    unsigned long active = atomic_long_read(&ctx->active_slots);

    if (total == 0) {
        return 1;
    }

    unsigned long usage_percent = (active * 100) / total;
    return usage_percent >= expansion_trigger_percent;
}

static void dynaswap_expand_file(struct dynamap_ctx *ctx) {
    struct file *file = ctx->backing_file;

    ulong old_slots = atomic_long_read(&ctx->total_slots);
    ulong inc_size = (chunk_size * 1024 * 1024) >> PAGE_SHIFT;
    ulong new_slots = old_slots + inc_size;

    if (new_slots > ctx->slot_max_count) {
        new_slots = ctx->slot_max_count;
    }

    if (new_slots <= old_slots) return;

    loff_t offset = (loff_t)old_slots << PAGE_SHIFT;
    loff_t len = (loff_t)(new_slots - old_slots) << PAGE_SHIFT;

    int ret = vfs_fallocate(file, 0, offset, len);
    if (ret) return;

    down_write(&ctx->map_rwsem);
    atomic_long_set(&ctx->total_slots, new_slots);
    up_write(&ctx->map_rwsem);

    pr_debug("dynaswap [EXPAND]: Capacity increased to %lu slots\n", new_slots);
}

#pragma endregion

#pragma region Workqueue Functions

void dynaswap_extend(struct work_struct *work) {
    struct dynamap_ctx *ctx = container_of(work, struct dynamap_ctx, extend_work);
    dynaswap_expand_file(ctx);
}

#pragma endregion

#pragma region Main Functions

blk_status_t dynaswap_read(struct dynamap_ctx *ctx, sector_t sector, struct page *page) {
    ulong page_index = sector >> SECTOR_INDEX_SHIFT;
    ulong dir = page_index / ENTRIES_PER_PAGE;
    ulong leaf = page_index % ENTRIES_PER_PAGE;

    down_read(&ctx->map_rwsem);

    ulong p_slot = ctx->virt_to_phys_map[dir][leaf];

    up_read(&ctx->map_rwsem);

    if (p_slot == ~0UL) {
        clear_highpage(page);
        return BLK_STS_OK;
    }

    loff_t block_offset = (loff_t)p_slot << PAGE_SHIFT;
    void *vaddr = kmap_local_page(page);

    ssize_t ret = kernel_read(ctx->backing_file, vaddr, PAGE_SIZE, &block_offset);
    if (unlikely(ret != PAGE_SIZE)) {
        // If read fails, zero the page so we don't return random junk
        memset(vaddr, 0, PAGE_SIZE);
        pr_err("dynaswap [READ]: Failed to read slot (slot=%lu, err=%ld)\n", p_slot, (long)ret);
        kunmap_local(vaddr);
        return BLK_STS_IOERR;
    }

    kunmap_local(vaddr);
    return BLK_STS_OK;
}

blk_status_t dynaswap_write(struct dynamap_ctx *ctx, sector_t sector, struct page *page) {
    ulong page_index = sector >> SECTOR_INDEX_SHIFT;

    ulong dir = page_index / ENTRIES_PER_PAGE;
    ulong leaf = page_index % ENTRIES_PER_PAGE;

    ulong p_slot;

    if (dynaswap_should_extend(ctx)) {
        queue_work(ctx->wq, &ctx->extend_work);
    }

    // Check if the sector has been mapped before
    down_read(&ctx->map_rwsem);

    // If it has been mapped, reuse the p_slot
    if (ctx->virt_to_phys_map[dir][leaf] != ~0UL) {
        p_slot = ctx->virt_to_phys_map[dir][leaf];
        up_read(&ctx->map_rwsem);
        goto disk_write;
    }

    // If the sector has never been mapped before, we make a new slot
    up_read(&ctx->map_rwsem);
    down_write(&ctx->map_rwsem);

    p_slot = ctx->virt_to_phys_map[dir][leaf];
    if (p_slot != ~0UL) {
        up_write(&ctx->map_rwsem);
        goto disk_write;
    }

    // Find the next available slot
    ulong max_slots = atomic_long_read(&ctx->total_slots);
    p_slot = find_next_zero_bit(ctx->slot_bitmap, max_slots, ctx->slot_hint);
    if (p_slot >= max_slots) {
        p_slot = find_next_zero_bit(ctx->slot_bitmap, max_slots, 0);
    }

    if (p_slot >= max_slots) {
        pr_err("dynaswap [WRITE]: Out of slots. (max=%lu, used=%lu)\n", max_slots, atomic_long_read(&ctx->active_slots));
        up_write(&ctx->map_rwsem);
        return BLK_STS_NOSPC;
    }

    // Update the slot to be in use
    set_bit(p_slot, ctx->slot_bitmap);
    ctx->slot_hint = (p_slot + 1) % max_slots;
    ctx->virt_to_phys_map[dir][leaf] = p_slot;
    atomic_long_inc(&ctx->active_slots);

    up_write(&ctx->map_rwsem);

    // Use the slot to write to the backing file
    disk_write:
    loff_t block_offset = (loff_t)p_slot << PAGE_SHIFT;
    void *page_addr = kmap_local_page(page);
    kernel_write(ctx->backing_file, page_addr, PAGE_SIZE, &block_offset);
    kunmap_local(page_addr);
    return BLK_STS_OK;
}

blk_status_t dynaswap_discard(struct dynamap_ctx *ctx, sector_t sector_start, sector_t nr_sectors) {
    ulong start_index = sector_start >> SECTOR_INDEX_SHIFT;
    ulong end_index = (sector_start + nr_sectors) >> SECTOR_INDEX_SHIFT;
    ulong i = start_index;

    pr_debug("dynaswap [DISCARD]: Performing Discard. (start=%lu, end=%lu)\n", start_index, end_index);

    while (i < end_index) {
        // Process in batches of 512 (one full leaf page)
        ulong batch_end = min(i + 512, end_index);

        down_write(&ctx->map_rwsem);
        for (; i < batch_end; i++) {
            ulong dir = i / ENTRIES_PER_PAGE;
            ulong leaf = i % ENTRIES_PER_PAGE;
            
            // Safety check for directory allocation
            if (!ctx->virt_to_phys_map[dir]) continue;

            ulong p_slot = ctx->virt_to_phys_map[dir][leaf];
            if (p_slot == ~0UL) continue;

            // Logical Clear
            ctx->virt_to_phys_map[dir][leaf] = ~0UL;
            clear_bit(p_slot, ctx->slot_bitmap);
            atomic_long_dec(&ctx->active_slots);

            // TODO Physical Clear, move this into a workqueue or something 
            loff_t offset = (loff_t)p_slot << PAGE_SHIFT;
            int ret = vfs_fallocate(ctx->backing_file, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE, offset, PAGE_SIZE);
            if (ret) {
                pr_warn("dynaswap [DISCARD]: fallocate failed at p_slot %lu with error %d\n", p_slot, ret);
            }
        }
        up_write(&ctx->map_rwsem);
        
        // Yield to let other tasks run
        cond_resched();
    }

    return BLK_STS_OK;
}

#pragma endregion

#pragma region Setup and Teardown

int dynamap_init(struct dynamap_ctx *ctx, const char *path, size_t total_capacity) {
    // Backing File Storage
    ctx->backing_file = filp_open(path, FILE_OPEN_FLAGS, 0600);
    if (IS_ERR(ctx->backing_file)) {
        pr_err("dynamap: failed to open %s\n", path);
        return PTR_ERR(ctx->backing_file);
    }

    // Slot Mapping
    unsigned long entries_per_page = PAGE_SIZE / sizeof(unsigned long);
    unsigned long total_pages = total_capacity >> PAGE_SHIFT;
    unsigned long nr_dirs = DIV_ROUND_UP(total_pages, entries_per_page);
    ctx->map_entries = nr_dirs;
    
    ctx->virt_to_phys_map = kvzalloc(nr_dirs * sizeof(unsigned long *), GFP_KERNEL);
    ctx->phys_to_virt_map = kvzalloc(nr_dirs * sizeof(unsigned long *), GFP_KERNEL);
    if (!ctx->virt_to_phys_map || !ctx->phys_to_virt_map)
        goto fail;

    for (unsigned long i = 0; i < nr_dirs; i++) {
        ctx->virt_to_phys_map[i] = (unsigned long *)__get_free_page(GFP_KERNEL);
        ctx->phys_to_virt_map[i] = (unsigned long *)__get_free_page(GFP_KERNEL);

        if (!ctx->virt_to_phys_map[i] || !ctx->phys_to_virt_map[i])
            goto fail;
        
        memset(ctx->virt_to_phys_map[i], 0xFF, PAGE_SIZE);
        memset(ctx->phys_to_virt_map[i], 0xFF, PAGE_SIZE);
    }

    // Slot Bitmapping
    ctx->slot_bitmap = kvzalloc(BITS_TO_LONGS(total_pages) * sizeof(unsigned long), GFP_KERNEL);
    if (!ctx->slot_bitmap)
        goto fail;

    ctx->slot_hint = 0;
    ctx->slot_max_count = total_pages;

    // Actual Disk Mapped Slots
    atomic_long_set(&ctx->total_slots, 0);
    atomic_long_set(&ctx->active_slots, 0);

    // WorkQueues
    ctx->wq = alloc_workqueue("dynamap_wq", WQ_MEM_RECLAIM | WQ_HIGHPRI | WQ_UNBOUND, 0);
    if (!ctx->wq)
        goto fail;

    INIT_WORK(&ctx->extend_work, dynaswap_extend);

    // Semaphores
    init_rwsem(&ctx->map_rwsem);

    // File Initialization
    queue_work(ctx->wq, &ctx->extend_work);

    return 0;

    fail:
        dynamap_cleanup(ctx);
        return -ENOMEM;
}

void dynamap_cleanup(struct dynamap_ctx *ctx) {
    if (!ctx) return;

    // WorkQueues
    cancel_work_sync(&ctx->extend_work);
    cancel_work_sync(&ctx->truncate_work);

    if (ctx->wq) {
        // Should we flush here?
        // flush_workqueue(ctx->wq);
        destroy_workqueue(ctx->wq);
        ctx->wq = NULL;
    }

    // Bitmaps
    if (ctx->slot_bitmap) {
        kvfree(ctx->slot_bitmap);
        ctx->slot_bitmap = NULL;
    }

    // Slot Mapping
    if (ctx->virt_to_phys_map) {
        for (unsigned long i = 0; i < ctx->map_entries; i++) {
            if (ctx->virt_to_phys_map[i]) {
                free_page((unsigned long)ctx->virt_to_phys_map[i]);
            }
        }
        kvfree(ctx->virt_to_phys_map);
        ctx->virt_to_phys_map = NULL;
    }

    if (ctx->phys_to_virt_map) {
        for (unsigned long i = 0; i < ctx->map_entries; i++) {
            if (ctx->phys_to_virt_map[i]) {
                free_page((unsigned long)ctx->phys_to_virt_map[i]);
            }
        }
        kvfree(ctx->phys_to_virt_map);
        ctx->phys_to_virt_map = NULL;
    }

    // Backing File
    if (ctx->backing_file) {
        filp_close(ctx->backing_file, NULL);
    }
}

#pragma endregion
