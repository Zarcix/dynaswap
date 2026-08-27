#include "common.h"

#include <linux/module.h>
#include <linux/fs.h>

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

void extend_storage(void) {}

void truncate_storage(void) {}

void read_storage(void) {}

void write_storage(void) {}

/**
 * Teardown
 */

void teardown_storage(void) {
    if (STORAGE_CONTEXT.backing_file && !IS_ERR(STORAGE_CONTEXT.backing_file)) {
        struct inode *inode = file_inode(STORAGE_CONTEXT.backing_file);

        if (inode) {
            inode_lock(inode);
            inode->i_flags &= ~S_SWAPFILE;
            put_write_access(inode);
            inode_unlock(inode);
        }

        filp_close(STORAGE_CONTEXT.backing_file, NULL);
        STORAGE_CONTEXT.backing_file = NULL;
    }

    memset(&STORAGE_CONTEXT, 0, sizeof(STORAGE_CONTEXT));
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

    init_rwsem(&STORAGE_CONTEXT.work_sem);

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
    err = get_write_access(inode);
    if (err) {
        inode_unlock(inode);
        log_err("failed to get write access for file %s", storage_location);
        filp_close(STORAGE_CONTEXT.backing_file, NULL);
        STORAGE_CONTEXT.backing_file = NULL;
        return err;
    }

    // This is here to give the file the same perms as a normal swap file, like rm denial
    inode->i_flags |= S_SWAPFILE;

    inode_unlock(inode);

    return 0;
}
