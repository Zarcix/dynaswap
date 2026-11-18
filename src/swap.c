#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <uuid/uuid.h>
#include <sys/swap.h>

#include "include/log.h"
#include "include/swapheader.h"

#include "swap.h"

static uint32_t _mkswap_last_page(int fd) {
    struct stat swap_stat;
    if (fstat(fd, &swap_stat) != 0) {
        log_error("unable to read swap file descriptor %d", fd);
        return 0;
    }

    off_t swap_size = swap_stat.st_size;
    long page_size = sysconf(_SC_PAGESIZE);
    if (swap_size < page_size) {
        log_error("could not create swap, file too small");
        return 0;
    }

    return (swap_size / page_size) - 1;
}

static void mkswap(int fd) {
    struct swap_header_v1_2 header = {0};

    // Completely clear out disklabel part of header
    for (int i = 0; i < sizeof(header.bootbits); i++) {
        header.bootbits[i] = '\0';
    }

    // Set up some defaults based on the size
    uint32_t swap_version = htole32(SWAP_VERSION);
    uint32_t last_page = htole32(_mkswap_last_page(fd));
    uint32_t bad_pages = htole32(0);
    uuid_generate(header.uuid);

    // Actually write to the file
    write(fd, header.bootbits, sizeof(header.bootbits));
    write(fd, &swap_version, sizeof(uint32_t));
    write(fd, &last_page, sizeof(uint32_t));
    write(fd, &bad_pages, sizeof(uint32_t));
    write(fd, header.uuid, sizeof(header.uuid));
    write(fd, header.volume_name, sizeof(header.volume_name));

    // Finally, write the signature
    lseek(fd, SWAP_SIGNATURE_OFFSET, SEEK_SET);
    write(fd, SWAP_SIGNATURE, SWAP_SIGNATURE_SZ);
}

int create_swap(char *path, off_t swap_size) {
    errno = 0;
    FILE *file = fopen(path, "w");
    if (!file) {
        int err = errno;
        log_error(
            "failed to open %s for swap creation: %s",
            path,
            strerror(err)
        );
        
        return err;
    }

    errno = 0;
    int fd = fileno(file);
    if (fd == -1) {
        int err = errno;
        log_error(
            "failed to get the file descriptor for %s: %s",
            path,
            strerror(err)
        );
        return err;
    }

    errno = 0;
    if (fallocate(fd, 0, 0, swap_size) != 0) {
        int err = errno;
        log_error(
            "failed to fallocate file %s: %s",
            path,
            strerror(err)
        );
    }

    mkswap(fd);

    fclose(file);
    return 0;
}

int remove_swap(char *path) {
    int ret = 0;
    errno = 0;

    ret = swapoff(path);
    if (ret != 0) {
        int err = errno;
        log_error("failed to swap off %s: %s", path, strerror(err));
        return 1;
    }

    errno = 0;
    ret = remove(path);
    if (ret != 0) {
        int err = errno;
        log_error("failed to remove file %s: %s", path, strerror(err));
        return 1;
    }

    return 0;
}