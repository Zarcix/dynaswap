#include "common.h"

#include "context.h"
#include "fs/storage.h"
#include "fs/slot.h"

static struct context CONTEXT = {0};

/**
 * Helpers
 */

/**
 * Workqueue Functions
 */

static void dynaswap_extend(struct work_struct *work) {}

static void dynaswap_truncate(struct work_struct *work) {}

/**
 * Public Functions
 */

blk_status_t dynaswap_read(sector_t sector, struct page *page) {
    log_debug("received read operation (sector = %llu)", sector);
    return BLK_STS_OK;
}

blk_status_t dynaswap_write(sector_t sector, struct page *page) {
    log_debug("received read operation (sector = %llu)", sector);
    return BLK_STS_OK;
}

blk_status_t dynaswap_discard(void) {
    return BLK_STS_OK;
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

    return 0;
}
