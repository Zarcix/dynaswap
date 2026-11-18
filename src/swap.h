#ifndef SWAP_H
#define SWAP_H

#include <fcntl.h>

int create_swap(char *path, off_t swap_size);
int remove_swap(char *path);

#endif