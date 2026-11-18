# DynaSwap Block Driver

This block device driver acts as a scalable block device that swap can read and write from.

## Architecture

Instead of just allocating and freeing up swap files, I opted to try going down the block device driver route.

The way this works is by creating a huge block device that exceeds the swap space you need. The driver will then map requests coming from the kernel to write to the block device to slots in a backing file that holds the swap data.

This driver contains two parts:

1. Slot Manager

2. Backing File Management

### Slot Manager

There is a slot manager that is a part of this driver. It functions as a two way map to map block device virtual sectors to real backing file sectors.

It handles the following case:

- Imagine a block device is registered for 1TB

- The kernel requests to swap out to block sector 123

- The driver would read the backing file slots and see that slot 4 is open. Slot in this case is a 4096 byte offset. Slot 0 is 0-4095. Slot 1 is 4096-8191.

- This allows us to map a gigantic block device to a small backing file.

### Backing File Management

This section mainly describes how the slot are actually handled in the backing file. When read and writes come through the slot manager, slots are directly written to the backing file. For example:

- Slot Manager finds that slot 23 is open, the backing file management will write to offset 94208 (23 * 4kb) in the backing file

In addition to this, there is additional managment for expanding and truncating files.

- Expansions occur when the total free slot count passes a specific threshold. The threshold is specified by a passed in option to the driver. The expansion allocates more space in the backing file then adds those new slots to the free backing file slot list.

- Truncations occur when the total free slot count goes under a specific threshold. Truncations happen by performing a hole punch for that slot. This is controlled by the kernel calling `discard` on the block device.

## Issues

1. This does not work at all with COW file systems

    - This is a problem with all swap files. If the backing file lives on a COW FS, the slot management in the backend will not work because of `bmap`

2. File System memory reclaim causes unavoidable deadlocks

    - When the system is running out of memory, the FS sends out a low memory signal to the kernel to try clearing up more space for writes

    - Since the writes coming in are going through the file system, this creates a circular deadlock over time

    - The pipeline for this looks like:
    
      1. Memory is low

      2. Driver tries to move out pages (waiting to finish)

      3. FS driver sees that there is not a lot of memory, sends out a swap out request to the kernel

      4. Kernel gets new request and sends to driver

      5. Driver tries to move out pages (deadlock from the last attempted move)

3. Inherently more risky

    - This block device driver is directly mapping swapped out memory. If swapped out memory is not returned correctly, applications using that block of memory will have incorrect data.

    - This does NOT hard fail, it causes undefined behavior for the application. It may SEGFAULT, it may keep running. Overall, it is unstable and hard to test.

## Building

Dependencies:

- Linux kernel headers for the running kernel (`/lib/modules/$(uname -r)/build`)
- A kernel-compatible toolchain (`gcc` or `clang`, matching how your kernel
  was built)

### Manual build

```sh
make
```

This produces `dynaswap.ko` in the current directory.

### DKMS

[dkms.conf](dkms.conf) is provided so the module can be installed against
every kernel on the system and rebuilt automatically across kernel upgrades.

```sh
sudo cp -r . /usr/src/dynaswap-1.0
sudo dkms add -m dynaswap -v 1.0
sudo dkms build -m dynaswap -v 1.0
sudo dkms install -m dynaswap -v 1.0
```

## Running

Load the module, pointing it at a backing file path:

```sh
sudo insmod dynaswap.ko path=/var/lib/dynaswap.bin capacity_gb=128
```

The `path` parameter is required; the file is opened with
`O_RDWR | O_CREAT | O_TRUNC | O_DIRECT | O_LARGEFILE`, so any existing
contents will be overwritten.

Once loaded, `/dev/dynaswap` appears. To use it as swap:

```sh
sudo mkswap /dev/dynaswap
sudo swapon /dev/dynaswap
```

To unload:

```sh
sudo swapoff /dev/dynaswap
sudo rmmod dynaswap
```

## Module Parameters

| Parameter | Type | Default | Meaning |
| --- | --- | --- | --- |
| `path` | string | (required) | Path to the backing file. Created if missing; truncated if present. |
| `capacity_gb` | uint | `128` | Logical capacity of `/dev/dynaswap` in GiB. The backing file will not grow beyond this. |
| `inflate_threshold` | byte (%) | `80` | Active-slot percentage at which the backing file is grown by another chunk. Writable at runtime via sysfs. |
| `chunk_size` | ulong (MiB) | `256` | Size of each auto-extension step applied to the backing file. |
