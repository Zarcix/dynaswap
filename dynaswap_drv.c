#define DEBUG

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/blk-mq.h>

#define BLKDEV_NAME     "dynaswap"
#define BLK_SECTOR_SIZE  512
#define BLK_CAPACITY_GB  64

static int block_major = 0;
static struct gendisk *dynaswap_disk;
static struct blk_mq_tag_set tag_set; // TODO: what is this?
static struct xarray dynaswap_mapping;

static blk_status_t dynaswap_handle_rq(struct blk_mq_hw_ctx *hctx, const struct blk_mq_queue_data *bd) {
    /* Simple completion for now */
    blk_mq_start_request(bd->rq);
    blk_mq_end_request(bd->rq, BLK_STS_OK);
    return BLK_STS_OK;
}

static const struct blk_mq_ops multi_queue_ops = {
    .queue_rq = dynaswap_handle_rq,
};

static const struct block_device_operations file_ops = {
    .owner = THIS_MODULE,
};

static int __init dynaswap_init(void) {
    pr_info("dynaswap: initializing\n");

    int status;

    struct queue_limits lim = { // TODO: what do these actually do?
        .logical_block_size = BLK_SECTOR_SIZE,
        .physical_block_size = BLK_SECTOR_SIZE,
        .max_discard_sectors = UINT_MAX,
        .discard_granularity = BLK_SECTOR_SIZE,
    };

    xa_init(&dynaswap_mapping);
    pr_debug("dynaswap: dynaswap_mapping initialized\n");

    status = register_blkdev(block_major, BLKDEV_NAME);
    if (status < 0) return status;
    block_major = status;
    pr_debug("dynaswap: block device registered, blkid=%d\n", block_major);

    tag_set.ops = &multi_queue_ops;
    tag_set.nr_hw_queues = 1;
    tag_set.queue_depth = 128;
    tag_set.numa_node = NUMA_NO_NODE;
    tag_set.flags = 0; 
    if (blk_mq_alloc_tag_set(&tag_set)) {
        status = -ENOMEM;
        goto err_unregister_blk_dev;
    }
    pr_debug("dynaswap: tag_set allocated (queues=%u, depth=%u)\n", 
         tag_set.nr_hw_queues, tag_set.queue_depth);

    // Disk Allocation Step
    dynaswap_disk = blk_mq_alloc_disk(&tag_set, &lim, NULL);
    if (IS_ERR(dynaswap_disk)) {
        status = PTR_ERR(dynaswap_disk);
        goto err_free_tag;
    }
    pr_debug("dynaswap: disk object allocated\n");

    dynaswap_disk->major = block_major;
    dynaswap_disk->first_minor = 0;
    dynaswap_disk->minors = 1;
    dynaswap_disk->fops = &file_ops;
    snprintf(dynaswap_disk->disk_name, 32, BLKDEV_NAME);

    sector_t capacity_size = BLK_CAPACITY_GB * 1024 * 1024 * 2;
    set_capacity(dynaswap_disk, capacity_size);

    pr_debug("dynaswap: setup device %s (major=%d, capacity=%llu sectors)\n", 
             dynaswap_disk->disk_name, block_major, (unsigned long long)capacity_size);

    status = add_disk(dynaswap_disk);
    if (status) {
        put_disk(dynaswap_disk);
        goto err_free_tag;
    }
    pr_debug("dynaswap: device live and registered with the block layer\n");

    pr_info("dynaswap: initialization complete\n");
    return 0;

err_free_tag:
    blk_mq_free_tag_set(&tag_set);
err_unregister_blk_dev:
    unregister_blkdev(block_major, BLKDEV_NAME);
    return status;
}

static void __exit dynaswap_exit(void) {
    pr_info("dynaswap: starting cleanup\n");

    del_gendisk(dynaswap_disk);
    put_disk(dynaswap_disk);

    pr_debug("dynaswap: disks have been cleaned up\n");

    blk_mq_free_tag_set(&tag_set);
    pr_debug("dynaswap: tag set has been cleaned up\n");

    unregister_blkdev(block_major, BLKDEV_NAME);
    pr_debug("dynaswap: block device has been unregistered\n");

    xa_destroy(&dynaswap_mapping);
    pr_debug("dynaswap: swap mapping destroyed\n");

    pr_info("dynaswap: cleanup finished\n");
}

module_init(dynaswap_init);
module_exit(dynaswap_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Dynamically creates swap space");