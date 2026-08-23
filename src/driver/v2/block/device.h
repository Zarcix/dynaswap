#ifndef DEVICE_H
#define DEVICE_H

#define BLKDEV_NAME "dynaswap"

#define BLOCK_CAPACITY (BLOCK_CAPACITY_GB * 1024ULL * 1024ULL * 2ULL)

void setup_disk(void);
void teardown_disk(void);

#endif