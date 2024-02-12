# Implementing malloc.h myself for fun

## Features done
1. malloc
2. free

## Features to be done
1. thread safety
2. memory alignment
3. calloc
4. realloc
5. reallocarray
6. rest of malloc.h (uncertain)

## Current Implementation
Memory is allocated in chunks, with a header that contains metadata followed by the memory chunk which the caller can use. The data segment is increased as necessary with `sbrk`, and the newly created chunks are added to a doubly linked list.
There exists a singly linked free-list which contains free-d but unoccupied chunks which cannot be returned to the OS as they reside at an address below some occupied chunk. Malloc finds the first sufficiently large unoccupied chunk and returns the data address of that chunk if it exists.
Otherwise, it creates a new chunk and appends it to the chunk list.

Free calculates the header address of a given pointer and marks the chunk vacant. The chunk is added to a free-list. If the free'd chunk is the last chunk (greatest address in memory), traverse the chunk list backwards to find the lowest new address we can set the program break to.
Truncate the chunk list, and traverse the free-list, filtering any chunks which reside beyond the new program break and reduce the data segment with `brk`.

## Testing/benchmarking
1. Random malloc/free calls: Make `NUM_ITERS` calls to either `malloc` with random sizes or `free` on any previously allocated pointers, with at most `MAX_ALLOCS` pointers allocated at any one time.
  - For `NUM_ITERS` = 10 million and `MAX_ALLOCS` = 1 million and max alloc size = 4096 bytes:
    - My naive malloc: `real    0m19.259s, user    0m19.209s, sys     0m0.050s`
    - std lib malloc: `real    0m19.300s, user    0m19.290s, sys     0m0.010s`