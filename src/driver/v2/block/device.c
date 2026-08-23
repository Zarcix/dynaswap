#include "config.h"

#include <linux/blkdev.h>
#include <linux/blk-mq.h>

#include "block/device.h"

static const struct block_device_operations FILE_OPS = {
    .owner = THIS_MODULE,
};

static blk_status_t handle_rq(struct blk_mq_hw_ctx *hw_ctx, const struct blk_mq_queue_data *queue_data) {
    return BLK_STS_OK;
}

static const struct blk_mq_ops MQ_OPS = {
    .queue_rq = handle_rq,
};

/** Tag Set Allocation
 * 
 * I am actually not sure what tag sets are even used for, but we need it to get
 * operations working for requests. XD
 */

static struct blk_mq_tag_set TAG_SET = {
    .ops = &MQ_OPS,
    .nr_hw_queues = 1,
    .queue_depth = 128,
    .numa_node = NUMA_NO_NODE,
    .flags = BLK_MQ_F_BLOCKING
};

static bool alloc_tagset(void) {
    int err = blk_mq_alloc_tag_set(&TAG_SET);
    if (err) {
        pr_err("dynaswap::block::device.c -- blk_mq_alloc_tag_set failed. (err: %d,%pE)\n", err, ERR_PTR(err));
        return false;
    }

    pr_debug("dynaswap::block::device.c -- tag set allocated\n");
    return true;
}

static void free_tagset(void) {
    if (!TAG_SET.tags) {
        pr_err("dynaswap: tag_set already freed or not initialized\n");
        return;
    }

    blk_mq_free_tag_set(&TAG_SET);
    memset(&TAG_SET, 0, sizeof(TAG_SET));
    pr_debug("dynaswap::block::device.c -- freed tag set\n");
}

/** Device Registration
 * 
 * This section performs some device registration. Keep in mind that this does
 * not create the device under /dev/dynaswap itself btw. That is handled under
 * block device creation.
 */

static int BLOCK_MAJOR = 0;

static bool register_device(void) {
    int major = register_blkdev(BLOCK_MAJOR, BLKDEV_NAME);
    if (major < 0) {
        pr_err("dynaswap::block::device.c -- register_blkdev failed. (err = %d,%pE)\n", major, ERR_PTR(major));
        return false;
    }

    BLOCK_MAJOR = major;
    pr_debug("dynaswap::block::device.c -- block device registered. (major = %d)\n", BLOCK_MAJOR);
    return true;
}

static void unregister_device(void) {
    if (BLOCK_MAJOR > 0) {
        pr_debug("dynaswap::block::device.c -- block device found, unregistering device. (major = %d)\n", BLOCK_MAJOR);
        unregister_blkdev(BLOCK_MAJOR, BLKDEV_NAME);
        BLOCK_MAJOR = 0;
    }
}

/** Public Functions
 * 
 * Actual functions to be called in other locations. Pretty self explanatory.
 */

void setup_disk(void) {
    register_device();
    alloc_tagset();
}

void teardown_disk(void) {
    free_tagset();
    unregister_device();
}