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
static void init_reader() {
    init_named_futex();
}

ssize_t cc_read_via_futex(int fd, void *buf, size_t count)
{
	cc_dcap cap;
	ssize_t cap_bytes=syscall_cp(SYS_read, fd, &cap, sizeof(cc_dcap));
	printf("\nread:"); cc_print_cap(cap);

	if (cap_bytes!=sizeof(cc_dcap))
		return -1;
	if (count<cap.size)
		cc_memcpy_i8(buf, cap, count);
	else
		cc_memcpy_i8(buf, cap, cap.size);
	
	// Signal the writer by setting the futex to 1 and waking any waiting process.
	*named_futex = 1;
	futex_wake(named_futex, 1);
	return (ssize_t)count;
	
}

// ssize_t cc_read_via_signal(int fd, void *buf, size_t count)
// {
// 	pid_t ppid;
// 	ssize_t ppid_bytes=syscall_cp(SYS_read, fd, &ppid, sizeof(pid_t));
// 	if (ppid_bytes!=sizeof(pid_t))
// 		return -1;
// 	cc_dcap cap;
// 	ssize_t cap_bytes=syscall_cp(SYS_read, fd, &cap, sizeof(cc_dcap));
// 	if (cap_bytes!=sizeof(cc_dcap))
// 		return -1;
// 	if (count<cap.size)
// 		cc_memcpy_i8(buf, cap, count);
// 	else
// 		cc_memcpy_i8(buf, cap, cap.size);
// 	kill(ppid, SIGCONT);
// 	return (ssize_t)count;
// }

ssize_t read(int fd, void *buf, size_t count)
{
	struct stat st;
    fstat(fd, &st);
    if (S_ISFIFO(st.st_mode)) {
		//if (count>CC_CAP_THRESHOLD_SIZE){
			return cc_read_via_futex(fd, buf, count);
		//}
	}
	return syscall_cp(SYS_read, fd, buf, count);
}

// ssize_t read(int fd, void *buf, size_t count)
// {
// 	return syscall_cp(SYS_read, fd, buf, count);
// }
