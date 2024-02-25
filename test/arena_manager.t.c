#include "arena_manager.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>

const size_t NUM_THREADS = 100;
const size_t NUM_ITERS = 1000000;

void *thread_func(void *_unused) {
  pid_t my_pid = syscall(__NR_gettid);

  arena_t initial = get_arena(my_pid);
  if (initial.thread_id != my_pid) {
    fprintf(stderr,
            "Thread with id %d got an arena with pid %d on initial call\n",
            my_pid, initial.thread_id);
    exit(1);
  }

  for (size_t i = 0; i < 100; i++) {
    arena_t actual_arena = get_arena(my_pid);
    pid_t actual_id = actual_arena.thread_id;
    if (my_pid != actual_id) {
      fprintf(stderr,
              "Thread with id %d got an arena with pid %d on access %lu\n",
              my_pid, actual_id, i);
      exit(1);
    }

    usleep(random() % 5 + 1);
  }

#ifdef VERBOSE
  fprintf(stderr,
          "Thread with id %d successfully maintained its arena for %lu uses\n",
          my_pid, NUM_ITERS);
#endif
  return NULL;
}

int main() {
  pthread_t threads[NUM_THREADS];
  for (size_t i = 0; i < NUM_THREADS; i++) {
    pthread_create(&threads[i], NULL, thread_func, NULL);
  }

  for (size_t i = 0; i < NUM_THREADS; i++) {
    pthread_join(threads[i], NULL);
  }
}