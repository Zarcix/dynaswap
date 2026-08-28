#include "common.h"

#include <linux/types.h>
#include <linux/bitmap.h>
#include <linux/xarray.h>

#include "device.h"

#include "fs/slot.h"

static struct slot_manager SLOT_MANAGER = {0};

bool slot_manager_needs_extend(int threshold) {
    unsigned long total = atomic_long_read(&SLOT_MANAGER.total_slots);
    unsigned long active = atomic_long_read(&SLOT_MANAGER.active_slots);

    if (total == active) {
        return true;
    }

    unsigned long usage_percentage = (active * 100) / total;
    return usage_percentage >= threshold;
}

// unsigned long slot_manager_write(sector_t sector) {
//     unsigned long page = (unsigned long)(sector >> SECTORS_PER_PAGE_SHIFT);

//     // Check if the page has already been allocated. If it has, we will overwrite the page.
//     void *potential_slot = xa_load(&SLOT_MANAGER.page_to_slot, page);
//     if (xa_is_value(potential_slot)) {
//         return xa_to_value(potential_slot);
//     }

// }

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