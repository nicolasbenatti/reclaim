#include <stdio.h>

void *__wrap_malloc(size_t size) {
  printf("malloc wrapper.\n");

  return (void *)0x400000000100;
}
