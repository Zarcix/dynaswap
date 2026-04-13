#define DEBUG

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/blk-mq.h>
#include "dynamap.h"

#define BLKDEV_NAME     "dynaswap"
#define BLK_SECTOR_SIZE  512
#define BLK_CAPACITY_GB  128

static int block_major = 0;
static struct gendisk *dynaswap_disk;
static struct blk_mq_tag_set tag_set; // TODO: what is this?
static struct xarray dynaswap_mapping;

#pragma region Request Handling

static unsigned long dynaswap_get_map_size(void) {
    unsigned long count = 0;
    void *entry;
    unsigned long index;

    xa_for_each(&dynaswap_mapping, index, entry) {
        count++;
    }

    return count;
}

static blk_status_t dynaswap_handle_rq(struct blk_mq_hw_ctx *hctx, const struct blk_mq_queue_data *bd) {
    struct request *req = bd->rq;

    pr_debug("dynaswap: beginning request\n");
    blk_mq_start_request(req);

    unsigned int operation = req_op(req);
    sector_t req_position = blk_rq_pos(req);
    pr_debug("dynaswap: request at pos %llu (op %u)\n", 
             (unsigned long long)req_position, operation);

    if (operation == REQ_OP_DISCARD) {
        sector_t start = blk_rq_pos(req);
        sector_t end = start + blk_rq_sectors(req);
        pr_debug("dynaswap: discarding from swap (idx %llu - idx %llu)\n", start, end);
        
        for (; start < end; start += 8) { // Jump by page sizes
            unsigned long index = start >> 3;
            struct page *p = xa_erase(&dynaswap_mapping, index);
            if (p) {
                __free_page(p); // Actually give the memory back to the system!
            }
        }

        #ifdef DEBUG

        auto dynaswap_page_count = dynaswap_get_map_size();
        pr_info("dynaswap: currently holding %lu pages (%lu KB)\n",
            dynaswap_page_count, dynaswap_page_count * 4);

        #endif

        blk_mq_end_request(req, BLK_STS_OK);
        return BLK_STS_OK;
    }

    struct bio_vec bvec;
    struct req_iterator iter;
    rq_for_each_segment(bvec, req, iter) {
        sector_t current_sector = iter.iter.bi_sector;
        unsigned long page_index = current_sector >> (PAGE_SHIFT - 9);

        void *kaddr = kmap_local_page(bvec.bv_page);

        struct page *p;
        void *mem_page = bvec.bv_offset + kaddr;
        switch (operation) {
            case REQ_OP_WRITE: {
                pr_debug("dynaswap: write to swap (idx %lu)\n", page_index);
                p = xa_load(&dynaswap_mapping, page_index);
                if (!p) {
                    p = alloc_page(GFP_ATOMIC);
                }

                if (!p) {
                    blk_mq_end_request(req, BLK_STS_RESOURCE);
                    return BLK_STS_OK;
                }

                xa_store(&dynaswap_mapping, page_index, p, GFP_ATOMIC);

                void *dest = kmap_local_page(p);
                memcpy(dest, mem_page, bvec.bv_len);
                kunmap_local(dest);

                break;
            }
            case REQ_OP_READ: {
                pr_debug("dynaswap: reading from swap (idx %lu)\n", page_index);
                p = xa_load(&dynaswap_mapping, page_index);

                if (!p) {
                    memset(mem_page, 0, bvec.bv_len);
                    break;
                }

                void *src = kmap_local_page(p);
                memcpy(mem_page, src, bvec.bv_len);
                kunmap_local(src);

                break;
            }
        }

        kunmap_local(kaddr);
    }

    #ifdef DEBUG

    auto dynaswap_page_count = dynaswap_get_map_size();
    pr_info("dynaswap: currently holding %lu pages (%lu KB)\n",
         dynaswap_page_count, dynaswap_page_count * 4);

    #endif

    blk_mq_end_request(req, BLK_STS_OK);
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
    xa_init(&dynaswap_mapping);
    pr_debug("dynaswap: mapping initialized\n");
    return 0;
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
    tag_set.flags = 0;

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
    capacity = BLK_CAPACITY_GB * 1024 * 1024 * 2;
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