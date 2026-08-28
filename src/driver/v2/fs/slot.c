#include "common.h"

#include <linux/types.h>
#include <linux/bitmap.h>
#include <linux/xarray.h>

#include "device.h"

#include "fs/slot.h"

static struct slot_manager SLOT_MANAGER = {0};

/**
 * Module Arguments
 */

unsigned char EXTEND_THRESHOLD_PERCENT = 80;
module_param_named(extend_threshold, EXTEND_THRESHOLD_PERCENT, byte, S_IRUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(extend_threshold, "Percent where DynaSwap's backing file will self inflate (default: 80%)");

/**
 * Helpers
 */

bool slot_manager_needs_extend(void) {
    unsigned long total = atomic_long_read(&SLOT_MANAGER.total_slots);
    unsigned long active = atomic_long_read(&SLOT_MANAGER.active_slots);

    if (total == 0) {
        return false;
    }

    unsigned long usage_percentage = (active * 100) / total;
    return usage_percentage >= EXTEND_THRESHOLD_PERCENT;
}

unsigned long get_total_slots(void) {
    return atomic_long_read(&SLOT_MANAGER.total_slots);
}

/**
 * Slot Functionality
 */

void extend_slots(unsigned long new_slots) {
    long old_slots = atomic_long_read(&SLOT_MANAGER.total_slots);
    long max_slots = SLOT_MANAGER.bitmap_size;

    long target_slots;
    do {
        if (old_slots >= max_slots) {
            break;
        }

        target_slots = old_slots + new_slots;
        if (target_slots > max_slots) {
            target_slots = max_slots;
        }
    } while (!atomic_long_try_cmpxchg(&SLOT_MANAGER.total_slots, &old_slots, target_slots));
    log_debug("extended slots (total slot count = %lu)", target_slots);
}

int reserve_slot(sector_t sector, unsigned long *slot) {
    if (find_slot(sector, slot) == 0) {
        return 0;
    }

    unsigned long page = sector >> SECTORS_PER_PAGE_SHIFT;

    // This page has never been allocated before, need to find free slot
    unsigned long total_slots = atomic_long_read(&SLOT_MANAGER.total_slots);
    unsigned long free_slot = find_next_zero_bit(SLOT_MANAGER.slot_bitmap, total_slots, SLOT_MANAGER.bitmap_hint);
    if (free_slot >= total_slots) {
        free_slot = find_next_zero_bit(SLOT_MANAGER.slot_bitmap, total_slots, 0);
    }

    if (free_slot >= total_slots) {
        pr_err("no free slots found. (total slots = %lu, used slots = %lu)", total_slots, atomic_long_read(&SLOT_MANAGER.active_slots));
        return -ENOSPC;
    }

    // Free slot found, set bitmap first
    set_bit(free_slot, SLOT_MANAGER.slot_bitmap);
    SLOT_MANAGER.bitmap_hint = (free_slot + 1) % total_slots;
    
    // Update BiMap with both values
    void *xa_page = xa_mk_value(page);
    void *xa_slot = xa_mk_value(free_slot);

    int ret;

    ret = xa_err(xa_store(&SLOT_MANAGER.page_to_slot, page, xa_slot, GFP_KERNEL));
    if (unlikely(ret)) {
        pr_err("failed to store data to page_to_slot map (page = %lu, slot = %lu)", page, free_slot);
        clear_bit(free_slot, SLOT_MANAGER.slot_bitmap);
        return ret;
    }

    ret = xa_err(xa_store(&SLOT_MANAGER.slot_to_page, free_slot, xa_page, GFP_KERNEL));
    if (unlikely(ret)) {
        pr_err("failed to store data to slot_to_page map (slot = %lu, page = %lu)",free_slot ,page);
        xa_erase(&SLOT_MANAGER.page_to_slot, page);
        clear_bit(free_slot, SLOT_MANAGER.slot_bitmap);
        return ret;
    }

    atomic_long_inc(&SLOT_MANAGER.active_slots);

    *slot = free_slot;

    return 0;
}

int find_slot(sector_t sector, unsigned long *slot) {
    unsigned long page = sector >> SECTORS_PER_PAGE_SHIFT;

    // Find slot if it exists
    void *potential_slot = xa_load(&SLOT_MANAGER.page_to_slot, page);
    if (xa_is_value(potential_slot)) {
        *slot = xa_to_value(potential_slot);
        return 0;
    }

    // If it doesn't exist, then we just quit
    return -ENOENT;
}

/**
 * Initialization
 */

int setup_slot_manager(void) {
    log_debug("setting up slot manager");

    // we prealloc the slot bitmap since it's a tiny allocation, no other real reason around it lol
    SLOT_MANAGER.bitmap_size = (unsigned long)(BLOCK_CAPACITY >> SECTORS_PER_PAGE_SHIFT);
    SLOT_MANAGER.bitmap_hint = 0;
    log_debug("trying to allocate slot bitmap (size = %lu)", SLOT_MANAGER.bitmap_size);
    SLOT_MANAGER.slot_bitmap = kvmalloc_array(BITS_TO_LONGS(SLOT_MANAGER.bitmap_size), sizeof(unsigned long), GFP_KERNEL);

    if (!SLOT_MANAGER.slot_bitmap) {
        return -ENOMEM;
    }

    log_debug("trying to init slot bimap");

    xa_init(&SLOT_MANAGER.page_to_slot);
    xa_init(&SLOT_MANAGER.slot_to_page);

    log_debug("setting slot sizes to 0");

    atomic_long_set(&SLOT_MANAGER.total_slots, 0);
    atomic_long_set(&SLOT_MANAGER.active_slots, 0);

    return 0;
}

void teardown_slot_manager(void){ 
    xa_destroy(&SLOT_MANAGER.slot_to_page);
    xa_destroy(&SLOT_MANAGER.page_to_slot);
    kvfree(SLOT_MANAGER.slot_bitmap);
}