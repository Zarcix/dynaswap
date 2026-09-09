#include "common.h"

#include "fs/storage_debug.h"
#include "fs/storage.h"

static ssize_t backing_file_size_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
    char buf[32];
    loff_t file_size = 0;

    /* Assuming you have a global or accessible storage context pointer */
    if (STORAGE_CONTEXT.backing_file) {
        struct inode *inode = file_inode(STORAGE_CONTEXT.backing_file);
        file_size = i_size_read(inode);
    }

    int len = snprintf(buf, sizeof(buf), "%lld\n", file_size);
    return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static const struct file_operations backing_file_size_fops = {
    .read = backing_file_size_read,
    .llseek = default_llseek,
    .open = simple_open,
};

void setup_storage_debug(void) {
    struct dentry *storage_sysfs_dir = debugfs_create_dir("storage", DYNASWAP_SYSFS_DIR);
    if (storage_sysfs_dir) {
        debugfs_create_file("backing_file_size_bytes", DEBUGFS_FLAGS, storage_sysfs_dir, NULL, &backing_file_size_fops);
    }
}