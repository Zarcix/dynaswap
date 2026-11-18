import os
import struct

def format_bytes(size):
    """Formats bytes into a human-readable string (KiB, MiB, etc.)"""
    for unit in ['B', 'KiB', 'MiB', 'GiB', 'TiB']:
        if size < 1024:
            return f"{size:.2f} {unit}"
        size /= 1024
    return f"{size:.2f} PiB"

def get_sys_params():
    """Queries the OS for architecture-specific constants"""
    try:
        page_size = os.sysconf('SC_PAGE_SIZE')
    except (ValueError, AttributeError):
        page_size = 4096  # Fallback to standard
        
    ptr_size = struct.calcsize('P')
    return {
        "page_size": page_size,
        "ptr_size": ptr_size,
        "entries_per_page": page_size // ptr_size,
        "page_shift": page_size.bit_length() - 1
    }

def map_overhead_info(block_size_gb):
    sys = get_sys_params()
    
    # 1. Basic Counts
    block_size_bytes = block_size_gb << 30
    page_count = block_size_bytes >> sys['page_shift']

    # 2. Bitmap Size (1 bit per page)
    bitmap_size = page_count / 8

    # 3. 2-Level Map Math
    # How many leaf pages do we need?
    map_leaf_count = -(-page_count // sys['entries_per_page'])
    
    # Size of the "Directory" (the array of pointers to pages)
    map_dir_size = map_leaf_count * sys['ptr_size']
    
    # Size of the "Leaves" (the actual 4KB pages holding the data)
    map_leaf_total_ram = map_leaf_count * sys['page_size']

    # Total for ONE map (virt_to_phys)
    one_map_total = map_leaf_total_ram + map_dir_size
    
    # Total for BOTH maps
    dual_map_total_size = 2 * one_map_total
    total_impl_size = bitmap_size + dual_map_total_size

    print(f"--- DYNAMAP Analysis for {block_size_gb} GiB ---")
    print(f"Detected Arch:    {sys['ptr_size']*8}-bit")
    print(f"Page Size:        {sys['page_size']} bytes")
    print(f"Entries/Page:     {sys['entries_per_page']}")
    print(f"-------------------------------------------")
    print(f"Total Pages:      {page_count:,}")
    print(f"Bitmap Size:      {format_bytes(bitmap_size)}")
    print(f"-------------------------------------------")
    print(f"Map Dir Array:    {format_bytes(map_dir_size)}")
    print(f"Map Leaf RAM:     {format_bytes(map_leaf_total_ram)}")
    print(f"Dual Map Total:   {format_bytes(dual_map_total_size)}")
    print(f"-------------------------------------------")
    print(f"TOTAL RAM COST:   {format_bytes(total_impl_size)}")
    print()

def xarray_overhead_info(block_size_gb):
    sys = get_sys_params()
    
    # Constants for XArray (General kernel estimates)
    SLOTS_PER_NODE = 64
    XA_NODE_SIZE = 576 if sys['ptr_size'] == 8 else 288
    STRUCT_SLOT_SIZE = 32  # Estimated slab size for a custom slot struct
    
    block_size_bytes = block_size_gb << 30
    page_count = block_size_bytes >> sys['page_shift']
    
    # 1. XArray Tree Overhead (Internal Nodes)
    # Using a 1.016 multiplier to account for parent nodes in the radix tree
    num_xa_nodes = (page_count / SLOTS_PER_NODE) * 1.016
    xa_tree_ram = num_xa_nodes * XA_NODE_SIZE
    
    # 2. Allocated Structs (The "Leaves")
    # In an XArray implementation, you usually kmalloc a struct for every entry
    struct_entries_ram = page_count * STRUCT_SLOT_SIZE
    
    total_ram_bytes = xa_tree_ram + struct_entries_ram
    
    print(f"--- XArray + Struct Analysis for {block_size_gb} GiB ---")
    print(f"Total Entries:    {page_count:,}")
    print(f"Tree Overhead:    {format_bytes(xa_tree_ram)}")
    print(f"Struct RAM:       {format_bytes(struct_entries_ram)}")
    print(f"-------------------------------------------")
    print(f"TOTAL RAM COST:   {format_bytes(total_ram_bytes)}")
    print()

if __name__ == "__main__":
    # Test with 1TB capacity
    TEST_GB = 100
    map_overhead_info(TEST_GB)
    xarray_overhead_info(TEST_GB)