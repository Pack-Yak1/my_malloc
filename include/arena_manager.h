// API for an arena manager. Must be thread safe. May use brk/sbrk

#ifndef ARENA_MANAGER_H
#define ARENA_MANAGER_H

#include <sys/types.h>

#include "arena_types.h"

// Returns a copy of the arena for the thread with id `thread_id`, creating an
// arena if no such arena exists
arena_t get_arena(pid_t thread_id);

// Update the arena for the thread with id `thread_id` to the contents of
// `new_value`. Returns 0 on success
int set_arena(pid_t thread_id, arena_t *new_value);

// Deletes the arena owned by the thread with id `thread_id`, if any. Does
// nothing if no such arena exists
void delete_arena(pid_t thread_id);

#endif