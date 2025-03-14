#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "syscall.h"

#include "cc.h"

ssize_t cc_read_u(int fd, void *buf, size_t count)
{
	pid_t ppid;
	ssize_t ppid_bytes=syscall_cp(SYS_read, fd, &ppid, sizeof(pid_t));
	if (ppid_bytes!=sizeof(pid_t))
		return -1;
	cc_dcap cap;
	ssize_t cap_bytes=syscall_cp(SYS_read, fd, &cap, sizeof(cc_dcap));
	if (cap_bytes!=sizeof(cc_dcap))
		return -1;
	if (count<cap.size)
		cc_memcpy_i8(buf, cap, count);
	else
		cc_memcpy_i8(buf, cap, cap.size);
	kill(ppid, SIGCONT);
	return (ssize_t)count;
}

ssize_t read(int fd, void *buf, size_t count)
{
	struct stat st;
    fstat(fd, &st);
    if (S_ISFIFO(st.st_mode)) {
		if (count>CC_CAP_THRESHOLD_SIZE){
			return cc_read_u(fd, buf, count);
		}
	}
	return syscall_cp(SYS_read, fd, buf, count);
}

// ssize_t read(int fd, void *buf, size_t count)
// {
// 	return syscall_cp(SYS_read, fd, buf, count);
// }
