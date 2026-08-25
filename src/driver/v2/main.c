#include "common.h"

#include <linux/module.h>

#include "context.h"
#include "device.h"

static int __init init_driver(void) {
    int status;
    
    status = setup_context();
    if (status) {
        return status;
    }

    status = setup_disk();
    if (status < 0) {
        teardown_context();
        return status;
    }

    return 0;
}

static void __exit exit_driver(void) {
    teardown_disk();
    teardown_context();
    return;
}

module_init(init_driver);
module_exit(exit_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Dynamically creates swap space");