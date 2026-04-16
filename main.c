#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "reclaim.h"

// This is just a very simple driver program for tesing purposes.

int main(void) {

  printf("PID: %d\n", getpid());

  // Provide an anonymous VM
  void *mainarena =
      mmap((void *)MAIN_ARENA_BASE, MAIN_ARENA_LEN, PROT_READ | PROT_WRITE,
           MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
  if (mainarena != MAP_FAILED) {
    printf("VMA allocated at %p\n", mainarena);
  } else {
    printf("ERROR: mmap failed: %s\n", strerror(errno));
  }

  char *buf = (char *)malloc(10 * sizeof(char));
  printf("buffer allocated at %p\n", buf);

  return EXIT_SUCCESS;
}