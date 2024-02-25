// API for an arena manager. Must be thread safe. May use brk/sbrk

#ifndef ARENA_MANAGER_H
#define ARENA_MANAGER_H

#include <sys/types.h>

#include "arena_types.h"

// Returns the arena for the thread with id `thread_id`, creating an arena if no
// such arena exists
arena_t get_arena(pid_t thread_id);

// Deletes the arena owned by the thread with id `thread_id`, if any. Does
// nothing if no such arena exists
void delete_arena(pid_t thread_id);

#endif