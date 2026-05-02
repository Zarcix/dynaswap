#define DEBUG
#undef DEBUG

#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/blk-mq.h>

#include "dynamap.h"

#pragma region Device Configuration

#pragma region Hard Constants

#define BLKDEV_NAME      "dynaswap"

#pragma endregion

#pragma region Module Parameters

static char *dynamap_location = NULL;
module_param_named(path, dynamap_location, charp, S_IRUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(path, "Path to the backing file for DynaSwap");

static uint blk_capacity_gb = 1024;
module_param_named(capacity_gb, blk_capacity_gb, uint, S_IRUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(capacity_gb, "Capacity of the block device in Gigabytes (default: 512G)");

#pragma endregion

#pragma endregion

static int block_major = 0;
static struct gendisk *dynaswap_disk;
static struct blk_mq_tag_set tag_set;
static struct xarray dynaswap_mapping;
static struct dynamap_ctx dynamap;

struct dynaswap_work {
    struct work_struct work;
    struct request *work_req;
    struct dynamap_ctx *work_context;
};

#pragma region Request Handling

static void dynaswap_process_work(struct work_struct *work) {
    struct dynaswap_work *dw = container_of(work, struct dynaswap_work, work);

    struct request *req = dw->work_req;
    unsigned int operation = req_op(req);
    struct dynamap_ctx *ctx = dw->work_context;

    struct bio_vec bvec;
    struct req_iterator iter;

    if (operation == REQ_OP_DISCARD) {
        sector_t nr_sectors = blk_rq_sectors(req);
        sector_t start = blk_rq_pos(req);

        if (nr_sectors > 0) {
            dynaswap_discard(ctx, start, nr_sectors);
        }

        goto end;
    }

    rq_for_each_segment(bvec, req, iter) {
        sector_t sector = iter.iter.bi_sector;
        struct page *page = bvec.bv_page;

        switch (operation) {
            case REQ_OP_WRITE:{
                dynaswap_write(ctx, sector, page);
                break;
            }

            case REQ_OP_READ: {
                dynaswap_read(ctx, sector, page);
                break;
            }

            default: {
                pr_alert("dynaswap: received unknown operation: %d\n", operation);
                break;
            }
        }
    }

end:
    blk_mq_end_request(req, BLK_STS_OK);
    kfree(dw);
}

static blk_status_t dynaswap_handle_rq(struct blk_mq_hw_ctx *hctx, const struct blk_mq_queue_data *bd) {
    struct request *req = bd->rq;
    struct dynaswap_work *dw;

    blk_mq_start_request(req);

    // Allocate a small wrapper to carry the request to the worker
    dw = kmalloc(sizeof(*dw), GFP_ATOMIC);
    if (!dw) return BLK_STS_RESOURCE;

    dw->work_req = req;
    dw->work_context = &dynamap;

    // Initialize the work with our handler function
    INIT_WORK(&dw->work, dynaswap_process_work);

    // Schedule the work on the system-wide workqueue
    schedule_work(&dw->work);

    return BLK_STS_OK;
}

#pragma endregion

#pragma region Cleanup

static void dynaswap_clear_disk(void) {
    if (!dynaswap_disk) {
        pr_debug("dynaswap: disk already NULL, skipping\n");
        return;
    }

    pr_info("dynaswap: deleting gendisk\n");
    del_gendisk(dynaswap_disk);

    pr_info("dynaswap: putting disk\n");
    put_disk(dynaswap_disk);

    dynaswap_disk = NULL;
}

static void dynaswap_free_tag_set(void) {
    if (!tag_set.tags) {
        pr_debug("dynaswap: tag_set already freed or not initialized\n");
        return;
    }

    pr_info("dynaswap: freeing tag_set\n");
    blk_mq_free_tag_set(&tag_set);

    memset(&tag_set, 0, sizeof(tag_set));
}

static void dynaswap_unregister_blockdev(void) {
    if (block_major <= 0) {
        pr_debug("dynaswap: block device not registered\n");
        return;
    }

    pr_info("dynaswap: unregistering block device\n");
    unregister_blkdev(block_major, BLKDEV_NAME);

    block_major = 0;
}

static void dynaswap_destroy_mapping(void) {
    pr_info("dynaswap: destroying xarray\n");
    xa_destroy(&dynaswap_mapping);
    dynamap_cleanup(&dynamap);
}

static void __exit dynaswap_module_exit(void) {
    pr_info("dynaswap: starting cleanup\n");

    dynaswap_clear_disk();
    dynaswap_free_tag_set();
    dynaswap_unregister_blockdev();
    dynaswap_destroy_mapping();

    pr_info("dynaswap: cleanup finished\n");
}

#pragma endregion

#pragma region Initialization

static const struct blk_mq_ops multi_queue_ops = {
    .queue_rq = dynaswap_handle_rq,
};

static const struct block_device_operations file_ops = {
    .owner = THIS_MODULE,
};

static int dynaswap_init_mapping(void) {
    if (dynamap_location == NULL) {
        pr_err("dynaswap: ERROR - 'path' parameter is required.\n");
        pr_err("Usage: insmod dynaswap.ko path=/path/to/file\n");
        return -EINVAL; // Invalid Argument
    }
    xa_init(&dynaswap_mapping);

    size_t total_capacity = (size_t)blk_capacity_gb * 1024 * 1024 * 1024;
    int status = dynamap_init(&dynamap, dynamap_location, total_capacity);
    pr_debug("dynaswap: mapping initialized\n");
    return status;
}

static int dynaswap_register_blockdev(void) {
    int major = register_blkdev(block_major, BLKDEV_NAME);
    if (major < 0) {
        pr_err("dynaswap: register_blkdev failed: %d\n", major);
        return major;
    }

    block_major = major;

    pr_debug("dynaswap: block device registered (major=%d)\n", block_major);
    return 0;
}

static int dynaswap_init_tag_set(void) {
    int ret;

    tag_set.ops = &multi_queue_ops;
    tag_set.nr_hw_queues = 1;
    tag_set.queue_depth = 128;
    tag_set.numa_node = NUMA_NO_NODE;
    tag_set.flags = BLK_MQ_F_BLOCKING;

    ret = blk_mq_alloc_tag_set(&tag_set);
    if (ret) {
        pr_err("dynaswap: blk_mq_alloc_tag_set failed: %d\n", ret);
        return ret;
    }

    pr_debug("dynaswap: tag_set initialized\n");
    return 0;
}

static int dynaswap_setup_disk(void) {
    struct queue_limits lim = {
        .logical_block_size = BLK_SECTOR_SIZE,
        .physical_block_size = BLK_SECTOR_SIZE,
        .max_discard_sectors = UINT_MAX,
        .discard_granularity = BLK_SECTOR_SIZE,
    };

    sector_t capacity;

    dynaswap_disk = blk_mq_alloc_disk(&tag_set, &lim, NULL);
    if (IS_ERR(dynaswap_disk)) {
        int err = PTR_ERR(dynaswap_disk);
        dynaswap_disk = NULL;
        pr_err("dynaswap: blk_mq_alloc_disk failed: %d\n", err);
        return err;
    }

    pr_debug("dynaswap: disk allocated\n");

    /* core identity */
    dynaswap_disk->major = block_major;
    dynaswap_disk->first_minor = 0;
    dynaswap_disk->minors = 1;
    dynaswap_disk->fops = &file_ops;

    snprintf(dynaswap_disk->disk_name, 32, BLKDEV_NAME);

    /* queue limits (override defaults if needed) */
    dynaswap_disk->queue->limits.max_discard_sectors = UINT_MAX;
    dynaswap_disk->queue->limits.max_hw_discard_sectors = UINT_MAX;
    dynaswap_disk->queue->limits.discard_granularity = PAGE_SIZE;

    /* capacity */
    capacity = blk_capacity_gb * 1024 * 1024 * 2;
    set_capacity(dynaswap_disk, capacity);

    pr_debug("dynaswap: disk configured (capacity=%llu sectors)\n",
             (unsigned long long)capacity);

    return 0;
}

static int dynaswap_init_err(int ret) {
    pr_err("dynaswap: init failed (%d), unwinding\n", ret);

    dynaswap_clear_disk();
    dynaswap_free_tag_set();
    dynaswap_unregister_blockdev();
    dynaswap_destroy_mapping();

    return ret;
}

static int __init dynaswap_module_init(void) {
    pr_info("dynaswap: initializing\n");

    int status;

    status = dynaswap_init_mapping();
    if (status) return dynaswap_init_err(status);

    status = dynaswap_register_blockdev();
    if (status) return dynaswap_init_err(status);

    status = dynaswap_init_tag_set();
    if (status) return dynaswap_init_err(status);

    status = dynaswap_setup_disk();
    if (status) return dynaswap_init_err(status);

    status = add_disk(dynaswap_disk);
    if (status) return dynaswap_init_err(status);

    pr_info("dynaswap: initialization complete\n");
    return 0;
}

#pragma endregion

module_init(dynaswap_module_init);
module_exit(dynaswap_module_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Dynamically creates swap space");