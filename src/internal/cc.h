#ifndef CC_H
#define CC_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>

// Adjust syscall numbers as needed
#define SYS_dgrant    292
#define SYS_read_cap  294
#define SYS_write_cap 295
#define CC_CAP_THRESHOLD_SIZE 1024*1024

typedef enum cc_perm_flags {
    READ = 1,
    WRITE = 2,
    EXEC = 4,
    TRANS = 8,
} cc_perm_flags;

typedef struct cc_dcap {
    uint64_t perms_base;
    uint32_t offset;
    uint32_t size;
    uint64_t PT;
    uint64_t MAC;
} cc_dcap;

typedef struct cc_dcl {
    uint64_t PC;
    uint64_t SP;
    uint64_t TTBR0_EL1;
    uint64_t TTBR1_EL1;
    uint64_t TPIDR_EL1; // task_struct pointer
    uint64_t PSTATE;
    uint64_t TPIDR_EL0;
    uint64_t TPIDRRO_EL0;
    uint64_t MAC;
} cc_dcl;

// --- External Function Prototypes ---

void cc_resume_process(pid_t pid);
void cc_suspend_process(pid_t pid);
void cc_dummy();

void cc_store_cap_from_CR0(cc_dcap* cap);
cc_dcap cc_create_signed_cap_on_CR0(const void* base, size_t offset, size_t size, bool write_flag);

void cc_load_ver_cap_to_CR0(cc_dcap* cap);
uint8_t* cc_memcpy_i8(void* dst, cc_dcap src, size_t count);
void cc_print_cap(cc_dcap cap);


#endif // CC_H
