#include "common.h"

#include <linux/types.h>
#include <linux/bitmap.h>
#include <linux/xarray.h>

#include "device.h"

#include "fs/slot_debug.h"
#include "fs/slot.h"

struct slot_manager SLOT_MANAGER = {0};

/**
 * Module Arguments
 */

unsigned char EXTEND_THRESHOLD_PERCENT = 50;
module_param_named(extend_threshold, EXTEND_THRESHOLD_PERCENT, byte, S_IRUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(extend_threshold, "Percent where DynaSwap's backing file will self inflate (default: 50%)");

/**
 * Helpers
 */

extend_status slot_manager_needs_extend(void) {
    unsigned long total = atomic_long_read(&SLOT_MANAGER.total_slots);
    unsigned long active = atomic_long_read(&SLOT_MANAGER.active_slots);

    if (total == 0 || total == active) {
        return EXTEND_REQUIRED;
    }

    unsigned long usage_percentage = (active * 100) / total;
    return usage_percentage >= EXTEND_THRESHOLD_PERCENT ? EXTEND_NEEDED : EXTEND_NOT_NEEDED;
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
    log_debug("extended slots (total slot count = %lu)", atomic_long_read(&SLOT_MANAGER.total_slots));
}

int reserve_slot_sector(sector_t sector, unsigned long *slot) {
    if (find_slot_sector(sector, slot) == 0) {
        log_debug("reusing reserved slot %lu", *slot);
        return 0;
    }

    // This page has never been allocated before, need to find free slot
    unsigned long total_slots = atomic_long_read(&SLOT_MANAGER.total_slots);
    unsigned long free_slot = find_next_zero_bit(SLOT_MANAGER.slot_bitmap, total_slots, SLOT_MANAGER.bitmap_hint);
    if (free_slot >= total_slots) {
        free_slot = find_next_zero_bit(SLOT_MANAGER.slot_bitmap, total_slots, 0);
    }

    if (free_slot >= total_slots) {
        log_err("no free slots found. (total slots = %lu, used slots = %lu)", total_slots, atomic_long_read(&SLOT_MANAGER.active_slots));
        return -ENOSPC;
    }

    // Free slot found, set bitmap first
    set_bit(free_slot, SLOT_MANAGER.slot_bitmap);
    SLOT_MANAGER.bitmap_hint = (free_slot + 1) % total_slots;
    atomic_long_inc(&SLOT_MANAGER.active_slots);

    int status = bimap_insert(&SLOT_MANAGER.slot_bimap, sector, free_slot);
    if (status) {
        log_debug("failed to insert into bimap (sector = %llu, slot = %lu)", sector, free_slot);
        return status;
    }

    *slot = free_slot;

    return 0;
}

int find_slot_sector(sector_t sector, unsigned long *slot) {
    return bimap_find_by_sector(&SLOT_MANAGER.slot_bimap, sector, slot);
}

int clear_slot_sector_range(sector_t sector, unsigned int count) {
    log_debug("sector attempted to clear");
    // unsigned long start_page = sector >> SECTORS_PER_PAGE_SHIFT;
    // unsigned long end_page = (sector + count + (1 << SECTORS_PER_PAGE_SHIFT) - 1) >> SECTORS_PER_PAGE_SHIFT;

    // unsigned long page;
    // for (page = start_page; page < end_page; page++) {
    //     void *potential_slot = xa_load(&SLOT_MANAGER.page_to_slot, page);
    //     if (!xa_is_value(potential_slot)) {
    //         continue;
    //     }

    //     unsigned long slot = xa_to_value(potential_slot);

    //     xa_erase(&SLOT_MANAGER.page_to_slot, page);
    //     xa_erase(&SLOT_MANAGER.slot_to_page, slot);

    //     if (test_and_clear_bit(slot, SLOT_MANAGER.slot_bitmap)) {
    //         atomic_long_dec(&SLOT_MANAGER.active_slots);
    //     }
    // }

    // log_debug("cleared slot range (start = %lu, end = %lu)", start_page, end_page);

    return 0;
}

/**
 * Initialization
 */

int setup_slot_manager(void) {
    log_debug("setting up slot manager");

    SLOT_MANAGER.bitmap_size = (unsigned long)(BLOCK_CAPACITY >> PAGE_SHIFT);
    SLOT_MANAGER.bitmap_hint = 0;
    log_debug("trying to allocate slot bitmap (size = %lu)", SLOT_MANAGER.bitmap_size);
    SLOT_MANAGER.slot_bitmap = kvzalloc(bitmap_size(SLOT_MANAGER.bitmap_size), GFP_KERNEL);

    if (!SLOT_MANAGER.slot_bitmap) {
        return -ENOMEM;
    }

    log_debug("trying to init slot bimap");

    init_bimap(&SLOT_MANAGER.slot_bimap);

    log_debug("setting slot sizes to 0");

    atomic_long_set(&SLOT_MANAGER.total_slots, 0);
    atomic_long_set(&SLOT_MANAGER.active_slots, 0);

    setup_slot_debug();

    return 0;
}

void teardown_slot_manager(void) {
    destroy_bimap(&SLOT_MANAGER.slot_bimap);
    kvfree(SLOT_MANAGER.slot_bitmap);
    log_debug("slot manager torn down successfully");
}