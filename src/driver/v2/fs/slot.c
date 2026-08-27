#include "common.h"

#include <linux/bitmap.h>
#include <linux/xarray.h>

#include "device.h"

#include "fs/slot.h"

static struct slot_manager SLOT_MANAGER = {0};

int setup_slot_manager(void) {
    log_debug("setting up slot manager");

    // we prealloc the slot bitmap since it's a tiny allocation, no other real reason around it lol
    unsigned int max_slots = (unsigned int)(BLOCK_CAPACITY >> SECTORS_PER_PAGE_SHIFT);
    log_debug("trying to allocate slot bitmap (size = %u)", max_slots);
    SLOT_MANAGER.slot_bitmap = kvmalloc_array(BITS_TO_LONGS(max_slots), sizeof(unsigned long), GFP_KERNEL);

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

    kfree(SLOT_MANAGER.slot_bitmap);
}