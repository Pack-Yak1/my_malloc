#include <stdbool.h>
#include <unistd.h>

// Must have fixed compile time size, so we store a pointer to the allocated
// heap memory instead of storing it in the chunk itself
typedef struct malloc_chunk {
  size_t chunk_size;
  struct malloc_chunk *next;
  struct malloc_chunk *prev;
  struct malloc_chunk *next_free;
  bool occupied;
} malloc_chunk_t;

static malloc_chunk_t *chunks_head = NULL;
static malloc_chunk_t *chunks_tail = NULL;
static malloc_chunk_t *free_head = NULL;
static malloc_chunk_t *free_tail = NULL;

static void free_list_delete_gte(void *cutoff_address) {
  // Prune head until we are certain at least one element remains
  while (free_head != NULL && (void *)free_head >= cutoff_address) {
    free_head = free_head->next_free;
  }

  if (free_head == NULL) {
    // List is empty, set tail NULL and return
    free_tail = NULL;
    return;
  }

  // List is not empty. Maintain 2 pointers to prune elements. The above
  // conditions ensure that free_head != NULL, and free_head < cutoff_address.
  malloc_chunk_t *prev = free_head;
  malloc_chunk_t *ptr = free_head->next_free;
  while (ptr != NULL) {
    if ((void *)ptr >= cutoff_address) {
      // Delete this node from linked list, do not increment prev
      prev->next_free = ptr->next_free;

      if (ptr == free_tail) {
        free_tail = prev;
      }

      ptr = ptr->next_free;
    } else {
      prev = prev->next_free;
      ptr = ptr->next_free;
    }
  }
}

// Creates a new chunk and places it at the end of the chunks list
static void *create_new_chunk(size_t sz) {
  // Make space for the metadata and actual data
  char *program_break = sbrk(sz + sizeof(malloc_chunk_t));
  if (program_break == (void *)-1) {
    // Failed to alloc
    return NULL;
  }
  void *data_ptr = program_break + sizeof(malloc_chunk_t);
  malloc_chunk_t *metadata_ptr = (malloc_chunk_t *)program_break;

  metadata_ptr->chunk_size = sz;
  metadata_ptr->prev = chunks_tail;
  metadata_ptr->next = NULL;
  metadata_ptr->next_free = NULL;
  metadata_ptr->occupied = true;

  if (chunks_head == NULL) {
    // First chunk
    chunks_head = metadata_ptr;
  } else {
    // Make old tail point to this chunk
    chunks_tail->next = metadata_ptr;
  }

  // Either way, this will be the new tail
  chunks_tail = metadata_ptr;
  return data_ptr;
}

void *malloc(size_t sz) {
  if (sz == 0) return NULL;

  if (free_head == NULL) return create_new_chunk(sz);

  // First, scan allocated blocks for any gaps we can slot into
  malloc_chunk_t *prev = free_head;
  malloc_chunk_t *ptr = free_head->next_free;

  // Check if head can be reused
  if (free_head->chunk_size >= sz) {
    free_head->occupied = true;

    // Update the free list
    free_head = free_head->next_free;
    // Previous free list head resides in `prev`. Set its next_free to NULL
    prev->next_free = NULL;

    // If head is NULL, free list is empty. Set tail NULL.
    if (free_head == NULL) {
      free_tail = NULL;
    }

    // prev still points to the old free_head
    return (char *)prev + sizeof(malloc_chunk_t);
  }

  // Try reusing any other element of the free list
  while (ptr != NULL) {
    if (ptr->chunk_size >= sz) {
      // We will return `ptr`'s data_ptr. Mark this chunk as occupied.
      ptr->occupied = true;

      // Remove ptr from the list by making its prev point past it.
      prev->next_free = ptr->next_free;
      // If prev points to NULL, it's the new tail.
      if (prev->next_free == NULL) {
        free_tail = prev;
      }

      // Delete the newly occupied chunk's next_free
      ptr->next_free = NULL;

      return (char *)ptr + sizeof(malloc_chunk_t);
    }

    ptr = ptr->next_free;
    prev = prev->next_free;
  }

  // No eligible gaps. Create new chunk at put at end of list.
  return create_new_chunk(sz);
}

void free(void *addr) {
  malloc_chunk_t *chunk_to_free =
      (malloc_chunk_t *)((char *)addr - sizeof(malloc_chunk_t));
  // Mark chunk unoccupied
  chunk_to_free->occupied = false;
  // Append to free list. Set head if list empty
  if (free_head == NULL) {
    free_head = chunk_to_free;
  } else {
    free_tail->next_free = chunk_to_free;
  }
  free_tail = chunk_to_free;

  // We're freeing the last chunk. Scan backwards for all contiguous free'd
  // chunks
  if (chunk_to_free == chunks_tail) {
    while (chunk_to_free->prev != NULL && !chunk_to_free->prev->occupied) {
      chunk_to_free = chunk_to_free->prev;
    }

    // chunk_to_free == chunks_head and it's not occupied.
    if (chunk_to_free->prev == NULL) {
      malloc_chunk_t *new_program_break = chunks_head;
      chunks_head = NULL;
      chunks_tail = NULL;

      free_head = NULL;
      free_tail = NULL;
      brk(new_program_break);
      return;
    }

    // We know that at least one chunk remains and that chunk_to_free is the
    // earliest address we can free
    malloc_chunk_t *new_program_break = chunk_to_free;
    chunks_tail = chunk_to_free->prev;
    chunks_tail->next = NULL;
    free_list_delete_gte(new_program_break);
    brk(new_program_break);
  }
}
