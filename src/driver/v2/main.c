#include "config.h"

#include <linux/module.h>

#include "block/device.h"

static int __init init_driver(void) {
    setup_disk();
    return 0;
}

static void __exit exit_driver(void) {
    teardown_disk();
    return;
}

module_init(init_driver);
module_exit(exit_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Dynamically creates swap space");