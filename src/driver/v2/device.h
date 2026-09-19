#ifndef DEVICE_H
#define DEVICE_H

#include "linux/types.h"

#define BLKDEV_NAME "dynaswap"
#define BLOCK_CAPACITY (BLOCK_CAPACITY_GB << 30)

extern ullong BLOCK_CAPACITY_GB;

bool setup_disk(void);
void teardown_disk(void);

#endif