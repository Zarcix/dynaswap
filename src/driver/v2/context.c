#include "common.h"

#include <linux/blk_types.h>
#include <linux/blkdev.h>

#include "context.h"
#include "fs/storage.h"
#include "fs/slot.h"

static struct context CONTEXT = {0};

/**
 * Module Arguments
 */

unsigned short int CHUNK_SIZE_MB = 1024UL;
module_param_named(chunk_size, CHUNK_SIZE_MB, ushort, S_IRUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(chunk_size, "Size of DynaSwap chunks in MB (default: 1024MB)");

/**
 * Workqueue Functions
 */

static void dynaswap_extend(struct work_struct *work) {
    unsigned long current_slots = get_total_slots();
    unsigned long new_slots = CHUNK_SIZE >> PAGE_SHIFT;

    down_write(&CONTEXT.work_sem);
    int status = extend_storage(current_slots, new_slots);
    if (status < 0) {
        log_err("failed to extend storage file (current slot count = %lu, attempted new count = %lu)", current_slots, current_slots + new_slots);
        up_write(&CONTEXT.work_sem);
        return;
    }
    up_write(&CONTEXT.work_sem);

    extend_slots(new_slots);
}

static void dynaswap_truncate(struct work_struct *work) {}

/**
 * Public Functions
 */

int dynaswap_read(sector_t sector, struct page *page) {
    down_read(&CONTEXT.work_sem);

    unsigned long slot;
    int ret;
    
    // This isn't really an error. If find_slot fails, it means it couldn't find a slot. This means that it is an empty page aka a zeroed page.
    ret = find_slot_sector(sector, &slot);
    if (ret) {
        up_read(&CONTEXT.work_sem);
        clear_highpage(page);
        return 0;
    }

    up_read(&CONTEXT.work_sem);

    ret = read_storage(slot, page);
    if (ret) {
        log_err("failed to get read from backing storage (slot = %lu, err = %pe)", slot, ERR_PTR(ret));
        return ret;
    }

    return 0;
}

int dynaswap_write(sector_t sector, struct page *page) {
    if (slot_manager_needs_extend()) {
        queue_work(DYNASWAP_WORKQUEUE, &CONTEXT.extend_work);
    }

    unsigned long write_slot;
    int ret;
 
    down_write(&CONTEXT.work_sem);

    ret = reserve_slot_sector(sector, &write_slot);
    if (ret) {
        log_err("failed to get write slot for sector (sector = %llu, err = %pe)", sector, ERR_PTR(ret));
        up_write(&CONTEXT.work_sem);
        return ret;
    }

    up_write(&CONTEXT.work_sem);

    ret = write_storage(write_slot, page);
    if (ret) {
        log_err("failed to write to backing storage (slot = %lu, err = %pe)", write_slot, ERR_PTR(ret));
        return ret;
    }

    return 0;
}

int dynaswap_discard(sector_t start, unsigned int count) {
    int ret;
    down_write(&CONTEXT.work_sem);

    ret = clear_slot_sector_range(start, count);
    if (unlikely(ret)) {
        log_err("failed to discard sector range (start = %llu, count = %u)", start, count);
        up_write(&CONTEXT.work_sem);
        return ret;
    }

    up_write(&CONTEXT.work_sem);

    // we don't holepunch a file because it would just take more cycles.
    // if the space is truly empty, we will truncate it instead

    return ret;
}

/**
 * Cleanup
 */

void teardown_context(void) {
    teardown_slot_manager();
    teardown_storage();
}

/**
 * Initialization
 */

static int setup_self_context(void) {
    INIT_WORK(&CONTEXT.extend_work, dynaswap_extend);
    INIT_WORK(&CONTEXT.truncate_work, dynaswap_truncate);

    init_rwsem(&CONTEXT.work_sem);
    return 0;
}

int setup_context(void) {
    int status;

    status = setup_self_context();
    if (status < 0) {
        log_err("self context initialization failed");
        return status;
    }

    status = setup_storage();
    if (status < 0) {
        log_err("failed initializing storage");
        return status;
    }

    status = setup_slot_manager();
    if (status < 0) {
        log_err("failed to setup slot manager");
        teardown_storage();
        return status;
    }

    // Call first extend to actually make sure we initially have space
    dynaswap_extend(NULL);

    return 0;
}
