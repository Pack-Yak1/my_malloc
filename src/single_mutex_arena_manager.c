#include <pthread.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>

#include "arena_manager.h"

// Store tightly packed arenas in space created with sbrk. Binary search for
// arenas on demand. O(n) insertion. Don't bother deleting entries on deletion.

#define MIN_ARENAS 32
#define UNLIKELY(x) __builtin_expect(x, 0)

static size_t num_arenas = 0;
static size_t arenas_capacity = 0;
static arena_t *arenas_head = NULL;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static void init_arena_array() {
  arenas_head = sbrk(sizeof(arena_t) * MIN_ARENAS);
  arenas_capacity = MIN_ARENAS;
}

// `addr` is where the user would like a new arena to be created.
static void create_arena(arena_t *addr, pid_t thread_id) {
  // First, ensure that addr is safe to write to.
  if (UNLIKELY(num_arenas >= arenas_capacity)) {
    // Double capacity
    sbrk(sizeof(arena_t) * num_arenas);
    arenas_capacity *= 2;
  }

  // Copy arenas one by one, one slot to the right.
  arena_t *dest = arenas_head + num_arenas;
  while (dest > addr) {
    arena_t *src = dest - 1;
    memcpy(dest, src, sizeof(arena_t));
    dest--;
  }

  num_arenas++;
  // Initialize the arena at `addr`
  addr->free_head = NULL;
  addr->free_tail = NULL;
  addr->regions_start = NULL;
  addr->regions_end = NULL;
  addr->thread_id = thread_id;
}

static arena_t *binary_search_helper(arena_t *start, arena_t *end,
                                     pid_t thread_id) {
  if (start == end) {
    // Equivalent to return start->thread_id >= thread_id ? start : start + 1;
    return start + (1 - (start->thread_id >= thread_id));
  }

  arena_t *mid = start + (end - start) / 2;
  if (mid->thread_id < thread_id) {
    // This branch is never reached if mid + 1 > end because we know the answer
    // is in this array.
    return binary_search_helper(mid + 1, end, thread_id);
  } else {
    return binary_search_helper(start, mid, thread_id);
  }
}

// Returns the first arena_t pointer with a thread id greater than or equal to
// `thread_id` between `start` and `end` inclusive. Requires `start` and `end`
// not NULL and `end` > `start. If no arenas have id >= `thread_id`, returns a
// pointer one past `end`.
static arena_t *binary_search(arena_t *start, arena_t *end, pid_t thread_id) {
  if (end->thread_id < thread_id) {
    return end + 1;
  }

  return binary_search_helper(start, end, thread_id);
}

static arena_t *get_arena_pointer(pid_t thread_id) {
  if (UNLIKELY(arenas_head == NULL)) {
    // No arenas exist yet
    init_arena_array();
    create_arena(arenas_head, thread_id);
    return arenas_head;
  }

  arena_t *last_arena = arenas_head + num_arenas - 1;
  arena_t *first_gte = binary_search(arenas_head, last_arena, thread_id);

  if (first_gte > last_arena || first_gte->thread_id != thread_id) {
    // No such arena for this thread exists, and we should insert it here and
    // shift everything back.
    create_arena(first_gte, thread_id);
  }

  return first_gte;
}

arena_t get_arena(pid_t thread_id) {
  pthread_mutex_lock(&lock);

  arena_t ret = *get_arena_pointer(thread_id);
  pthread_mutex_unlock(&lock);
  return ret;
}

int set_arena(pid_t thread_id, arena_t *new_value) {
  pthread_mutex_lock(&lock);

  arena_t *dest = get_arena_pointer(thread_id);
  if (UNLIKELY(thread_id != new_value->thread_id)) {
    return -1;
  }

  memcpy(dest, new_value, sizeof(arena_t));

  pthread_mutex_unlock(&lock);
  return 0;
}

void delete_arena(pid_t thread_id) {}
