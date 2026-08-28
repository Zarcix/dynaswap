#include "common.h"

#include <linux/workqueue.h>

struct workqueue_struct *DYNASWAP_WORKQUEUE = NULL;

int setup_common(void) {
    DYNASWAP_WORKQUEUE = alloc_workqueue("dynaswap_wq", WORKQUEUE_FLAGS, 0);
    if (!DYNASWAP_WORKQUEUE) {
        return -ENOMEM;
    }
    return 0;
}

void teardown_common(void) {
    destroy_workqueue(DYNASWAP_WORKQUEUE);
}