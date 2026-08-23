#include "config.h"

#include <linux/module.h>

#include "block/device.h"

static int __init init(void) {
    setup_disk();
    return 0;
}

static void __exit exit(void) {
    teardown_disk();
    return;
}

module_init(init);
module_exit(exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Dynamically creates swap space");