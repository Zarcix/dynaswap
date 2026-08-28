/* Slot Information

This sections is pretty much required because this is the bread and butter of how this thing's gonna work.

We have two different "types" here that we will be working with: a page and a slot

A page essentially reduces a sector down to a virtual index.
- This is done to drastically shrink the key space for bimap
- Each incoming sector is 

A slot represents memory page mapping in the backing file.
- For example, slot 0 represents a backing file offset of 0-4095. Slot 1 represents an offset of 4096-8191.
- Offset comes from memory pages being 4KB by default. This is defined by the var PAGE_SIZE
- This essentially chunks the backing file into the respective backing file.
- Another way of thinking about this is a ton of smaller files being squeezed together into one backing file. Each smaller file is 4kb in size.
*/


#ifndef FS_SLOT_H
#define FS_SLOT_H

#include <linux/atomic/atomic-long.h>
#include <linux/blk_types.h>
#include <linux/bitmap.h>
#include <linux/xarray.h>

#define SECTORS_PER_PAGE_SHIFT (PAGE_SHIFT - SECTOR_SHIFT)

struct slot_manager {
    /**
     * Slots here refer to the backing file slots, not the swap file slots.
     * This difference is important since when the storage driver checks for a free slot, it's checking the backing file for space 
     */
    unsigned long *slot_bitmap;
    unsigned long bitmap_size;
    unsigned long bitmap_hint;

    struct xarray page_to_slot;
    struct xarray slot_to_page;

    atomic_long_t total_slots;
    atomic_long_t active_slots;
};

/* Helpers */

bool slot_manager_needs_extend(void);
unsigned long get_total_slots(void);

/* Slot Functionality */

void extend_slots(unsigned long new_slots);
int get_write_slot(sector_t sector, unsigned long *slot);


/* Init */

int setup_slot_manager(void);
void teardown_slot_manager(void);

#endif