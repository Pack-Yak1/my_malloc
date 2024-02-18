#include <stdbool.h>
#include <stdio.h>
#include <sys/mman.h>

#define PAGESIZE 4096
// When mmap-ing a new region for a certain size, ensure the mapped region can
// fit at least this many times the size requested to reduce mmap calls
#define REDUNDANCY_MULTIPLIER 32

struct mmap_region;

typedef struct malloc_chunk malloc_chunk_t;
typedef struct mmap_region mmap_region_t;

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

// Head of global regions linked list
static mmap_region_t *regions_start = NULL;
// Tail of global regions linked list
static mmap_region_t *regions_end = NULL;

static malloc_chunk_t *free_head = NULL;
static malloc_chunk_t *free_tail = NULL;

// Handles region local head/tail updates as well
void delete_free_list_chunk(malloc_chunk_t *chunk) {
  // Disconnect previous if any
  malloc_chunk_t *prev = chunk->prev_free;
  if (prev == NULL) {
    // Removing head of free list. Change head pointer
    free_head = chunk->next_free;
  } else {
    prev->next_free = chunk->next_free;
  }

  // Disconnect from next if any
  malloc_chunk_t *next = chunk->next_free;
  if (next == NULL) {
    // Removing tail of free list. Change tail pointer
    free_tail = chunk->prev_free;
  } else {
    next->prev_free = chunk->prev_free;
  }

  // Update region local free head/tail if needed
  mmap_region_t *this_region = chunk->region;
  if (this_region->local_free_head == chunk &&
      this_region->local_free_tail == chunk) {
    // This region only has 1 chunk in the free list. Local list is now empty
    this_region->local_free_head = NULL;
    this_region->local_free_tail = NULL;
  } else if (this_region->local_free_head == chunk) {
    // There's at least 2 elements in the local free list, we can just move head
    this_region->local_free_head = chunk->next_free;
  } else if (this_region->local_free_tail == chunk) {
    // There's at least 2 elements in the local free list, we can just move tail
    this_region->local_free_tail = chunk->prev_free;
  }

  // Mark this chunk's next and prev free as NULL
  chunk->prev_free = NULL;
  chunk->next_free = NULL;
}

// Remove `region` from the region linked list and munmap it
void delete_region(mmap_region_t *region) {
  // Disconnect previous if any
  mmap_region_t *prev = region->prev_region;
  if (prev == NULL) {
    // Removing head of free list. Change head pointer
    regions_start = region->next_region;
  } else {
    prev->next_region = region->next_region;
  }

  // Disconnect from next if any
  mmap_region_t *next = region->next_region;
  if (next == NULL) {
    // Removing tail of free list. Change tail pointer
    regions_end = region->prev_region;
  } else {
    next->prev_region = region->prev_region;
  }

  // Mark this chunk's next and prev free as NULL
  region->prev_region = NULL;
  region->next_region = NULL;

  // Return to OS
  munmap(region, region->size);
}

// Returns a pointer to the malloc chunk which owns `ptr`, or NULL if `ptr` is
// NULL.
malloc_chunk_t *get_chunk_from_data_pointer(void *ptr) {
  if (ptr == NULL) return NULL;
  return (malloc_chunk_t *)((char *)ptr - sizeof(malloc_chunk_t));
}

// Returns the address of the user owned data region in a malloc chunk.
static void *get_chunk_data_address(malloc_chunk_t *chunk) {
  return (char *)chunk + sizeof(malloc_chunk_t);
}

// Returns the address just above the end of the data region belonging to
// `chunk`.
static void *get_address_after_malloc_chunk(malloc_chunk_t *chunk) {
  return (char *)chunk + chunk->chunk_size + sizeof(malloc_chunk_t);
}

// Return the number of remaining bytes for new malloc chunks in this region.
// Does not include space in the region's free list.
static size_t mmap_region_space_remaining(mmap_region_t *region) {
  if (region == NULL) return 0;

  size_t region_max_capacity = region->size - sizeof(mmap_region_t);

  if (region->chunks_tail == NULL) {
    // No chunks allocated, region is empty except for mmap_region_t metadata
    return region_max_capacity;
  }

  return region->size -
         ((size_t)region->chunks_tail - (size_t)region) -  // Offset of tail
         sizeof(malloc_chunk_t) -                          // Metadata of tail
         region->chunks_tail->chunk_size;                  // Data of tail
}

// Create a new mmap region and initialize it to have no chunks. Will be page
// alligned and have sufficient space for a malloc chunk with data size of
// `size_requested`
static void *create_mmap_region(size_t size_requested) {
  size_requested *= REDUNDANCY_MULTIPLIER;
  size_t region_size = PAGESIZE;
  while (region_size - sizeof(mmap_region_t) < size_requested) {
    region_size += region_size;
  }

  mmap_region_t *ptr = mmap(NULL, region_size, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  if (ptr == MAP_FAILED) return NULL;

  // Initialize region
  ptr->size = region_size;
  ptr->chunks_head = NULL;
  ptr->chunks_tail = NULL;
  ptr->next_region = NULL;
  ptr->prev_region = regions_end;
  ptr->local_free_head = NULL;
  ptr->local_free_tail = NULL;
  ptr->occupied_chunks = 0;

  // Maintain mapped region linked list
  if (regions_start == NULL) {
    regions_start = ptr;
  } else {
    regions_end->next_region = ptr;
  }

  regions_end = ptr;

  return ptr;
}

// Traverse the free list and return any existing unoccupied chunk that is
// sufficiently large to hold `size_requested` bytes. Returns NULL if no such
// chunk was found. The free chunk returned, if any, is removed from the free
// list, and its region's local free head and tail are updated if necessary
static void *get_chunk_from_free_list(size_t size_requested) {
  // malloc_chunk_t *prev = free_head;
  malloc_chunk_t *ptr = free_head;

  // Find an unoccupied chunk that is sufficiently large
  while (ptr != NULL) {
    if (ptr->chunk_size >= size_requested) {
      delete_free_list_chunk(ptr);
      return ptr;
    }

    ptr = ptr->next_free;
  }

  return NULL;
}

// Create and initialize a new malloc chunk with space for `size_requested`
// bytes. Handles creation of new mmap regions in the scenario where there's
// insufficient space.
static malloc_chunk_t *create_malloc_chunk(size_t size_requested) {
  // Go to last malloc region and see if there's enough space for user's request
  // plus chunk metadata. If not, get new region
  if (mmap_region_space_remaining(regions_end) <
      size_requested + sizeof(malloc_chunk_t)) {
    // We need space for the new data and its metadata
    void *new_region =
        create_mmap_region(sizeof(malloc_chunk_t) + size_requested);
    if (new_region == NULL) return NULL;  // mmap failure
  }

  // Now we know that regions_end has enough space for this chunk.
  malloc_chunk_t *new_chunk;
  if (regions_end->chunks_head == NULL) {
    // Initialize the head of the chunk list at the end of the region metadata
    new_chunk = (malloc_chunk_t *)((char *)regions_end + sizeof(mmap_region_t));

    regions_end->chunks_head = new_chunk;
  } else {
    new_chunk = get_address_after_malloc_chunk(regions_end->chunks_tail);
    regions_end->chunks_tail = new_chunk;
  }

  // Initialize the new chunk
  new_chunk->chunk_size = size_requested;
  new_chunk->next = NULL;
  new_chunk->prev_free = free_tail;
  new_chunk->next_free = NULL;
  new_chunk->region = regions_end;

  // Make new chunk the chunks tail and increment its occupied chunk count
  regions_end->chunks_tail = new_chunk;
  // Maintain the region's occupancy
  regions_end->occupied_chunks++;
  return new_chunk;
}

void *malloc(size_t sz) {
  if (sz == 0) return NULL;

  // If free list is empty, we must create a new chunk
  if (free_head == NULL) {
    return get_chunk_data_address(create_malloc_chunk(sz));
  }

  // If free list exists, try searching for a sufficiently large chunk first
  malloc_chunk_t *free_list_chunk = get_chunk_from_free_list(sz);
  if (free_list_chunk != NULL) {
    free_list_chunk->region->occupied_chunks++;
    return get_chunk_data_address(free_list_chunk);
  }

  // No suitable chunks. Create new one.
  return get_chunk_data_address(create_malloc_chunk(sz));
}

void free(void *ptr) {
  malloc_chunk_t *chunk_to_free = get_chunk_from_data_pointer(ptr);
  mmap_region_t *region = chunk_to_free->region;

  // If region has no more occupied chunks, we can return it to OS
  region->occupied_chunks--;
  if (region->occupied_chunks == 0) {
    // Remove all of this region's free chunks from the global free list
    if (region->local_free_head != NULL) {
      malloc_chunk_t *prev = region->local_free_head->prev_free;
      // tail != NULL because head is not NULL
      if (prev == NULL) {
        free_head = region->local_free_tail->next_free;
      } else {
        prev->next_free = region->local_free_tail->next_free;
      }

      malloc_chunk_t *next = region->local_free_tail->next_free;
      // head != NULL because tail is not NULL
      if (next == NULL) {
        free_tail = region->local_free_head->prev_free;
      } else {
        next->prev_free = region->local_free_head->prev_free;
      }

      // No need to modify pointers within list as the associated memory will be
      // returned to OS
    }

    delete_region(region);
  } else {
    // Append to free list. First check if the chunk's region has free chunks
    if (region->local_free_head == NULL) {
      // Insert to end of the global free list
      if (free_head == NULL) {
        free_head = chunk_to_free;
      } else {
        free_tail->next_free = chunk_to_free;
      }
      chunk_to_free->prev_free = free_tail;
      free_tail = chunk_to_free;

      region->local_free_head = chunk_to_free;
      region->local_free_tail = chunk_to_free;
    } else {
      // Insert into global free list after `local_free_tail`
      malloc_chunk_t *prev = region->local_free_tail;
      malloc_chunk_t *next = prev->next_free;

      prev->next_free = chunk_to_free;
      chunk_to_free->prev_free = prev;

      chunk_to_free->next_free = next;
      if (next == NULL) {
        // The newly free'd chunk will be the global tail
        free_tail = chunk_to_free;
      } else {
        next->prev_free = chunk_to_free;
      }

      region->local_free_tail = chunk_to_free;
    }
  }
}

// test fns
void print_regions() {
  printf("Listing out mmap regions and remaining space:\n");
  mmap_region_t *region = regions_start;
  while (region != NULL) {
    printf("\t%p: %lu\n", (void *)region, mmap_region_space_remaining(region));
    region = region->next_region;
  }
}

// int main() {
//   printf("start test\n");
//   void *ptr = malloc(8000);
//   void *ptr2 = malloc(8000);
//   void *ptr3 = malloc(4000);
//   printf("Got %p, %p, %p\n", (void *)ptr, (void *)ptr2, (void *)ptr3);
//   print_regions();

//   free(ptr2);
//   print_regions();
//   free(ptr3);
//   print_regions();
//   free(ptr);
//   print_regions();

//   ptr = malloc(8000);
//   print_regions();
// }