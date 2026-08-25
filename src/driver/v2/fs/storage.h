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

void extend_storage(void);
void truncate_storage(void);
void write_storage(void);
void read_storage(void);

int setup_storage(void);
void teardown_storage(void);

#endif