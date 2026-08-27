#ifndef FS_STORAGE_H
#define FS_STORAGE_H

#define STORAGE_OPEN_FLAGS (O_DIRECT | O_RDWR | O_CREAT | O_TRUNC | O_LARGEFILE)
#define STORAGE_PERMISSIONS (S_IRUSR | S_IWUSR)

#include <linux/fs.h>
#include <linux/rwsem.h>

struct storage_context {
    struct file *backing_file;

    struct rw_semaphore work_sem;
};

int extend_storage(unsigned long current_slots, unsigned long added_slots);
void truncate_storage(void);
int read_storage(unsigned long slot, struct page *dest);
int write_storage(unsigned long slot, struct page *page);

int setup_storage(void);
void teardown_storage(void);

#endif