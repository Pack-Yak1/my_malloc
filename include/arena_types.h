#ifndef ARENA_TYPES_H
#define ARENA_TYPES_H

#include <stddef.h>
#include <sys/types.h>

typedef struct malloc_chunk malloc_chunk_t;
typedef struct mmap_region mmap_region_t;
typedef struct arena arena_t;
typedef struct arena_meta arena_meta_t;

struct malloc_chunk {
  // The size of memory the user can use from this chunk. Resides right after
  // this struct in memory
  size_t chunk_size;
  // Previous chunk in the chunk list this chunk belongs to
  // Next chunk in the chunk list this chunk belongs to
  malloc_chunk_t *next;

  // Previous free chunk in the free list. NULL if no prev free chunk or if this
  // chunk is not free
  malloc_chunk_t *prev_free;
  // Next free chunk in the free list. NULL if no next free chunk or if this
  // chunk is not free
  malloc_chunk_t *next_free;

  // The region this chunk resides in
  mmap_region_t *region;
};

struct mmap_region {
  // Size of mapped region, including this header
  size_t size;

  // Head of chunks linked list for this region
  malloc_chunk_t *chunks_head;
  // Tail of chunks linked list for this region
  malloc_chunk_t *chunks_tail;

  // Free chunks in the global free list belonging to the same region are
  // contiguous in the free list. This is the start of this region's section in
  // the free list
  malloc_chunk_t *local_free_head;
  // The end of this region's section in the free list
  malloc_chunk_t *local_free_tail;

  // Pointer to prev region if any
  mmap_region_t *prev_region;

  // Pointer to next region if any
  mmap_region_t *next_region;

  // Number of occupied malloc chunks in this region
  size_t occupied_chunks;
};

// Every thread has its own arena. Thus no need for locks once arena is found.
struct arena {
  pid_t thread_id;
  mmap_region_t *regions_start;
  mmap_region_t *regions_end;
  malloc_chunk_t *free_head;
  malloc_chunk_t *free_tail;
};

#endif