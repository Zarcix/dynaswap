#include <linux/debugfs.h>
#include "./dynaswap_sys.h"
#include "./dynamap.h"

static struct dentry *debug_dir;

static int stats_show(struct seq_file *m, void *v)
{
    long total = atomic_long_read(&dynamap.total_slots);
    long active = atomic_long_read(&dynamap.active_slots);
    long free_llist_count = 0;
    struct llist_node *pos;
    
    // Struct sizes
    size_t entry_size = sizeof(struct slot_entry);
    
    // 1. Calculate llist (Free Pool) count
    llist_for_each(pos, dynamap.free_slots.first) {
        free_llist_count++;
    }

    // 2. RAM calculations
    unsigned long free_ram = free_llist_count * entry_size;
    unsigned long active_ram = active * entry_size;
    
    // 3. XArray Internal Overhead estimation
    // Each xa_node is ~576 bytes and handles 64 entries.
    // This is a rough kernel estimate; xa_get_marks or similar is more complex.
    unsigned long xa_overhead = (active / 64) * 576; 

    seq_printf(m, "--- DynaSwap Logical Stats ---\n");
    seq_printf(m, "Swap Slot Size:          4 KB\n");
    seq_printf(m, "Total Managed Slots:     %ld\n", total);
    seq_printf(m, "Active Slots (Mapped):   %ld\n", active);
    seq_printf(m, "Free Slots (Pool):       %ld\n", free_llist_count);
    
    seq_printf(m, "\n--- Data Metrics (Virtual) ---\n");
    seq_printf(m, "Current Swap Capacity:   %ld KB\n", total * 4);
    seq_printf(m, "Actual Data Swapped:     %ld KB\n", active * 4);
    seq_printf(m, "Fill Ratio:              %ld%%\n", total ? (active * 100 / total) : 0);

    seq_printf(m, "\n--- Driver RAM Overhead (Real) ---\n");
    seq_printf(m, "Struct slot_entry size:  %zu bytes\n", entry_size);
    seq_printf(m, "Free Pool RAM:           %lu KB\n", free_ram / 1024);
    seq_printf(m, "Active Metadata RAM:     %lu KB\n", active_ram / 1024);
    seq_printf(m, "XArray Internal Est:     %lu KB\n", xa_overhead / 1024);
    seq_printf(m, "Total Driver Footprint:  %lu KB\n", (free_ram + active_ram + xa_overhead) / 1024);

    return 0;
}

static int stats_open(struct inode *inode, struct file *file)
{
    return single_open(file, stats_show, NULL);
}

static const struct file_operations stats_fops = {
    .owner = THIS_MODULE,
    .open = stats_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

int dynaswap_debugfs_init(void)
{
    debug_dir = debugfs_create_dir("dynaswap", NULL);
    if (!debug_dir)
        return -ENOMEM;

    // 1. A formatted summary file using the Sequence File API
    debugfs_create_file("stats", 0444, debug_dir, NULL, &stats_fops);

    // 2. Direct access to atomic counters for scripting
    debugfs_create_atomic_t("active_slots", 0444, debug_dir, (atomic_t *)&dynamap.active_slots);
    debugfs_create_atomic_t("total_slots", 0444, debug_dir, (atomic_t *)&dynamap.total_slots);

    return 0;
}

void dynaswap_debugfs_exit(void)
{
    debugfs_remove_recursive(debug_dir);
}