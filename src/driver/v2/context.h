#ifndef CONTEXT_H
#define CONTEXT_H

#include <linux/blk_types.h>
#include <linux/workqueue.h>

#define CHUNK_SIZE ((unsigned long)(CHUNK_SIZE_MB * 1024 * 1024))

struct context {
    struct work_struct extend_work;
    struct work_struct truncate_work;

    struct rw_semaphore work_sem;
};

int dynaswap_read(sector_t sector, struct page *page);
int dynaswap_write(sector_t sector, struct page *page);
int dynaswap_discard(void);

int setup_context(void);
void teardown_context(void);

#endif