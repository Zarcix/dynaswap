#ifndef COMMON_H
#define COMMON_H

#define DEBUG

#include <linux/printk.h>
#include <linux/compiler.h>

#define log_err(fmt, ...) pr_err("dynaswap::%s -- " fmt "\n", __FILE__, ##__VA_ARGS__)
#define log_debug(fmt, ...) pr_debug("dynaswap::%s -- " fmt "\n", __FILE__, ##__VA_ARGS__)
#define log_info(fmt, ...) pr_info("dynaswap::%s -- " fmt "\n", __FILE__, ##__VA_ARGS__)

#endif