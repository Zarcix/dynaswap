#include "dynamap.h"

#pragma region Setup and Teardown

#define FILE_OPEN_FLAGS O_RDWR | O_CREAT | O_EXCL | O_LARGEFILE

int dynamap_init(struct dynamap_ctx *ctx, const char *path) {
    xa_init(&ctx->xa_virt_to_phys);
    xa_init(&ctx->xa_phys_to_virt);

    INIT_LIST_HEAD(&ctx->free_slots);
    ctx->total_slots = 0;
    atomic_set(&ctx->active_slots, 0);

    ctx->backing_file = filp_open(path, FILE_OPEN_FLAGS, 0600);
    if (IS_ERR(ctx->backing_file)) {
        pr_err("dynaswap: Failed to open backing file %s\n", path);
        return PTR_ERR(ctx->backing_file);
    }

    pr_info("dynaswap: initialization of dynaswap backing finished\n");
    return 0;
}

void dynamap_cleanup(struct dynamap_ctx *ctx) {
    if (!ctx) return;

    pr_info("dynaswap: starting cleanup of backing storage\n");

    if (ctx->backing_file && !IS_ERR(ctx->backing_file)) {
        struct path path = ctx->backing_file->f_path;
        struct dentry *dentry = path.dentry;
        struct inode *dir = d_inode(dentry->d_parent);

        inode_lock_nested(dir, I_MUTEX_PARENT);
        vfs_unlink(&nop_mnt_idmap, dir, dentry, NULL);
        inode_unlock(dir);

        filp_close(ctx->backing_file, NULL);
        ctx->backing_file = NULL;

        pr_debug("dynaswap: backing file closed\n");
    }

    xa_destroy(&ctx->xa_virt_to_phys);
    xa_destroy(&ctx->xa_phys_to_virt);
    pr_debug("dynaswap: mapping xarrays destroyed\n");

    struct slot_entry *entry, *next;
    list_for_each_entry_safe(entry, next, &ctx->free_slots, list) {
        list_del(&entry->list);
        kfree(entry);
    }
    pr_debug("dynaswap: free slot list cleared\n");

    ctx->total_slots = 0;
    atomic_set(&ctx->active_slots, 0);

    pr_info("dynaswap: cleanup finished\n");
}

#pragma endregion
