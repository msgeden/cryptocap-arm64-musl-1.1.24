#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "syscall.h"


#include "cc.h"
#include "cc_futex.h"

// Use a constructor to initialize the shared futex when this object is loaded.
__attribute__((constructor))
static void init_writer() {
    init_named_futex();
}



ssize_t cc_write_via_futex(int fd, const void *buf, size_t count)
{
    cc_dcap cap = cc_create_signed_cap_on_CR0(buf, 0, count, false);
	printf("\nwrite:"); cc_print_cap(cap);
    ssize_t cap_bytes = syscall_cp(SYS_write, fd, &cap, sizeof(cc_dcap));
    if (cap_bytes != sizeof(cc_dcap))
        return -1;

    // Wait for the reader to signal completion.
    // The writer waits until the shared futex is set to 1.
    while (__sync_val_compare_and_swap((volatile int*)named_futex, 0, 0) == 0) {
         futex_wait(named_futex, 0);
    }

    // Reset futex for next use.
    *named_futex = 0;
    return (ssize_t)count;

}


// ssize_t cc_write_via_signal(int fd, const void *buf, size_t count)
// {
// 	pid_t pid=getpid();
// 	ssize_t pid_bytes=syscall_cp(SYS_write, fd, &pid, sizeof(pid_t));
// 	if (pid_bytes!=sizeof(pid_t))
// 		return -1;
// 	cc_dcap cap=cc_create_signed_cap_on_CR0(buf, 0, count, false);
// 	cc_print_cap(cap);


// 	ssize_t cap_bytes=syscall_cp(SYS_write, fd, &cap, sizeof(cc_dcap));
// 	if (cap_bytes!=sizeof(cc_dcap))
// 		return -1;
// 	kill(pid, SIGSTOP);
// 	return (ssize_t)count;	
// }

ssize_t write(int fd, const void *buf, size_t count)
{
	struct stat st;
    fstat(fd, &st);
    if (S_ISFIFO(st.st_mode)) {
		//if (count>CC_CAP_THRESHOLD_SIZE){
			return cc_write_via_futex(fd, buf, count);
		//}
	}
	return syscall_cp(SYS_write, fd, buf, count);
}

// ssize_t write(int fd, const void *buf, size_t count)
// {
// 	return syscall_cp(SYS_write, fd, buf, count);
// }
