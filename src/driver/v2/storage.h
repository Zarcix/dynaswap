#ifndef STORAGE_H
#define STORAGE_H

#include <linux/blkdev.h>
#include <linux/fs.h>

#define STORAGE_OPEN_FLAGS O_DIRECT | O_RDWR | O_CREAT | O_TRUNC | O_LARGEFILE
#define STORAGE_PERMISSIONS S_IRUSR | S_IWUSR

struct storage {
    struct file *file;
};

int extend_storage(struct storage *store);

int read_storage(struct storage *store);
int write_storage(struct storage *store);

int init_storage(struct storage *store);
void free_storage(struct storage *store);

#endif