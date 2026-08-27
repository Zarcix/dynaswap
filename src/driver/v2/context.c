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

void dynaswap_read(void) {}

void dynaswap_write(void) {}

void dynaswap_discard(void) {}

/**
 * Cleanup
 */

static void teardown_self_context(void) {
    if (!CONTEXT.workqueue) {
        log_debug("workqueue does not exist in self context teardown");
        return;
    }

    destroy_workqueue(CONTEXT.workqueue);
    CONTEXT.workqueue = NULL;
}

void teardown_context(void) {
    teardown_storage();
    teardown_self_context();
}

/**
 * Initialization
 */

static int setup_self_context(void) {
    CONTEXT.workqueue = alloc_workqueue("dynaswap_wq", WORKQUEUE_FLAGS, 0);
    if (!CONTEXT.workqueue) {
        return -ENOMEM;
    }

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
        teardown_self_context();
        return status;
    }

    status = setup_slot_manager();
    if (status < 0) {
        log_err("failed to setup slot manager");
        teardown_storage();
        teardown_self_context();
        return status;
    }

    return 0;
}
