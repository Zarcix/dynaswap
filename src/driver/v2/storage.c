#define DEBUG

#include <linux/fs.h>

#include "common.h"
#include "storage.h"

static char *store_path = NULL;
module_param_named(path, store_path, charp, S_IRUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(path, "Path to the backing file for DynaSwap");

static long long extend_size = 128 << 20;

int extend_storage(struct storage *store) {
    struct file *file = store->file;

    struct inode *inode = file_inode(file);

    loff_t offset = inode->i_size;
    int ret = vfs_fallocate(file, 0, offset, extend_size);

    inode_unlock(inode);
    if (ret) return -ENOSPC;

    return 0;
}

int read_storage(struct storage *store) {
    return 0;
}

int write_storage(struct storage *store) {
    return 0;
}

int init_storage(struct storage *store) {
    store->file = filp_open(store_path, STORAGE_OPEN_FLAGS, STORAGE_PERMISSIONS);
    if (IS_ERR(store->file)) {
        log_err("failed to initialize backing file for path %s", store_path);
        return PTR_ERR(store->file);
    }
    return 0;
}

void free_storage(struct storage *store) {
    if (store->file) {
        filp_close(store->file, 0);
    }
}