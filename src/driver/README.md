# DynaSwap (Block Driver)

A Linux kernel block driver that exposes a virtual block device, `/dev/dynaswap`,
backed by a single regular file. The backing file is created sparse and grows
on demand as the device is written to, so the on-disk footprint tracks actual
usage rather than the advertised capacity.

The device is a general-purpose block device. The intended use is to format it
with `mkswap` and activate it with `swapon`, but it can be used for any block
workload.

## How It Works

The driver presents a fixed logical capacity (`capacity_gb`) but defers
physical allocation until pages are actually written. A two-level mapping
translates each logical page of the device into a physical slot inside the
backing file.

1. **Logical → physical mapping** — every 4 KiB logical page on the device
   has an entry in a two-level radix-style table (`virt_to_phys_map`). An
   unmapped entry is `~0UL`. A free-slot bitmap (`slot_bitmap`) tracks
   which physical slots in the backing file are in use, and `slot_hint` is
   the rotating starting point for the next-fit search.
2. **Write** — looks up the logical page. If it has been written before,
   the existing physical slot is reused. Otherwise the next free slot is
   claimed from the bitmap, the mapping is recorded, and the page is
   written to the backing file at that offset via `kernel_write`.
3. **Read** — looks up the logical page. If unmapped, the request is
   served by zeroing the page (no I/O). Otherwise the corresponding slot
   is read from the backing file via `kernel_read`.
4. **Discard** — clears the mapping, releases the slot in the bitmap, and
   punches a hole in the backing file (`vfs_fallocate` with
   `FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE`) to reclaim disk space.
   Processed in batches of 512 entries with `cond_resched` between batches.
5. **Auto-extend** — on every write, if the fraction of active slots
   relative to currently-allocated slots is at or above `inflate_threshold`,
   an extend job is queued on the driver's workqueue. The worker
   `fallocate`s an additional `chunk_size` MiB onto the backing file (up to
   `capacity_gb`), making more slots available.

All requests come in through `blk-mq` and are deferred to the system
workqueue so the request path itself is non-blocking. Concurrent access to
the mapping is guarded by an `rw_semaphore`.

Source map:

- [dynaswap_drv.c](dynaswap_drv.c) — module entry/exit, `blk-mq` setup, request dispatch
- [dynamap.c](dynamap.c) — read/write/discard, slot allocation, auto-extension
- [dynamap.h](dynamap.h) — `dynamap_ctx` definition and public API
- [helpers.py](helpers.py) — utility for estimating the RAM cost of the
  in-kernel mapping structures at a given device capacity
- [dkms.conf](dkms.conf) — DKMS package descriptor

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

