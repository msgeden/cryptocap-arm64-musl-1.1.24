#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "syscall.h"


#include "cc.h"


ssize_t cc_write_u(int fd, const void *buf, size_t count)
{
	pid_t pid=getpid();
	ssize_t pid_bytes=syscall_cp(SYS_write, fd, &pid, sizeof(pid_t));
	if (pid_bytes!=sizeof(pid_t))
		return -1;
	cc_dcap cap=cc_create_signed_cap_on_CR0(buf, 0, count, false);
	ssize_t cap_bytes=syscall_cp(SYS_write, fd, &cap, sizeof(cc_dcap));
	if (cap_bytes!=sizeof(cc_dcap))
		return -1;
	kill(pid, SIGSTOP);
	return (ssize_t)count;	
}

ssize_t write(int fd, const void *buf, size_t count)
{
	struct stat st;
    fstat(fd, &st);
    if (S_ISFIFO(st.st_mode)) {
		if (count>CC_CAP_THRESHOLD_SIZE){
			return cc_write_u(fd, buf, count);
		}
	}
	return syscall_cp(SYS_write, fd, buf, count);
}

// ssize_t write(int fd, const void *buf, size_t count)
// {
// 	return syscall_cp(SYS_write, fd, buf, count);
// }
