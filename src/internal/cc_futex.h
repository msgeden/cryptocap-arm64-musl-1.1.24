#ifndef FUTEX_SYNC_H
#define FUTEX_SYNC_H

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/syscall.h>

#if defined(__has_include)
#  if __has_include(<linux/futex.h>)
#    include <linux/futex.h>
#  else
     // Define the necessary futex constants manually
#    define FUTEX_WAIT 0
#    define FUTEX_WAKE 1
#  endif
#else
   // Fallback if __has_include is not supported:
#  define FUTEX_WAIT 0
#  define FUTEX_WAKE 1
#endif

// Name and size for our shared futex variable.
#define FUTEX_SHM_NAME "/cc_futex"
#define FUTEX_SHM_SIZE sizeof(int)

// Pointer to the shared futex variable.
// (Each translation unit including this header gets its own pointer variable,
// but they will all point to the same shared mapping.)
static volatile int *named_futex = NULL;

// Initialize the named shared futex.
// This function should be called (or used as a constructor) in both the reader
// and writer source files.
static void init_named_futex(void) {
    int fd = shm_open(FUTEX_SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
        perror("shm_open");
        exit(1);
    }
    if (ftruncate(fd, FUTEX_SHM_SIZE) == -1) {
        perror("ftruncate");
        close(fd);
        exit(1);
    }
    named_futex = mmap(NULL, FUTEX_SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (named_futex == MAP_FAILED) {
         perror("mmap");
         close(fd);
         exit(1);
    }
    close(fd);
    // Initialize futex to 0 (only one process should actually do this if needed,
    // but setting it in both cases should be harmless if they’re coordinated).
    *named_futex = 0;
}

// Simple futex wrappers
static int futex_wait(volatile int *addr, int expected) {
    return syscall(SYS_futex, (int *)addr, FUTEX_WAIT, expected, NULL, NULL, 0);
}

static int futex_wake(volatile int *addr, int count) {
    return syscall(SYS_futex, (int *)addr, FUTEX_WAKE, count, NULL, NULL, 0);
}

#endif /* FUTEX_SYNC_H */
