#ifndef CONTEXT_H
#define CONTEXT_H

#define WORKQUEUE_FLAGS (WQ_MEM_RECLAIM | WQ_HIGHPRI | WQ_UNBOUND)

#include <linux/workqueue.h>

struct context {
    struct workqueue_struct *workqueue;
    struct work_struct extend_work;
    struct work_struct truncate_work;

};

void dynaswap_read(void);
void dynaswap_write(void);
void dynaswap_discard(void);

int setup_context(void);
void teardown_context(void);

#endif