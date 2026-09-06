#include "common.h"

#include <linux/debugfs.h>
#include <linux/workqueue.h>

struct workqueue_struct *DYNASWAP_WORKQUEUE = NULL;
struct dentry *DYNASWAP_SYSFS_DIR = NULL;

int setup_common(void) {
    DYNASWAP_WORKQUEUE = alloc_workqueue("dynaswap_wq", WORKQUEUE_FLAGS, 0);
    if (!DYNASWAP_WORKQUEUE) {
        return -ENOMEM;
    }

    DYNASWAP_SYSFS_DIR = debugfs_create_dir("dynaswap", NULL);
    return 0;
}

void teardown_common(void) {
    debugfs_remove_recursive(DYNASWAP_SYSFS_DIR);
    destroy_workqueue(DYNASWAP_WORKQUEUE);
}