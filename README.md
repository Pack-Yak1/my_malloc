# Implementing malloc.h myself for fun

## Features to be done

1. thread safety
2. memory alignment
3. more rigorous testing (random nature of tests, would be good to repeat to reduce variance. also would be good to automate testing and updating this document)
4. calloc
5. realloc
6. reallocarray
7. rest of malloc.h (uncertain)

## Current Implementations

1. `brk_malloc`: Memory is allocated in chunks, with a header that contains metadata followed by a data chunk which the caller can use. The data segment is increased as necessary with `sbrk`, and the newly created chunks are added to a doubly linked list.
   There exists a singly linked free-list which contains free-d but still valid chunks which cannot be returned to the OS as they reside at an address below some occupied chunk. Malloc finds the first sufficiently large unoccupied chunk and returns the data address of that chunk if it exists.
   Otherwise, it creates a new chunk and appends it to the chunk list. This allows us to compact memory into holes left by free and reduce fragmentation.
   Free calculates the header address of a given pointer and marks the chunk vacant. The chunk is added to a free-list. If the free'd chunk is the last chunk (greatest address in memory), traverse the chunk list backwards to find the lowest new address we can set the program break to.
   Truncate the chunk list, and traverse the free-list, filtering any chunks which reside beyond the new program break and reduce the data segment with `brk`.
2. `mmap_malloc`: We keep the idea of memory chunks from `brk_malloc`. But now, whenever we need memory, we call `mmap` to give us some number of pages to write to. Each page can be thought of as a self contained version of `brk_malloc` which has a chunk list, in addition to some metadata for this mmap-ed region, which we store in an `mmap_region_t` struct. There exists a global linked list of regions. Each region maintains its size and a counter of the number of occupied (malloc-ed but not free-d) chunks within them. When a region has no occupied chunks, it can be returned to the OS with `munmap`.
   We keep a global free list similar to `brk_malloc`. However, we want to be able to quickly drop all chunks belonging to a region when we `munmap` it. Thus, we store all free-d chunks from the same region contiguously in the free list, and make each region maintain a pointer to its first and last free chunk in the free list. This means dropping all chunks from a region can be done in O(1) time by manipulating the region's local free head's and tail's pointers. Adding to the free list while maintaining this contiguity invariant is also O(1) as we just insert to the tail of the local free list, and before the next region's free head if any.

## Testing/benchmarking

Tested using `/bin/time -v` on binaries compiled with `-O3`

1. Random malloc/free calls: Make `NUM_ITERS` calls to either `malloc` with random sizes or `free` on any previously allocated pointers, with at most `MAX_ALLOCS` pointers allocated at any one time.

- For `NUM_ITERS` = 10 million and `MAX_ALLOCS` = 1 million and max alloc size = 64 KB:

| Implementation | real time (s) | user time (s) | sys time (s) | Maxi resident set size (KB) |
| -------------- | ------------- | ------------- | ------------ | --------------------------- |
| malloc.h       | 3.75          | 3.71          | 0.04         | 183,976                     |
| brk_malloc     | 2.85          | 2.85          | 0.00         | 30,596                      |
| mmap_malloc    | 2.88          | 2.87          | 0.01         | 36,236                      |
