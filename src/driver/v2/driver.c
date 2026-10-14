#include "linux/blk_types.h"
#define DEBUG

#include <linux/blkdev.h>
#include <linux/blk-mq.h>
#include <linux/module.h>

#include "common.h"
#include "swap.h"

#define BLKDEV_NAME "dynaswap"

/* Request Handles */

static struct swap dynaswap = {0};

static blk_status_t queue_rq(struct blk_mq_hw_ctx *hctx, const struct blk_mq_queue_data *bd) {
    struct request *req = bd->rq;

    blk_mq_start_request(req);

    if (blk_rq_is_passthrough(req)) {
        blk_mq_end_request(req, BLK_STS_IOERR);
        return BLK_STS_OK;
    }

    enum req_op operation = rq_data_dir(req);

    struct bio_vec bvec;
    struct req_iterator iter;
    rq_for_each_segment(bvec, req, iter) {
        sector_t sector = iter.iter.bi_sector;
        struct page *page = bvec.bv_page;

        sector_t virtual_slot = sector >> 3;

        switch (operation) {
            case REQ_OP_READ: {
                swap_read(&dynaswap, virtual_slot, page);
                break;
            }

            case REQ_OP_WRITE: {
                swap_write(&dynaswap, virtual_slot, page);
                break;
            }

            default: {
                log_alert("unknown operation: %d\n", operation);
                break;
            }
        }
    }


    blk_mq_end_request(req, BLK_STS_OK);

    return BLK_STS_OK;
}

/* Tag Set */

static const struct blk_mq_ops MQ_OPS = {
    .queue_rq = queue_rq,
};

static struct blk_mq_tag_set TAG_SET = {
    .ops = &MQ_OPS,
    .nr_hw_queues = 1,
    .queue_depth = 128,
    .numa_node = NUMA_NO_NODE,
    .flags = BLK_MQ_F_BLOCKING
};

static void free_tagset(void) {
    if (!TAG_SET.tags) {
        log_alert("tagset free called but tagset is not initialized");
        return;
    }

    blk_mq_free_tag_set(&TAG_SET);
    memset(&TAG_SET, 0, sizeof(TAG_SET));

    log_debug("tag set freed");
    return;
}

static int init_tagset(void) {
    int ret = blk_mq_alloc_tag_set(&TAG_SET);
    if (ret) {
        log_err("blk_mq_alloc_tag_set failed: %d\n", ret);
        return ret;
    }

    log_debug("tag set initialized");

    return 0;
}

/* Block Device */

static int blkdev_major = 0;

static int init_blkdev(void) {
    int major = register_blkdev(0, BLKDEV_NAME);
    if (major < 0) {
        log_err("register_blkdev failed: %d", major);
        return major;
    }

    blkdev_major = major;

    log_debug("block device registered (major = %d)", blkdev_major);
    return 0;
}

static void free_blkdev(void) {
    if (blkdev_major <= 0) {
        log_alert("block device free called but block device was not registered");
        return;
    }

    unregister_blkdev(blkdev_major, BLKDEV_NAME);

    blkdev_major = 0;
}

/* Disk */

static struct queue_limits QUEUE_LIMITS = {
    .logical_block_size = SECTOR_SIZE,
    .physical_block_size = SECTOR_SIZE,
    .max_discard_sectors = UINT_MAX,
    .discard_granularity = SECTOR_SIZE,
};

static const struct block_device_operations FILE_OPS = {
    .owner = THIS_MODULE,
};

static struct gendisk *blkdev_disk;
static uint blkdev_size = 128;
module_param_named(capacity_gb, blkdev_size, uint, S_IRUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(capacity_gb, "Capacity of the block device in Gigabytes (default: 128G)");

static void free_disk(void) {
    if (!blkdev_disk) {
        log_alert("free disk called but disk not initialized");
        return;
    }

    del_gendisk(blkdev_disk);
    put_disk(blkdev_disk);
    blkdev_disk = NULL;

    log_debug("freed block device disk");

    return;
}

static int init_disk(void) {
    blkdev_disk = blk_mq_alloc_disk(&TAG_SET, &QUEUE_LIMITS, NULL);
    if (IS_ERR(blkdev_disk)) {
        int err = PTR_ERR(blkdev_disk);
        blkdev_disk = NULL;
        log_err("dynaswap: blk_mq_alloc_disk failed: %d", err);
        return err;
    }

    blkdev_disk->major = blkdev_major;
    blkdev_disk->first_minor = 0;
    blkdev_disk->minors = 1;
    blkdev_disk->fops = &FILE_OPS;

    snprintf(blkdev_disk->disk_name, 32, BLKDEV_NAME);

    sector_t sectors = blkdev_size << 21;
    set_capacity(blkdev_disk, sectors);

    int status = add_disk(blkdev_disk);
    if (status) return status;

    log_debug("initialized block disk (sectors = %llu, size = %u)", sectors, blkdev_size);

    return 0;
}


/* Module */

static void __exit mod_exit(void) {
    free_swap(&dynaswap);
    free_disk();
    free_tagset();
    free_blkdev();

    log_info("driver torn down");
    return;
}

static int __init mod_init(void) {
    int err;
    
    err = init_blkdev();
    if (err) {
        return err;
    }

    err = init_tagset();
    if (err) {
        return err;
    }

    err = init_disk();
    if (err) {
        return err;
    }

    err = init_swap(&dynaswap);
    if (err) {
        return err;
    }

    log_info("driver initialized");
    return 0;
}

module_init(mod_init);
module_exit(mod_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Dynamic Swap Device");