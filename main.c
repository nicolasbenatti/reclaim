#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include "reclaim.h"

// This is just a very simple driver program for tesing purposes.

int main(void) {

  printf("PID: %d\n", getpid());

  char *buf = (char *)malloc(10 * sizeof(char));
  printf("buffer allocated at %p\n", buf);

  recl_alloc_main_heap();

  char *rebuf = (char *)recl_malloc(10 * sizeof(char));
  printf("buffer allocated (with libreclaim) at %p\n", rebuf);

  return EXIT_SUCCESS;
}