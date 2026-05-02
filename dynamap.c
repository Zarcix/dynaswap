#define DEBUG

#include <linux/blkdev.h>
#include "dynamap.h"

#define FILE_OPEN_FLAGS O_DIRECT | O_RDWR | O_CREAT | O_TRUNC | O_LARGEFILE

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

/* IO Constants */
static u8 deflate_amount_percent = 70;

module_param_named(deflate_target, deflate_amount_percent, byte, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(
    deflate_target,
    "The target utilization percentage to reach after a deflation event (default 70%)"
);

#define DYNAMAP_DEFAULT_CHUNK_SIZE (256 * 1024 * 1024)

static u32 chunk_size = DYNAMAP_DEFAULT_CHUNK_SIZE;
module_param_named(chunk_size, chunk_size, uint, S_IRUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(chunk_size, "Size of DynaSwap chunks in bytes (default: 256MB)");

#pragma region Workqueue Functions


void dynaswap_extend(struct work_struct *work) {

}

void dynaswap_truncate(struct work_struct *work) {

}

#pragma endregion

#pragma region Main Functions

void dynaswap_read(struct dynamap_ctx *ctx, sector_t sector, struct page *page) {
    // unsigned long page_index = sector >> (PAGE_SHIFT - 9);

    // down_read(&ctx->mapping_rwsem);

    // void *entry = xa_load(&ctx->xa_virt_to_phys, page_index);
    // if (!entry) {
    //     // If it isn't a value, just memset the whole thing to 0
    //     // pr_debug("dynaswap: tried to read from an empty page index (idx: %lu)\n", page_index);
    //     void *vaddr = kmap_local_page(page);
    //     memset(vaddr, 0, PAGE_SIZE);
    //     kunmap_local(vaddr);
    //     goto out;
    // }

    // struct slot_entry *slot = (struct slot_entry *)entry;
    // loff_t bd_offset = (loff_t)(slot->index - 1) * PAGE_SIZE;

    // pr_debug("dynamap [READ]: HIT idx %lu -> p_slot %lu (offset %lld)\n", 
    //          page_index, slot->index, bd_offset);

    // void *vaddr = kmap_local_page(page);
    // kernel_read(ctx->backing_file, vaddr, PAGE_SIZE, &bd_offset);
    // kunmap_local(vaddr);

    // out:
    //     up_read(&ctx->mapping_rwsem);
}

void dynaswap_write(struct dynamap_ctx *ctx, sector_t sector, struct page *page) {
    // unsigned long page_index = sector >> (PAGE_SHIFT - 9);
    // unsigned int flags = memalloc_noio_save();
    // down_read(&ctx->mapping_rwsem);

    // unsigned long p_slot = dynaswap_acquire_slot(ctx, page_index);
    // if (!p_slot) {
    //     goto out;
    // }

    // loff_t bd_offset = (loff_t)(p_slot - 1) * PAGE_SIZE;

    // pr_debug("dynamap [WRITE]: Mapping idx %lu -> p_slot %lu (offset %lld)\n", 
    //          page_index, p_slot, bd_offset);

    // void *page_addr = kmap_local_page(page);
    // kernel_write(ctx->backing_file, page_addr, PAGE_SIZE, &bd_offset);
    // kunmap_local(page_addr);

    // out:
    //     up_read(&ctx->mapping_rwsem);
    //     memalloc_noio_restore(flags);
}

void dynaswap_discard(struct dynamap_ctx *ctx, sector_t sector_start, sector_t nr_sectors) {
    // unsigned long page_start = sector_start >> (PAGE_SHIFT - 9);
    // unsigned long page_end = (sector_start + nr_sectors) >> (PAGE_SHIFT - 9);

    // if (page_start >= page_end) {
    //     pr_debug("dynamap [DISCARD]: range too small (sect: %llu, nr: %llu)\n", 
    //              (unsigned long long)sector_start, (unsigned long long)nr_sectors);
    //     return;
    // }

    // pr_debug("dynamap [DISCARD]: requested pages %lu to %lu\n", page_start, page_end);

    // void *entry = NULL;
    // unsigned long index = page_start;

    // down_write(&ctx->mapping_rwsem);

    // xa_for_each_range(&ctx->xa_virt_to_phys, index, entry, page_start, page_end) {
    //     struct slot_entry *slot = (struct slot_entry *)entry;

    //     if (!slot) continue;

    //     pr_debug("dynamap [DISCARD] erasing page %lu (p_slot %lu)\n", index, slot->index);

    //     xa_erase(&ctx->xa_virt_to_phys, index);
    //     slot->virt_page = 0;

    //     llist_add(&slot->node, &ctx->free_slots);
    //     atomic_long_dec(&ctx->active_slots);
    // }

    // up_write(&ctx->mapping_rwsem);
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
        ctx->virt_to_phys_map[i] = (unsigned long *)get_zeroed_page(GFP_KERNEL);
        ctx->phys_to_virt_map[i] = (unsigned long *)get_zeroed_page(GFP_KERNEL);

        if (!ctx->virt_to_phys_map[i] || !ctx->phys_to_virt_map[i])
            goto fail;
    }

    // Slot Bitmapping
    ctx->slot_bitmap = kvzalloc(BITS_TO_LONGS(total_pages) * sizeof(unsigned long), GFP_KERNEL);
    if (!ctx->slot_bitmap)
        goto fail;

    ctx->slot_hint = 0;
    atomic_long_set(&ctx->total_slots, total_pages);
    atomic_long_set(&ctx->active_slots, 0);

    // WorkQueues
    ctx->wq = alloc_workqueue("dynamap_wq", WQ_MEM_RECLAIM | WQ_HIGHPRI | WQ_UNBOUND, 0);
    if (!ctx->wq)
        goto fail;

    INIT_WORK(&ctx->extend_work, dynaswap_extend);
    INIT_WORK(&ctx->truncate_work, dynaswap_truncate);

    // Semaphores
    init_rwsem(&ctx->map_rwsem);

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
