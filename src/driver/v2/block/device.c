#include <linux/blkdev.h>
#include <linux/blk-mq.h>

#include "device.h"

static int BLOCK_MAJOR = 0;

static blk_status_t handle_rq(struct blk_mq_hw_ctx *hw_ctx, const struct blk_mq_queue_data *queue_data) {
    return BLK_STS_OK;
}

static const struct blk_mq_ops MQ_OPS = {
    .queue_rq = handle_rq,
};

static const struct block_device_operations FILE_OPS = {
    .owner = THIS_MODULE,
};

static bool register_device(void) {
    int major = register_blkdev(BLOCK_MAJOR, BLKDEV_NAME);

    if (major < 0) {
        pr_err("dynaswap::block::device.c -- register_blkdev failed. (major = %d)\n", major);
        return false;
    }

    BLOCK_MAJOR = major;
    pr_debug("dynaswap::block::device.c -- block device registered. (major = %d)\n", BLOCK_MAJOR);
    return true;
}


void setup_device(void) {
    register_device();
}

void teardown_device(void) {
    if (BLOCK_MAJOR > 0) {
        unregister_blkdev(BLOCK_MAJOR, BLKDEV_NAME);
        BLOCK_MAJOR = 0;
    }
}