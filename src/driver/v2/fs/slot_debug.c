#include "common.h"

#include <linux/debugfs.h>
#include <linux/uaccess.h>

#include "fs/slot_debug.h"
#include "fs/slot.h"

static struct dentry *slot_sysfs_dir = NULL;

static ssize_t atomic_ulong_read_file(struct file *file, char __user *user_buf,
                                      size_t count, loff_t *ppos)
{
    atomic_long_t *val_ptr = file->private_data;
    char buf[32];
    long val = atomic_long_read(val_ptr);
    int len = snprintf(buf, sizeof(buf), "%ld\n", val);
    return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static const struct file_operations atomic_ulong_fops = {
    .read = atomic_ulong_read_file,
    .llseek = default_llseek,
    .open = simple_open,
};

static ssize_t free_slots_read_file(struct file *file, char __user *user_buf,
                                    size_t count, loff_t *ppos)
{
    char buf[32];
    long total = atomic_long_read(&SLOT_MANAGER.total_slots);
    long active = atomic_long_read(&SLOT_MANAGER.active_slots);
    long free = (total > active) ? (total - active) : 0;
    int len = snprintf(buf, sizeof(buf), "%ld\n", free);
    return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static const struct file_operations free_slots_fops = {
    .read = free_slots_read_file,
    .llseek = default_llseek,
    .open = simple_open,
};

static ssize_t usage_percent_read_file(struct file *file, char __user *user_buf,
                                       size_t count, loff_t *ppos)
{
    char buf[32];
    long total = atomic_long_read(&SLOT_MANAGER.total_slots);
    long active = atomic_long_read(&SLOT_MANAGER.active_slots);
    long percent = (total > 0) ? (active * 100) / total : 0;
    int len = snprintf(buf, sizeof(buf), "%ld%%\n", percent);
    return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static const struct file_operations usage_percent_fops = {
    .read = usage_percent_read_file,
    .llseek = default_llseek,
    .open = simple_open,
};

void setup_slot_debug(void) {
    slot_sysfs_dir = debugfs_create_dir("slots", DYNASWAP_SYSFS_DIR);
    if (!slot_sysfs_dir)
        return;

    debugfs_create_file("total_slots", DEBUGFS_FLAGS, slot_sysfs_dir, &SLOT_MANAGER.total_slots, &atomic_ulong_fops);
    debugfs_create_file("active_slots", DEBUGFS_FLAGS, slot_sysfs_dir, &SLOT_MANAGER.active_slots, &atomic_ulong_fops);
    debugfs_create_file("free_slots", DEBUGFS_FLAGS, slot_sysfs_dir, NULL, &free_slots_fops);
    debugfs_create_file("slot_usage_percent", DEBUGFS_FLAGS, slot_sysfs_dir, NULL, &usage_percent_fops);
}
