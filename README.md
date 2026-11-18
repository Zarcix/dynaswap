# DynaSwap Kernel Driver

This combination of kernel patch and userspace service allows for swap space increase using event polling.

## Architecture

This acts similar to how the original pure userspace driver worked. The difference lies in how we get the triggers. This implementation adds a new event to the swap mechanics. The event triggers one of the following: Nothing, Extend, Truncate. The userspace driver reads those triggers and performs and action accordingly.

The new event swap mechanics implementation works by adding in a check every time the kernel does a swap event. This includes, using a swap entry (swapin), taking a swap entry (swapout), adding more swap entries (swapon), or removing swap entires (swapoff). A check goes through and looks at the current number of pages used and the total amount of pages, turns it into a usage percent, then compares it against an extend and truncate threshold. Events trigger to `/sys/kernel/mm/swap/pages/event`.

The userspace driver polls on `/sys/kernel/mm/swap/pages/event`. When an extend event comes through, the driver allocates a new file under the swap directory, adds swap headers, then swaps on the file. When a truncate event comes through, the driver swaps off the latest swap file and deletes the driver to restore space.

Extend and Truncate events are defined by percentage thresholds. These thresholds are able to be user set, but default to 80% full and 30% empty respectively. These should ideally never be at 50% for both because that woudld cause a lot of churn for the swap driver if usage is right at 50%.

The main reason for adding the kernel monitor is to avoid the issue where PSI reacts too late to memory pressure. By adding a page monitoring system, we can scale out swap files much earlier so the system doesn't hang. This also avoid the issues with file system allocation that we ran into with the block device driver since this is using the swap system itself and scaling out swap files during low system load.

## Issues

These will share some issues with the pure userspace driver, but this does solve a lot of the main pain points of that implementation.

1. You now need to compile the kernel to get these changes working.

    - This no longer uses any currently implemented stuff like PSI or meminfo parsing

    - The gate to entry is very high for this because of the kernel compilation

2. There is a limit to how many swap files you can allocate

    - If you set the per file swap size too low and hit the max amount of swap files, you will not be able to add any more.

    - This leads to a problem where you need to choose a small enough size so it increments, but a big enough file that you don't run out of swap entries.

    - Smaller entries also take longer to swapon so that's also not ideal.

## Building

There are two things that need to be built for this to work.

The first is building the kernel. The main thing about kernel building is that the patches need to be applied. I included examples of how to build using what I used for testing below. This could also maybe work with kernel patching, though I have not tried it myself.

The second is the userspace service. This service reads from the events the new kernel changes emit, so the running kernel changes are a prerequisite to this.

### Kernel Patch

Before trying to build the kernel, make sure the tag v7.3 is checked out for linux. This has only been tested on linux 7.3.

1. Go into the linux kernel directory

    - In this repo, there is a kernel cloned in `kernel/linux` as a git submodule

2. Patch the kernel with the patch file

```bash
# Make sure you're in the kernel directory
export PATCH_DIR="dir/to/swap-monitor.patch"

git apply $PATCH_DIR
```

3. Set up kernel configs

`make olddefconfig`

4. Build the kernel

`make -j$(nproc) bzImage`

### Userspace Service

In the top level directory of this repo, run:

1. `meson setup build`

2. `meson compile -C build`

The userspace driver will be in `builddir/src/dynaswap`

## Usage

### Kernel Usage

The kernel changes introduce a new path, `/sys/kernel/mm/swap/pages` that emits logging events and page entry details.

Configuration of thresholds is supported by modifying either the `extend_theshold` or `truncate_threshold` files under that path.

### Userspace Service Usage

You can start the userspace service by simply running `./dynaswap`

There are a few configuration options that can be set, namely `path` and `size`

- `path` - The path where the swap files will be created under

- `size` - How big each swap file should be
