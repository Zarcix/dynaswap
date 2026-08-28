#include "common.h"

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/blkdev.h>

#include "storage.h"

static struct storage_context STORAGE_CONTEXT = {0};

/**
 * Module Arguments
 */

char *storage_location = NULL;
module_param_named(path, storage_location, charp, S_IRUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(path, "Path to the backing file");

/**
 * Functions
 */

int extend_storage(unsigned long current_slots, unsigned long added_slots) {
    loff_t offset = (loff_t)(current_slots << PAGE_SHIFT);

    loff_t len = (loff_t)(added_slots << PAGE_SHIFT);

    int ret = vfs_fallocate(STORAGE_CONTEXT.backing_file, 0, offset, len);

    if (unlikely(ret < 0)) {
        log_err("failed to extend backing storage (offset = %lld, len = %lld, err = %pe)",
                (long long)offset, (long long)len, ERR_PTR(ret));
        return ret;
    }

    return 0;
}

void truncate_storage(void) {}

int read_storage(unsigned long slot, struct page *dest) {
    loff_t offset = (loff_t)(slot << PAGE_SHIFT);

    void *kpage = kmap_local_page(dest);
    
    ssize_t ret = kernel_read(STORAGE_CONTEXT.backing_file, kpage, PAGE_SIZE, &offset);
    if (unlikely(ret != PAGE_SIZE)) {
        long err = (ret < 0) ? ret : -EIO;
        log_err(
            "failed to read from slot (slot = %lu, ret = %ld, err = %pe)",
            slot,
            ret,
            ERR_PTR(err)
        );
        memset(kpage, 0, PAGE_SIZE);
        kunmap_local(kpage);
        return (int)err;
    }

    kunmap_local(kpage);

    return 0;
}

int write_storage(unsigned long slot, struct page *src) {
    loff_t offset = (loff_t)(slot << PAGE_SHIFT);

    void *kpage = kmap_local_page(src);

    ssize_t ret = kernel_write(STORAGE_CONTEXT.backing_file, kpage, PAGE_SIZE, &offset);
    if (unlikely(ret != PAGE_SIZE)) {
        long err = (ret < 0) ? ret : -EIO;
        log_err(
            "failed to write to slot (slot = %lu, ret = %ld, err = %pe)",
            slot,
            ret,
            ERR_PTR(err)
        );
        kunmap_local(kpage);
        return (int)err;
    }

    kunmap_local(kpage);

    return 0;
}

/**
 * Teardown
 */

void teardown_storage(void) {
    if (STORAGE_CONTEXT.backing_file && !IS_ERR(STORAGE_CONTEXT.backing_file)) {
        struct inode *inode = file_inode(STORAGE_CONTEXT.backing_file);

        log_debug("removing write protection to backing storage");
        if (inode) {
            inode_lock(inode);
            put_write_access(inode);
            inode_unlock(inode);
        }

        log_debug("closing backing file");
        filp_close(STORAGE_CONTEXT.backing_file, NULL);

        log_debug("truncating backing file");
        vfs_truncate(&STORAGE_CONTEXT.backing_file->f_path, 0);

        STORAGE_CONTEXT.backing_file = NULL;
    }

    log_debug("clearing storage context");
    memset(&STORAGE_CONTEXT, 0, sizeof(STORAGE_CONTEXT));

    log_debug("backing file finished teardown");
}

/**
 * Setup
 */

int setup_storage(void) {
    struct inode *inode;
    int err;

    if (storage_location == NULL) {
        log_err("'path' parameter is required.");
        log_err("Usage: path=/path/to/file");
        return -EINVAL;
    }

    log_debug("opening backing file");
    STORAGE_CONTEXT.backing_file = filp_open(storage_location, STORAGE_OPEN_FLAGS, STORAGE_PERMISSIONS);
    if (IS_ERR(STORAGE_CONTEXT.backing_file)) {
        err = PTR_ERR(STORAGE_CONTEXT.backing_file);

        log_err("failed to open file %s", storage_location);
        STORAGE_CONTEXT.backing_file = NULL;

        return err;
    }

    inode = file_inode(STORAGE_CONTEXT.backing_file);

    inode_lock(inode);

    // Write access is put in here to not let users randomly modify memory
    log_debug("locking write access to backing file");
    err = get_write_access(inode);
    if (err) {
        inode_unlock(inode);
        log_err("failed to get write access for file %s", storage_location);
        filp_close(STORAGE_CONTEXT.backing_file, NULL);
        STORAGE_CONTEXT.backing_file = NULL;
        return err;
    }

    inode_unlock(inode);

    log_debug("backing file finished setup");
    return 0;
}
