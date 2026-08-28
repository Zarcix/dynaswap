#ifndef COMMON_H
#define COMMON_H

#define DEBUG

#define log_err(fmt, ...) pr_err("dynaswap::%s -- " fmt "\n", __FILE__, ##__VA_ARGS__)
#define log_debug(fmt, ...) pr_debug("dynaswap::%s -- " fmt "\n", __FILE__, ##__VA_ARGS__)
#define log_info(fmt, ...) pr_info("dynaswap::%s -- " fmt "\n", __FILE__, ##__VA_ARGS__)
#define log_alert(fmt, ...) pr_alert("dynaswap::%s -- " fmt "\n", __FILE__, ##__VA_ARGS__)

#include <linux/printk.h>
#include <linux/compiler.h>
#include <linux/types.h>

#define WORKQUEUE_FLAGS (WQ_MEM_RECLAIM | WQ_HIGHPRI | WQ_UNBOUND)

extern struct workqueue_struct *DYNASWAP_WORKQUEUE;

int setup_common(void);
void teardown_common(void);

#endif