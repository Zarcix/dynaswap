import time
import argparse
import sys
import random

def test_swap_memory(size_mb, block_size_mb, delay, master_seed):
    total_bytes = size_mb * 1024 * 1024
    block_bytes = block_size_mb * 1024 * 1024
    
    print(f"[*] Target allocation: {size_mb} MB in {block_size_mb} MB blocks (delay: {delay}s)...")
    print(f"[*] Using pseudo-random data generation (Master Seed: {master_seed}) to defeat compression.")

    mem_blocks = []
    allocated_mb = 0
    block_index = 0

    try:
        while allocated_mb < size_mb:
            current_block_mb = min(block_size_mb, size_mb - allocated_mb)
            current_block_bytes = current_block_mb * 1024 * 1024

            # Allocate block
            block = bytearray(current_block_bytes)

            # Seed a deterministic RNG for this specific block to ensure uncompressible random data
            block_seed = master_seed + block_index
            rng = random.Random(block_seed)
            
            # Fill block with random bytes efficiently (chunk by chunk to avoid massive memory spikes during generation)
            chunk_size = 1024 * 1024  # 1 MB generation chunks
            for offset in range(0, current_block_bytes, chunk_size):
                this_chunk = min(chunk_size, current_block_bytes - offset)
                block[offset:offset + this_chunk] = rng.randbytes(this_chunk)

            mem_blocks.append((block, block_seed))
            allocated_mb += current_block_mb
            block_index += 1
            
            print(f"[*] Allocated and filled {allocated_mb}/{size_mb} MB...", end="\r")
            sys.stdout.flush()

            if delay > 0:
                time.sleep(delay)
                
    except MemoryError:
        print(f"\n[!] Error: Out of memory while allocating at {allocated_mb} MB.")
        sys.exit(1)

    print(f"\n[+] Successfully allocated and filled {size_mb} MB total with random data.")
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

    print("[*] Verifying memory integrity against expected random sequences...")
    errors = 0
    mismatch_logged = 0
    global_offset = 0

    # Read back and compare block by block using the regenerated expected sequence
    for block, block_seed in mem_blocks:
        block_len = len(block)
        rng = random.Random(block_seed)
        
        chunk_size = 1024 * 1024
        for offset in range(0, block_len, chunk_size):
            this_chunk = min(chunk_size, block_len - offset)
            expected = rng.randbytes(this_chunk)
            actual = block[offset:offset + this_chunk]

            if actual != expected:
                for j in range(this_chunk):
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
    parser = argparse.ArgumentParser(description="Test a custom swap driver using uncompressible random data.")
    parser.add_argument("-s", "--size", type=int, default=512, help="Total memory to allocate and test in MB (default: 512)")
    parser.add_argument("-b", "--block-size", type=int, default=32, help="Size of each allocation block in MB (default: 32)")
    parser.add_argument("-d", "--delay", type=float, default=0.1, help="Delay in seconds between blocks to slow down allocation (default: 0.1)")
    parser.add_argument("--seed", type=int, default=1337, help="Master seed for random data generation (default: 1337)")

    args = parser.parse_args()

    if args.size <= 0 or args.block_size <= 0:
        print("[!] Size and block size must be positive integers.")
        sys.exit(1)

    test_swap_memory(args.size, args.block_size, args.delay, args.seed)