#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

const size_t MAX_ALLOC_SIZE = 4096 * 16;
const size_t MAX_ALLOCS = 1000000;
const size_t NUM_ITERS = 1000000;

void *verbose_malloc(size_t sz) {
  printf("malloc-ing %lu\n", sz);
  void *old_program_break = sbrk(0);

  void *ret = malloc(sz);

  void *new_program_break = sbrk(0);
  bool went_up = new_program_break > old_program_break;
  size_t diff = went_up ? (size_t)new_program_break - (size_t)old_program_break
                        : (size_t)old_program_break - (size_t)new_program_break;
  printf("Program break went %s by %lu\n", went_up ? "up" : "down", diff);
  printf("malloc-ed %p for %lu bytes\n", ret, sz);

  return ret;
}

void verbose_free(void *ptr) {
  printf("free-ing %p\n", ptr);
  void *old_program_break = sbrk(0);

  free(ptr);

  void *new_program_break = sbrk(0);
  bool went_up = new_program_break > old_program_break;
  size_t diff = went_up ? (size_t)new_program_break - (size_t)old_program_break
                        : (size_t)old_program_break - (size_t)new_program_break;
  printf("Program break went %s by %lu\n", went_up ? "up" : "down", diff);
}

void basic_test() {
  printf("Making call to printf first as it modifies program break\n");
  void *start = sbrk(0);
  printf("starting break is %p\n", start);
  void *p = verbose_malloc(100);
  verbose_free(p);
  printf("ending break is %p. Net diff: %lu\n", sbrk(0),
         (size_t)sbrk(0) - (size_t)start);
}

void random_test() {
#ifdef VERBOSE
  printf("Making call to printf first as it modifies program break\n");
  void *start = sbrk(0);
#endif
  void *ptrs[MAX_ALLOCS];
  size_t next_empty_alloc_slot = 0;

  for (size_t i = 0; i < MAX_ALLOCS; i++) {
    ptrs[i] = NULL;
  }

  for (size_t i = 0; i < NUM_ITERS; i++) {
    bool test_alloc = next_empty_alloc_slot < MAX_ALLOCS &&
                      ((next_empty_alloc_slot == 0) || (random() % 2 == 0));

    if (test_alloc) {
#ifdef VERBOSE
      void *ptr = verbose_malloc(random() % MAX_ALLOC_SIZE + 1);
#else
      void *ptr = malloc(random() % MAX_ALLOC_SIZE + 1);
#endif
      ptrs[next_empty_alloc_slot++] = ptr;
    } else {
      size_t idx_to_free = random() % next_empty_alloc_slot;
#ifdef VERBOSE
      verbose_free(ptrs[idx_to_free]);
#else
      free(ptrs[idx_to_free]);
#endif
      // Shift everything else back
      for (size_t j = idx_to_free; j < next_empty_alloc_slot; j++) {
        // If there's no value to the right to copy, just put 0
        void *new_value = j == MAX_ALLOCS ? NULL : ptrs[j + 1];
        ptrs[j] = new_value;
      }

      next_empty_alloc_slot--;
    }

#ifdef VERBOSE
    printf("[");
    for (size_t i = 0; i < MAX_ALLOCS; i++) {
      printf("%p, ", ptrs[i]);
    }
    printf("]\n");
    printf("Net difference so far: %lu\n", (size_t)sbrk(0) - (size_t)start);

    printf("\n");
#endif
  }

#ifdef VERBOSE
  printf("Cleaning up\n");
  for (size_t i = 0; i < next_empty_alloc_slot; i++) {
    verbose_free(ptrs[i]);
  }
  void *end = sbrk(0);

  printf("Net program break difference: %lu\n", (size_t)end - (size_t)start);
#endif
}

int main() {
  // basic_test();
  random_test();
}
