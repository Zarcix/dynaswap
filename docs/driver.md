# Driver Implementation

To avoid the problems of the service implementation, a driver implementation was created. This creates a block device that has reads and writes forwarded over to a fully grow and shrinkable file. This leads to us being able to set the swap size to be an arbitrarily high number where the driver itself will handle the backing file.

## V1 Implementation

This implementation is much simpler than the service implementation. Instead of polling a PSI to see what is starting to drag along, this implements directly as a driver. As part of the driver, a concept of `slots` are used.

### Slot Description

A `slot` represents a 4KB page in the backing file. When the kernel requests to swap out a memory page, the driver looks for a free slot and copies the data in that memory page into the slot.

#### Example

`-` = Open Page
`#` = Used Page

File A Slot Bitmap (16 KB): ---- (16 KB / 4 KB a page/slot = 4)

Driver Virt to Phys Map: ----

Driver Phys to Virt map: ----

---

Step 0: Kernel requests to swap out page 23

1. Driver looks for free slot in File A slot mapping.

2. Driver finds free slot at slot 0.

3. Driver marks slot 0 as being used.

4. Driver then performs a copy from kernel memory page 23 to the backing file's slot 0.

5. Driver updates virt to phys map to now hold virt[23] = 0 (This is 23 because virt = page)

6. Driver updates phys to virt map to hold phys[0] = 23 (This is 0 because slot = physical address)

Here is what the slots look like now:

File A Slot Bitmap (16 KB): #--- (16 KB / 4 KB a page/slot = 4)

Driver Virt to Phys Map: ----....0

Driver Phys to Virt map: (23)---

---

Step 1: Kernel requests to get page 23

1. Driver reads virt to phys to find where the memory map is (in this case reads 23)

2. Driver then copies the data from physical slot 0 to page 23.

Nothing in the slots have changed at all

---

Step 2: Kernel Requests to swap out page 578

1. Driver looks for free slot in File A slot mapping

2. Driver finds free slot at slot 1.

3. Driver marks slot 1 as being used

4. Driver performs copy from page 578 to slot 1

5. Driver updates virt to phys map to virt[578] = 1

6. Driver updates phys to virt map to phys[1] = 578

**Before**:

File A Slot Bitmap (16 KB): #--- (16 KB / 4 KB a page/slot = 4)

Driver Virt to Phys Map: ----....[23]=(0)

Driver Phys to Virt map: (23)---

**After**:

File A Slot Bitmap (16 KB): ##-- (16 KB / 4 KB a page/slot = 4)

Driver Virt to Phys Map: ----...[23]=(0)..............[578]=(1)

Driver Phys to Virt map: (23)(578)--

---

Step 3: Kernel Requests Discard of Page 23

1. Driver looks at phys map to virt to find used slot

2. Driver finds slot 0 being used

3. Driver removes slot 0 from the slot bitmap and performs fallocate hole punch to reclaim space

4. Driver removes virt to phys map binding for index 23

5. Driver removed phys to virt map binding for slot 0 that was found earlier

**Before**:

File A Slot Bitmap (16 KB): ##-- (16 KB / 4 KB a page/slot = 4)

Driver Virt to Phys Map: ----...[23]=(0)..............[578]=(1)

Driver Phys to Virt map: (23)(578)--

**After**:

File A Slot Bitmap (16 KB): -#-- (16 KB / 4 KB a page/slot = 4)

Driver Virt to Phys Map: ----..............[578]=(1)

Driver Phys to Virt map: -(578)--

---

Step 4: Driver Notices Unused Space

**This was left unimplemented due to hole punching**

### Issues with Driver and Development

1. The maps waste a lot of space depending on how big the block device is. This is due to how the maps are both dense maps. This was barely overcome by doing a 2d array instead but the problem still persists. This is exacerbated by the pre run malloc being run as the maps are always pre compiled to the maximum size.

2. Holes get created A LOT. Since these are holes and not contiguous blocks, there is a very high chance that the backing file gets very very large the longer you use the swap.

3. There is no actual implemented reclaim functionality. This was not possible with this setup as there is no fast and easy way to loop from the last bitmap up to the first with the speed needed for swap.

4. Potentially slow implementation of reads and writes. There is a linux feature called BIO. That was not investigated here and may result in faster read write times.

5. No real openings for debugging at all. No sysfs openings were ever made to look into this. This led to debugging being very difficult.

6. Workqueues were implemented after the initial swap implementation was finished, leading to a mix match of async and sync code.

