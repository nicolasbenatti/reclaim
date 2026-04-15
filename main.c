#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

// This is just a very simple driver program for tesing purposes.

#define MAIN_ARENA_BASE 0x400000000000
#define MAIN_ARENA_LEN 0x4000 // 16KB

int main(void) {

  printf("PID: %d\n", getpid());

  // Provide an anonymous VM
  void *mainarena = mmap(NULL, MAIN_ARENA_LEN, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
  if (mainarena != MAP_FAILED) {
    printf("VMA allocated at %p\n", mainarena);
  } else {
    printf("ERROR: mmap failed: %s\n", strerror(errno));
  }

  return EXIT_SUCCESS;
}