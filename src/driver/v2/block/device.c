#include "config.h"

#include <linux/blkdev.h>
#include <linux/blk-mq.h>

#include "block/device.h"

static blk_status_t handle_rq(struct blk_mq_hw_ctx *hw_ctx, const struct blk_mq_queue_data *queue_data) {
    struct request *req = queue_data->rq;

    blk_mq_start_request(req);
    blk_mq_end_request(req, BLK_STS_OK);

    return BLK_STS_OK;
}

static const struct blk_mq_ops MQ_OPS = {
    .queue_rq = handle_rq,
};

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
        pr_err("dynaswap::block::device.c -- tag_set already freed or not initialized\n");
        return;
    }

    blk_mq_free_tag_set(&TAG_SET);
    memset(&TAG_SET, 0, sizeof(TAG_SET));
    pr_debug("dynaswap::block::device.c -- freed tag set\n");
}

/** Disk Creation
 * 
 * This is where the disk itself is actually created to be used.
 */

static const struct block_device_operations FILE_OPS = {
    .owner = THIS_MODULE,
};

static struct queue_limits QUEUE_LIMITS = {
    .logical_block_size = SECTOR_SIZE,
    .physical_block_size = SECTOR_SIZE,
    .max_discard_sectors = UINT_MAX,
    .max_hw_discard_sectors = UINT_MAX,
    .discard_granularity = SECTOR_SIZE,
};

static uint BLOCK_CAPACITY_GB = 128;

static struct gendisk *DYNASWAP_DISK = NULL;

static bool create_disk(void) {
    // Allocate Disk First
    DYNASWAP_DISK = blk_mq_alloc_disk(&TAG_SET, &QUEUE_LIMITS, NULL);
    if (IS_ERR(DYNASWAP_DISK)) {
        pr_err("dynaswap::block::device.c -- blk_mq_alloc_disk failed: %pE\n", DYNASWAP_DISK);
        DYNASWAP_DISK = NULL;
        return false;
    }

    pr_debug("dynaswap::block::device.c -- gendisk allocated successfully\n");

    // Set up Disk Information
    snprintf(DYNASWAP_DISK->disk_name, 32, BLKDEV_NAME);
    DYNASWAP_DISK->major = BLOCK_MAJOR;
    DYNASWAP_DISK->first_minor = 0;
    DYNASWAP_DISK->minors = 1;
    DYNASWAP_DISK->fops = &FILE_OPS;

    pr_debug("dynaswap::block::device.c -- disk information updated\n");

    /** Configure Discard
     * This is commented out since the queue limits *should* cover this.
     * It may be added back in if this is completely borked and required.

        DYNASWAP_DISK->queue->limits.max_discard_sectors = UINT_MAX;
        DYNASWAP_DISK->queue->limits.max_hw_discard_sectors = UINT_MAX;
        DYNASWAP_DISK->queue->limits.discard_granularity = PAGE_SIZE;

        pr_debug("dynaswap::block::device.c -- discard configured\n");

    **/

    // Set Capacity
    set_capacity(DYNASWAP_DISK, BLOCK_CAPACITY);

    pr_debug("dynaswap::block::device.c -- block capacity updated. (capacity = %d, sectors = %llu)\n", BLOCK_CAPACITY_GB, BLOCK_CAPACITY);

    int err = add_disk(DYNASWAP_DISK);
    if (err) {
        pr_err("dynaswap::block::device.c -- add_disk failed: (err = %pe, code = %d)\n", ERR_PTR(err), err);
        put_disk(DYNASWAP_DISK);
        DYNASWAP_DISK = NULL;
        return false;
    }

    pr_debug("dynaswap::block::device.c -- disk added, creation finished\n");

    return true;
}

static void remove_disk(void) {
    if (!DYNASWAP_DISK) {
        pr_err("dynaswap::block::device.c -- dynaswap disk already NULL, skipping\n");
        return;
    }

    del_gendisk(DYNASWAP_DISK);
    put_disk(DYNASWAP_DISK);

    DYNASWAP_DISK = NULL;

    pr_debug("dynaswap::block::device.c -- deleted dynaswap disk\n");
}

/** Public Functions
 * 
 * Actual functions to be called in other locations. Pretty self explanatory.
 */

void setup_disk(void) {
    register_device();
    alloc_tagset();
    create_disk();
}

void teardown_disk(void) {
    remove_disk();
    free_tagset();
    unregister_device();
}