#include <pthread.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>

#include "arena_manager.h"

// Store tightly packed arenas in space created with sbrk. Binary search for
// arenas on demand. O(n) insertion. Don't bother deleting entries on deletion.

#define MIN_ARENAS 32
#define UNLIKELY(x) __builtin_expect(x, 0)
#define LIKELY(x) __builtin_expect(x, 1)

static size_t num_arenas = 0;
static size_t arenas_capacity = 0;
static arena_t *arenas_head = NULL;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

// `addr` is where the user would like a new arena to be created. Can be NULL if
// `arenas_head` is NULL.
void create_arena(arena_t *addr, pid_t thread_id) {
  // First, ensure that addr (or arenas_head if NULL) is safe to write to.
  if (UNLIKELY(arenas_head == NULL)) {
    // First time creating arenas. Initialize arenas_head
    arenas_head = sbrk(sizeof(arena_t) * MIN_ARENAS);
    arenas_capacity = MIN_ARENAS;
    addr = arenas_head;
  } else {
    // The array is initialized and `addr` is where we should insert. Ensure
    // sufficient capacity.
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
  }

  num_arenas++;
  // Initialize the arena at `addr`
  addr->free_head = NULL;
  addr->free_tail = NULL;
  addr->regions_start = NULL;
  addr->regions_end = NULL;
  addr->thread_id = thread_id;
}

// arena_t *create_arena(pid_t thread_id) {
//   if (UNLIKELY(arenas_head == NULL)) {
//     // First time creating arenas. Initialize arenas_head
//     arenas_head = sbrk(sizeof(arena_t) * MIN_ARENAS);
//     arenas_capacity = MIN_ARENAS;
//     return arenas_head;
//   } else {
//     if (UNLIKELY(num_arenas == arenas_capacity)) {
//       // Out of capacity. Double the number of arenas
//       sbrk(sizeof(arena_t) * num_arenas);
//       arenas_capacity *= 2;
//     }

//     return arenas_head + num_arenas++;
//   }
// }

static arena_t *binary_search_helper(arena_t *start, arena_t *end,
                                     pid_t thread_id) {
  if (start == end) {
    // TODO: remove branching with bool to int cast
    return start->thread_id >= thread_id ? start : start + 1;
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

arena_t get_arena(pid_t thread_id) {
  pthread_mutex_lock(&lock);

  if (UNLIKELY(arenas_head == NULL)) {
    // No arenas exist yet
    create_arena(NULL, thread_id);
    arena_t ret = *arenas_head;
    pthread_mutex_unlock(&lock);
    return ret;
  }

  arena_t *last_arena = arenas_head + num_arenas - 1;

  arena_t *first_gte = binary_search(arenas_head, last_arena, thread_id);

  if (UNLIKELY(first_gte > last_arena)) {
    // Every arena in array has id less than `thread_id. Create arena at end
    create_arena(first_gte, thread_id);
    arena_t ret = *first_gte;
    pthread_mutex_unlock(&lock);
    return ret;
  } else {
    if (first_gte->thread_id == thread_id) {
      // Arena for this thread exists. Return it
      arena_t ret = *first_gte;
      pthread_mutex_unlock(&lock);
      return ret;
    } else {
      // No such arena for this thread exists, and we should insert it here and
      // shift everything back.
      create_arena(first_gte, thread_id);
      arena_t ret = *first_gte;
      pthread_mutex_unlock(&lock);
      return ret;
    }
  }
}

void delete_arena(pid_t thread_id) {}
