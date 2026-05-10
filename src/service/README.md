# DynaSwap

DynaSwap is a userspace daemon that allocates and frees swap files dynamically
in response to live memory pressure on Linux systems.

## How It Works

The service runs as a long-lived loop, polling PSI for memory pressure events
and reacting by adding or removing swap chunks.

1. **Initialization**: parses CLI args (`-c <config>`) and reads the libconfig
   file. Opens `/proc/pressure/memory`, installs a PSI "some" trigger using the
   `PSI_SOME_AVG10` / `PSI_SOME_AVG60` thresholds, and allocates an initial
   swap chunk.
2. **Polling loop**: waits on the PSI fd for `POLLPRI`. The pressure state
   is reported as either `STRAINED` (trigger fired) or `RELAXED` (poll timed
   out).
3. **Allocation (`STRAINED`)**: if current swap usage exceeds
   `SWAP_FULL_THRESHOLD`, a new chunk file is created under `SWAP_PATH`
   (`Chunk0`, `Chunk1`, …), `fallocate`d to `SWAP_PART_SIZE`, given a v1.2
   swap header (written manually by `mkswap` in [src/swaphandler.c](src/swaphandler.c)),
   and activated via the `swapon(2)` syscall with `SWAP_FLAG_PREFER`. Chunks
   are tracked in a singly-linked list (`prog_swap`).
4. **Freeing (`RELAXED`)**: if there is more than one chunk, the 60-second
   PSI averages are below `PSI_SOME_STRESS` / `PSI_FULL_STRESS`, and swap
   usage has dropped under `SWAP_FREE_THRESHOLD`, the newest chunk is
   `swapoff`d and its file removed. The first chunk is always kept to avoid
   stalling the system.
5. **Shutdown**: `SIGINT` / `SIGTERM` / `SIGHUP` / `SIGQUIT` trigger
   `swapoff` and removal of every remaining chunk before exit.

## Building

Dependencies:

- `libconfig` (vendored as a submodule under `public/libconfig`, built
  statically by the Makefile)
- `libproc2` (procps-ng) — provides `procps_meminfo_*`
- `libuuid`

Steps:

1. Make sure the libconfig submodule is checked out: `git submodule update --init`
2. Run `make`
3. The binary is written to `build/dynaswap_<arch>`

## Running

There are two ways to run the service:

1. **Command line**
   - `sudo ./build/dynaswap_<arch> -c misc/dynaswap.conf`
   - Root is required: the program calls `swapon(2)` / `swapoff(2)` and
     creates files in `SWAP_PATH`.
2. **Systemd service**
   - Unit file: [misc/dynaswap.service](misc/dynaswap.service)
   - The unit expects the config at `/etc/dynaswap.conf`.

## Configuration

The config file is parsed with libconfig. A reference is provided at
[misc/dynaswap.conf](misc/dynaswap.conf). All keys are required:

| Key | Type | Meaning |
| --- | --- | --- |
| `SWAP_PATH` | string | Directory to hold chunk files; must already exist |
| `SWAP_PART_SIZE` | string | Size of each chunk, e.g. `"1G"`, `"512M"`, `"128K"` |
| `SWAP_FULL_THRESHOLD` | float (0, 1] | Swap usage fraction above which a new chunk is allocated under pressure |
| `SWAP_FREE_THRESHOLD` | float (0, 1] | Swap usage fraction below which a chunk can be freed |
| `PSI_SOME_STRESS` | int > 0 | 60s "some" PSI ceiling under which freeing is allowed |
| `PSI_FULL_STRESS` | int > 0 | 60s "full" PSI ceiling under which freeing is allowed |

