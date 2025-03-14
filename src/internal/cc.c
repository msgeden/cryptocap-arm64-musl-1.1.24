#include <unistd.h>
#include "syscall.h"

#include "cc.h"


/* -------------------------
   External Functions
   ------------------------- */


/* -------------------------
   Internal (static) Functions
   -------------------------
   The functions below are declared static so that they are
   private to this translation unit.
*/

void cc_resume_process(pid_t pid) {
    kill(pid, SIGCONT);
}

void cc_suspend_process(pid_t pid) {
    kill(pid, SIGSTOP);
}
    
static void cc_wait_for_call_via_loop(void) {
    while (1) {
        sleep(1);  // Wait for calls
    }
}

void cc_print_cap(cc_dcap cap) {
    uint64_t perms = (cap.perms_base & 0xFFFF000000000000) >> 48;
    uint64_t base = cap.perms_base & 0x0000FFFFFFFFFFFF;
    printf("cap.perms=0x%lx, .base=0x%lx, .offset=%d, .size=%d, .PT=0x%lx, .MAC=0x%lx\n",
           perms, base, cap.offset, cap.size, cap.PT, cap.MAC);
}

static void cc_print_dcl(cc_dcl dcl) {
    printf("dcl.PC=0x%lx, .SP=0x%lx, .TTBR0_EL1=0x%lx, .TTBR1_EL1=0x%lx, .TASK_STRUCT/TPIDR_EL1=0x%lx, "
           ".PSTATE=0x%lx, .TPIDR_EL0=0x%lx, .TPIDRRO_EL0=0x%lx, .MAC=0x%lx\n",
           dcl.PC, dcl.SP, dcl.TTBR0_EL1, dcl.TTBR1_EL1, dcl.TPIDR_EL0,
           dcl.PSTATE, dcl.TPIDR_EL0, dcl.TPIDRRO_EL0, dcl.MAC);
}

static uint64_t cc_get_PT() {
    uint64_t ttbr0_el1 = 0xDEADBEEF;
    __asm__ volatile (".word 0x03300000" : "=r"(ttbr0_el1));  // readttbr x0
    return ttbr0_el1;
}

void cc_load_ver_cap_to_CR0(cc_dcap* cap) {
    __asm__ volatile (
         "mov x9, %0\n\t"
         ".word 0x02000009\n\t"      // ldc cr0, [x9]
         :   
         : "r" (cap)
         : "x9"
    );
}

void cc_store_cap_from_CR0(cc_dcap* cap) {
    __asm__ volatile (
         "mov x9, %0\n\t"
         ".word 0x02100009\n\t"      // stc cr0, [x9]
         :   
         : "r" (cap)
         : "x9"
    );
}

static void cc_load_ver_dcl_to_DCLC(cc_dcl* dcl) {
    __asm__ volatile (
        "mov x9, %0\n\t"
        ".word 0x3900089\n\t"      // cldcl dclc, [x9]
        :
        : "r"(dcl)
        : "x9"
    ); 
}

static void cc_store_dcl_from_DCLC(cc_dcl* dcl) {
    __asm__ volatile (
        "mov x9, %0\n\t"
        ".word 0x3a00089\n\t"      // cstcl dclc, [x9]
        :
        : "r"(dcl)
        : "x9"
    ); 
}

static void cc_load_ver_dcl_to_DCLR(cc_dcl* dcl) {
    __asm__ volatile (
        "mov x9, %0\n\t"
        ".word 0x39000a9\n\t"      // cldcl dclr, [x9]
        :
        : "r"(dcl)
        : "x9"
    ); 
}

static void cc_store_dcl_from_DCLR(cc_dcl* dcl) {
    __asm__ volatile (
        "mov x9, %0\n\t"
        ".word 0x3a000a9\n\t"      // cstcl dclr, [x9]
        :
        : "r"(dcl)
        : "x9"
    ); 
}

static cc_dcap cc_create_cap_struct(void* base, size_t size, bool write_flag, uint64_t PT) {
    cc_dcap cap;
    uint64_t perms = (write_flag) ? WRITE : READ;
    perms = (perms << 48);
    cap.perms_base = perms | (uint64_t)base;
    cap.offset = 0;
    cap.size = size;
    cap.PT = PT;
    cap.MAC = 0x0;
    return cap;
}    

cc_dcap cc_create_signed_cap_on_CR0(const void* base, size_t offset, size_t size, bool write_flag) {
    cc_dcap cap;
    uint64_t perms = (write_flag) ? WRITE + READ : READ;
    perms = (perms << 48);
    uint64_t perms_base = perms | (uint64_t)base;
    uint64_t offset_size = (((uint64_t)size) << 32) | (uint64_t)offset;
    __asm__ volatile (
        "mov x9, %0\n\t" // mov perms_base to x9
        "mov x10, %1\n\t" // mov offset+size to x10
        ".word 0x0340012a\n\t"  // ccreate cr0, x9, x10  
        : 
        : "r" (perms_base), "r" (offset_size)
        : "x9", "x10"
    );
    cc_store_cap_from_CR0(&cap);
    return cap;
}    

static cc_dcl cc_grant_signed_DCLC(void* PC, void* SP) {
    cc_dcl dcl;
    syscall_cp(SYS_dgrant, PC, SP);
    cc_store_dcl_from_DCLC(&dcl);
    return dcl;
}    

static cc_dcap cc_setbase_resign_on_CR0(void* new_base, cc_dcap original_cap) {
    cc_load_ver_cap_to_CR0(&original_cap);
    cc_dcap new_cap;
    __asm__ volatile (
        "mov x9, %0\n\t" // mov new_base to x9
        ".word 0x02500009\n\t"  // csetbase cr0, cr0, x9  
        : 
        : "r" (new_base)
        : "x9", "x10"
    );
    cc_store_cap_from_CR0(&new_cap);
    return new_cap;
}    

static cc_dcap cc_setperms_resign_on_CR0(uint32_t perms, cc_dcap original_cap) {
    cc_load_ver_cap_to_CR0(&original_cap);
    cc_dcap new_cap;
    __asm__ volatile (
        "mov x9, %0\n\t" // mov new perms to x9
        ".word 0x02700009\n\t"  // setperms cr0, cr0, x9  
        : 
        : "r" (perms)
        : "x9", "x10"
    );
    cc_store_cap_from_CR0(&new_cap);
    return new_cap;
}    

static cc_dcap cc_setsize_resign_on_CR0(uint32_t size, cc_dcap original_cap) {
    cc_load_ver_cap_to_CR0(&original_cap);
    cc_dcap new_cap;
    __asm__ volatile (
        "mov x9, %0\n\t" // mov new size to x9
        ".word 0x02600009\n\t"  // setsize cr0, cr0, x9  
        : 
        : "r" (size)
        : "x9", "x10"
    );
    cc_store_cap_from_CR0(&new_cap);
    return new_cap;
}    

static void cc_inc_cap_offset(cc_dcap* cap, uint32_t leap) {
    cap->offset += leap;
}

static inline uint8_t cc_read_i8_via_CR0() {
    uint8_t data = 0;
    __asm__ volatile (
        ".word 0x02200d20\n\t"  // cldg8 x9, [cr0]
        "and %0, x9, #0xFF\n\t" // Extract least significant byte
        : "=r" (data)
        :
        : "x9", "memory"
    );
    return data;
}

static inline void cc_write_i8_via_CR0(uint8_t data) {
    __asm__ volatile (
        "and x9, %0, #0xFF\n\t"
        ".word 0x02300d20\n\t"  // cstg8 x9, [cr0]
        :
        : "r" (data)
        : "x9"
    );
}

static uint8_t cc_load_CR0_read_i8_data(cc_dcap cap) {
    uint8_t data = 0;
    cc_load_ver_cap_to_CR0(&cap);
    __asm__ volatile (
        ".word 0x02200d20\n\t"
        "and %0, x9, #0xFF\n\t"
        : "=r" (data)
        :
        : "x9", "memory"
    );
    return data;
}

static void cc_load_CR0_write_i8_data(cc_dcap cap, uint8_t data) {
    cc_load_ver_cap_to_CR0(&cap);
    __asm__ volatile (
        "and x9, %0, #0xFF\n\t"
        ".word 0x02300d20\n\t"
        :
        : "r" (data)
        : "x9"
    );
}

static inline uint16_t cc_read_i16_via_CR0() {
    uint16_t data;
    __asm__ volatile (
        ".word 0x02200920\n\t"  // cldg16 x9, [cr0]
        "and %0, x9, #0xFFFF\n\t"
        : "=r" (data)
        :
        : "x9", "memory"
    );
    return data;
}

static inline void cc_write_i16_data_via_CR0(uint16_t data) {
    __asm__ volatile (
        "and x9, %0, #0xFFFF\n\t"
        ".word 0x02300920\n\t"  // cstg16 x9, [cr0]
        :
        : "r" (data)
        : "x9"
    );
}

static uint16_t cc_load_CR0_read_i16_data(cc_dcap cap) {
    uint16_t data;
    cc_load_ver_cap_to_CR0(&cap);
    __asm__ volatile (
        ".word 0x02200920\n\t"
        "and %0, x9, #0xFFFF\n\t"
        : "=r" (data)
        :
        : "x9", "memory"
    );
    return data;
}

static void cc_load_CR0_write_i16_data(cc_dcap cap, uint16_t data) {
    cc_load_ver_cap_to_CR0(&cap);
    __asm__ volatile (
        "and x9, %0, #0xFFFF\n\t"
        ".word 0x02300920\n\t"
        :
        : "r" (data)
        : "x9"
    );
}

static inline uint32_t cc_read_i32_data_via_CR0() {
    uint32_t data;
    __asm__ volatile (
        ".word 0x02200520\n\t"  // cldg x9, [cr0]
        "and %0, x9, #0xFFFFFFFF\n\t"
        : "=r" (data)
        :
        : "x9", "memory"
    );
    return data;
}

static inline void cc_write_i32_data_via_CR0(uint32_t data) {
    __asm__ volatile (
        "and x9, %0, #0xFFFFFFFF\n\t"
        ".word 0x02300520\n\t"  // cstg x9, [cr0]
        :
        : "r" (data)
        : "x9"
    );
}

static uint32_t cc_load_CR0_read_i32_data(cc_dcap cap) {
    uint32_t data;
    cc_load_ver_cap_to_CR0(&cap);
    __asm__ volatile (
        ".word 0x02200520\n\t"
        "and %0, x9, #0xFFFFFFFF\n\t"
        : "=r" (data)
        : 
        : "x9", "memory"
    );
    return data;
}

static void cc_load_CR0_write_i32_data(cc_dcap cap, uint32_t data) {
    cc_load_ver_cap_to_CR0(&cap);
    __asm__ volatile (
        "and x9, %0, #0xFFFFFFFF\n\t"
        ".word 0x02300520\n\t"
        :
        : "r" (data)
        : "x9"
    );
}

static inline uint64_t cc_read_i64_via_CR0() {
    uint64_t data = 0;
    __asm__ volatile (
        ".word 0x02200120\n\t"  // cldg x9, [cr0]
        "mov %0, x9\n\t"
        : "=r" (data)
        :
        : "x9", "memory"
    );
    return data;
}

static inline void cc_write_i64_via_CR0(uint64_t data) {
    __asm__ volatile (
        "mov x9, %0\n\t"
        ".word 0x02300120\n\t"  // cstg x9, [cr0]
        :
        : "r" (data)
        : "x9"
    );
}

static uint64_t cc_load_CR0_read_i64_data(cc_dcap cap) {
    uint64_t data;
    cc_load_ver_cap_to_CR0(&cap);
    __asm__ volatile (
        ".word 0x02200120\n\t"
        "mov %0, x9\n\t"
        : "=r" (data)
        :
        : "x9", "memory"
    );
    return data;
}

static void cc_load_CR0_write_i64_data(cc_dcap cap, uint64_t data) {
    cc_load_ver_cap_to_CR0(&cap);
    __asm__ volatile (
        "mov x9, %0\n\t"
        ".word 0x02300120\n\t"
        :
        : "r" (data)
        : "x9"
    );
}

uint8_t* cc_memcpy_i8(void* dst, cc_dcap src, size_t count) {
    if (count == 0)
        return (uint8_t*)dst;
    
    cc_load_ver_cap_to_CR0(&src);

    __asm__ volatile(
        "mov x10, %[dst_ptr]\n\t"
        "mov x11, %[cnt]\n\t"
        "mov x12, #0\n\t"
        "1:\n\t"      
        ".word 0x02200d20\n\t"       // cldg8 x9, [cr0]
        ".word 0x03c00001\n\t"       // cincoffset cr0, #1
        "strb w9, [x10]\n\t"
        "add x10, x10, #1\n\t"
        "add x12, x12, #1\n\t"
        "cmp x12, x11\n\t"
        "b.lt 1b\n\t"
        : 
        : [dst_ptr] "r" (dst), [cnt] "r" (count)
        : "x9", "x10", "x11", "x12", "cc", "memory"
    ); 

    return (uint8_t*)dst;
}

void cc_dummy() { 
    __asm__ volatile(
        ".word 0x03500000\n\t"  // dret
    );
}

__attribute__((naked))
static inline void cc_dcall() { 
    __asm__ volatile (
        "str x9,  [sp, #-8]!\n\t"
        "str x10, [sp, #-8]!\n\t"
        "str x11, [sp, #-8]!\n\t"
        "str x12, [sp, #-8]!\n\t"
        "str x13, [sp, #-8]!\n\t"
        "str x14, [sp, #-8]!\n\t"
        "str x15, [sp, #-8]!\n\t"
        "str x16, [sp, #-8]!\n\t"
        "str x17, [sp, #-8]!\n\t"
        "str x18, [sp, #-8]!\n\t"
        "str x19, [sp, #-8]!\n\t"
        "str x20, [sp, #-8]!\n\t"
        "str x21, [sp, #-8]!\n\t"
        "str x22, [sp, #-8]!\n\t"
        "str x23, [sp, #-8]!\n\t"
        "str x24, [sp, #-8]!\n\t"
        "str x25, [sp, #-8]!\n\t"
        "str x26, [sp, #-8]!\n\t"
        "str x27, [sp, #-8]!\n\t"
        "str x28, [sp, #-8]!\n\t"
        "str x29, [sp, #-8]!\n\t"
        "str x30, [sp, #-8]!\n\t"
    );
    __asm__ volatile(
        ".word 0x3d00000\n\t"      // dcall
    );
    __asm__ volatile (
        "ldr x30, [sp], #8\n\t"
        "ldr x29, [sp], #8\n\t"
        "ldr x28, [sp], #8\n\t"
        "ldr x27, [sp], #8\n\t"
        "ldr x26, [sp], #8\n\t"
        "ldr x25, [sp], #8\n\t"
        "ldr x24, [sp], #8\n\t"
        "ldr x23, [sp], #8\n\t"
        "ldr x22, [sp], #8\n\t"
        "ldr x21, [sp], #8\n\t"
        "ldr x20, [sp], #8\n\t"
        "ldr x19, [sp], #8\n\t"
        "ldr x18, [sp], #8\n\t"
        "ldr x17, [sp], #8\n\t"
        "ldr x16, [sp], #8\n\t"
        "ldr x15, [sp], #8\n\t"
        "ldr x14, [sp], #8\n\t"
        "ldr x13, [sp], #8\n\t"
        "ldr x12, [sp], #8\n\t"
        "ldr x11, [sp], #8\n\t"
        "ldr x10, [sp], #8\n\t"
        "ldr x9,  [sp], #8\n\t"
        "ret\n\t"
    );
}

__attribute__((naked))
static inline void cc_dret() { 
    __asm__ volatile(
        ".word 0x3e00000\n\t"  // dret
    );
}
