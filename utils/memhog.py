import time
import argparse
import sys

def test_swap_memory(size_mb, block_size_mb, delay):
    total_bytes = size_mb * 1024 * 1024
    block_bytes = block_size_mb * 1024 * 1024
    
    print(f"[*] Target allocation: {size_mb} MB in {block_size_mb} MB blocks (delay: {delay}s)...")

    mem_blocks = []
    pattern_size = 1024 * 1024  # 1 MB pattern block
    pattern = bytearray(i % 256 for i in range(pattern_size))

    allocated_mb = 0
    try:
        while allocated_mb < size_mb:
            current_block_mb = min(block_size_mb, size_mb - allocated_mb)
            current_block_bytes = current_block_mb * 1024 * 1024

            # Allocate block
            block = bytearray(current_block_bytes)

            # Fill block with pattern
            for offset in range(0, current_block_bytes, pattern_size):
                chunk_size = min(pattern_size, current_block_bytes - offset)
                block[offset:offset + chunk_size] = pattern[:chunk_size]

            mem_blocks.append(block)
            allocated_mb += current_block_mb
            
            print(f"[*] Allocated and filled {allocated_mb}/{size_mb} MB...", end="\r")
            sys.stdout.flush()

            if delay > 0:
                time.sleep(delay)
                
    except MemoryError:
        print(f"\n[!] Error: Out of memory while allocating at {allocated_mb} MB.")
        sys.exit(1)

    print(f"\n[+] Successfully allocated and filled {size_mb} MB total.")
    print("-" * 60)
    print("Instructions for testing your swap driver:")
    print("1. Keep this script running (or background it).")
    print("2. Generate system memory pressure to force the OS to page this memory out.")
    print("3. Bring the memory back into RAM.")
    print("4. Press ENTER here to verify if the data survived the swap cycle intact.")
    print("-" * 60)

    try:
        input("Press ENTER when you are ready to verify memory integrity...")
    except KeyboardInterrupt:
        print("\n[!] Cancelled by user.")
        return

    print("[*] Verifying memory integrity...")
    errors = 0
    mismatch_logged = 0
    global_offset = 0

    # Read back and compare block by block
    for block in mem_blocks:
        block_len = len(block)
        for offset in range(0, block_len, pattern_size):
            chunk_size = min(pattern_size, block_len - offset)
            expected = pattern[:chunk_size]
            actual = block[offset:offset + chunk_size]

            if actual != expected:
                for j in range(chunk_size):
                    if actual[j] != expected[j]:
                        errors += 1
                        if mismatch_logged < 10:  # Print first 10 corruption instances
                            print(f"    [!] Mismatch at byte offset {global_offset + offset + j}: Expected {expected[j]}, got {actual[j]}")
                            mismatch_logged += 1
        global_offset += block_len

    print("-" * 60)
    if errors == 0:
        print("[SUCCESS] Memory verification passed! No data corruption detected.")
    else:
        print(f"[FAIL] Memory verification failed! Found {errors:,} corrupted bytes.")
        print("        Check your swap driver's serialization, paging, or block-storage logic.")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Test a custom swap driver by allocating memory incrementally and verifying integrity.")
    parser.add_argument("-s", "--size", type=int, default=512, help="Total memory to allocate and test in MB (default: 512)")
    parser.add_argument("-b", "--block-size", type=int, default=32, help="Size of each allocation block in MB (default: 32)")
    parser.add_argument("-d", "--delay", type=float, default=0.1, help="Delay in seconds between blocks to slow down allocation (default: 0.1)")

    args = parser.parse_args()

    if args.size <= 0 or args.block_size <= 0:
        print("[!] Size and block size must be positive integers.")
        sys.exit(1)

    test_swap_memory(args.size, args.block_size, args.delay)