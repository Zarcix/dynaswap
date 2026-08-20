# Service Implementation

This covers how the Dynaswap functions as a service. This will cover the swaphandling system that was created to be used in tandem with systemd.

## Implementation

The service was implemented as an executable that checks current system memory pressure. Once the system memory pressure exceeds a predefined baseline, a new swap file is `fallocate`d and added to the total system swap. If system memory pressure falls under the free threshold, the neweset swap files will be freed and those files will be deleted.

## Swap Creation

This service has it's own swap allocation function. The steps for this were pulled from how swap files get generated in the first place. This goes through the following steps:

1. Allocate a new file via `fallocate`

2. Run `mkswap` on the file. The `mkswap` function was reverse engineered from how the `mkswap` command runs.

3. Swap on the new swap file

4. Update memory chunks

    - This is just a linked list with all the swap files linked together

## System Memory Pressure Information

This service reads from a variety of kernel memory pressure leads: meminfo via procps, swpa usage, free direct memory, and PSI. 

### MemInfo

This is the simplest metric that we can get. Similar info can be found by running `free` to get memory information. In this case, we specifically calculate:

- Total Memory (MEMINFO_MEM_TOTAL)

- Total Swap (MEMINFO_SWAP_TOTAL)

- Available Memory (MEMINFO_MEM_AVAILABLE)

- Available Swap (MEMINFO_SWAP_FREE)

This was originally the only metric used for this service. However, we ran into a problem where memory spikes would not be handled fast enough by this. Thus, we added PSI metrics to the handler

### PSI

PSI Polling was implemented to move the swap allocation from reactive to proactive. If the kernel starts noticing some memory pressure, the pressure values will be passed into the service to be used. Metrics for this are taken from 2 overlying metrics: `Some` and `Full`. The `some` metric relays that there is a partial slowdown due to at least one process waiting for a resource. The `full` metric relays that all non idle tasks are waiting at the same time.

For both metrics, we read the avg from the last 10, 60, and 300 seconds.

## Implementation Issues

There are two bottlenecks here that lead to the service being unreliable:

1. Swap allocation is on a race condition against the system itself

    - Since the swap checker runs in a loop, potential memory usage that happens in the middle of that leads to a race condition spawning. In this case, the swap service is trying to add a swap file while there is no more memory to actually run that request.

    - There is realistically no real way of fixing this via a service since there is no way for a service to predict the future on when a swap allocation will be needed.

2. There is a limited amount of swap allocations that you are allowed to do

    - On a standard linux kernel compilation, the limit of swapfiles that can be added are 32 files. That being said, each of the files can be set to be very large as the limit is 16 TB per swap space.
    
    - This is not ideal however since you would essentially have to tweak the size of each swap partition to work with this.

3. The swap check is run in an event loop leading to higher power usage

    - This is with the implementation of this service itself. There is no way to see when an allocation is needed without polling the memory information

    - This leads to very high power usage if the event loop is set to run very quickly. 

    - Increasing the vent loop also leads to high CPU overhead to just run the loop. This leads to system slowdown too.