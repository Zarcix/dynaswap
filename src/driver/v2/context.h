#ifndef CONTEXT_H
#define CONTEXT_H

#include <linux/blk_types.h>
#include <linux/workqueue.h>

struct context {
    struct work_struct extend_work;
    struct work_struct truncate_work;

};

blk_status_t dynaswap_read(sector_t sector, struct page *page);
blk_status_t dynaswap_write(sector_t sector, struct page *page);
blk_status_t dynaswap_discard(void);

int setup_context(void);
void teardown_context(void);

#endif