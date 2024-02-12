#ifndef NAIVE_MALLOC_H
#define NAIVE_MALLOC_H

#include <unistd.h>

void *malloc(size_t sz);
void free(void *ptr);

#endif