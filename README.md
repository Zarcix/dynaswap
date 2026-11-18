# DynaSwap Userspace Driver

This is the version of dynaswap implementation.

## Monitoring

This uses both Pressure Stall Information (PSI) and memory use counts to see when a scale up is needed.

Thresholds for both PSI and memory use are configurable in the `src/misc/dynaswap.conf` file in this repo.

### PSI Monitoring

The way PSI metrics work is it reads out how many tasks are stalled based on some metric. In this case, we monitor the memory stalls since those app stalls are caused by memory pressure.

This means that the only time we get PSI metrics is when there is considerable system load.

Using PSI and polling also gives us a good way to not do any busy waiting, but to instead poll and wait on PSI monitoring first.

Think of this as the main "wakeup" signal for the service to start swapping out.

### Memory Monitoring

Memory monitoring has a pretty simple backing formula to it. If the memory usage is above the full threshold, we add more swap files until we get below that threshold. If the memory usage is below the free threshold, we remove the latest swap file until we get above that threshold.

Full threshold represents when the swap space is about to be completely taken up.

Free threshold represents low demand for memory, meaning that we should reduce the amount of swap to hand back to the user.

## Scaling

There are two variables that can be user set that relate to scaling:

- Path to scale in

- Scale step size

When first starting the application, there is one swap file that is activated first. The first swap file will be created in the path to scale in.

Subsequent swap files will follow a similar naming convention but with `_{num}` appended to the end.

Swap files are also swapped off in numerical order, but with the reverse order. 

- For example, imagine we have 5 swap files active. Swapoff will be 5, 4, 3, and 2. 1 is never swapped off since we need to have at least one swap file to run.

## Drawbacks

1. Polling on PSI is not a good metric for overall system load

    - PSI is great for slow memory growth as that leads to slow and steady PSI climb

    - PSI is not great for memory spikes. Memory is generally not swapped out until the system *must* swap out. In this situation, PSI stays relatively low (< 1), then jumps up to 30+ once pages are swapped out.

    - The reaction for this is just simply too slow for this reason. We are able to scale with pressure, but the pressure monitor is not good.

2. PSI metrics do not directly map to swap usage

    - PSI reads process stall which means it's indirectly only monitoring hot memory usage

    - If there is an application that is being stalled *because* it's being swapped out, PSI will have no idea.

   - In this case, it is not a failure or anything we really need to do in this case, it's just extra added noise to the system.
  
3. This uses swap files for growth

    - Swap files have a limit of 32 for the entire system

    - If each swap space is too small, we will not be taking up enough space with the 32
    
    - If we make it too large, we will be wasting space

    - Swap files require allocation which may cause swap failures even though we are increasing swap. This is compound with the late PSI monitoring issue described above.

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

