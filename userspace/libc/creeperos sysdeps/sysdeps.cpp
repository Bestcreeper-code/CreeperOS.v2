#include "abi-bits/clockid_t.h"
#include "abi-bits/fcntl.h"
#include "abi-bits/pid_t.h"
#include "abi-bits/vm-flags.h"
#include "mlibc/sysdep-tags.hpp"
#include "mlibc/tcb.hpp"
#include "options/internal/include/bits/ensure.h"
#include <abi-bits/errno.h>
#include <bits/ensure.h>
#include <bits/syscall.h>

#include <mlibc/all-sysdeps.hpp>
#include <stdio.h>
#include <string.h>
#include <unistd.h>


#define SYS_READ 0
#define SYS_WRITE 1
#define SYS_OPEN 2
#define SYS_CLOSE 3


#define SYS_LSEEK 8
#define SYS_MMAP 9
#define SYS_MPROTECT 10
#define SYS_MUNMAP 11
#define SYS_BRK 12

#define SYS_ACCESS 21

#define SYS_SCHED_YIELD 24

#define SYS_DUP 32
#define SYS_DUP2 33

#define SYS_GETPID 39

#define SYS_FORK 57

#define SYS_EXECVE 59
#define SYS_EXIT 60

#define SYS_UNAME 60

#define SYS_GETCWD 79
#define SYS_CHDIR 80

#define SYS_MKDIR 83
#define SYS_RMDIR 84
#define SYS_CREATE 85

#define SYS_UNLINK 87

#define SYS_ARCH_PRCTL 158

#define SYS_GETDENTS64 217
#define SYS_CLOCK_GETTIME 228

#define SYS_OPENAT 257
#define SYS_MKDIRAT 258

#define SYS_UNLINKAT 263



// ANCHOR: stub
#define STUB(nam)                                                                                     \
	({                                                                                             \
		syscall(SYS_WRITE, 1, "STUB function for " #nam" was called", sizeof("STUB function for " #nam" was called"));   \
	})
// ANCHOR_END: stub

namespace mlibc {

void Sysdeps<LibcPanic>::operator()() {
	sysdep<LibcLog>("!!! mlibc panic !!!");
	sysdep<Exit>(-1);
	__builtin_trap();
}

void Sysdeps<LibcLog>::operator()(const char *msg) {
	ssize_t unused;
	sysdep<Write>(STDERR_FILENO, msg, strlen(msg), &unused);
}

int Sysdeps<Isatty>::operator()(int fd) {
	(void)fd;
	// this returns ENOTTY when it is not a tty, but we do not have a proper implementation
	// so always return that a file is a tty
	return 0;
}

int Sysdeps<Write>::operator()(int fd, void const *buf, size_t size, ssize_t *ret) {
	*ret = syscall(SYS_WRITE, fd, buf, size);
	// this can never fail in the demo os
	return 0;
}


constexpr long ARCH_PRCTL_SET_FS = 0x1002;
int mlibc::SysdepImpl<TcbSet>::operator()(void* pointer) {
    long ret = syscall(SYS_ARCH_PRCTL, ARCH_PRCTL_SET_FS, (uintptr_t)pointer);

    if (ret < 0) {
        return (int)-ret;
    }
    return 0;
}



int Sysdeps<AnonAllocate>::operator()(size_t size, void **pointer) {
	static uintptr_t current_brk = 0;

	if (current_brk == 0) {
		current_brk = (uintptr_t)syscall(SYS_BRK, 0);
	}

	uintptr_t old_brk = current_brk;
	uintptr_t new_brk = old_brk + size;

	uintptr_t ret = (uintptr_t)syscall(SYS_BRK, new_brk);

	
	if (ret < new_brk) {
		return ENOMEM;
	}

	current_brk = ret;
	*pointer = (void *)old_brk;
	return 0;
}

int Sysdeps<AnonFree>::operator()(void *, unsigned long) {
	return 0;
} 

int Sysdeps<Seek>::operator()(int fd, off_t offset, int whence, off_t *new_offset) {
	long ret = syscall(8, fd, offset, whence);
	if (ret < 0) return -ret;
	*new_offset = ret;
	return 0;
}

void Sysdeps<Exit>::operator()(int status) {
	syscall(SYS_EXIT, status);
	__builtin_unreachable();
}


int Sysdeps<Close>::operator()(int fd) {
	long ret = syscall(3, fd);
	return ret < 0 ? -ret : 0;
}


int Sysdeps<FutexWake>::operator()(int *, bool) {
	STUB(FutexWake);
}
int Sysdeps<FutexWait>::operator()(int *, int, timespec const *) {
	STUB(FutexWait);
}
int Sysdeps<Read>::operator()(int fd, void* buf, unsigned long count, long* size_read) {
	ssize_t res = syscall(0, fd, buf, count);
	if(res >= 0){
		*size_read = res;
		return 0;
	}
	return -res; //since for tehlib it's positive errnos
}
int Sysdeps<Open>::operator()(const char *path, int flags, unsigned int mode, int *fd) {
	long ret = syscall(2, path, flags, mode);
	if (ret < 0) return -ret;
	*fd = (int)ret;
	return 0;
}
int Sysdeps<VmMap>::operator()(void *hint, size_t size, int prot, int flags, int fd, off_t offset, void **window) {
	return -syscall( 
		SYS_MMAP,
		hint,
		size,
		prot,
		flags,
		fd,
		offset
	);
}
int Sysdeps<VmProtect>::operator()(void* pointer, size_t size, int prot) {
    return syscall(
		SYS_MPROTECT,
		pointer,
		size,
		(unsigned long)prot
	);
}

int Sysdeps<VmUnmap>::operator()(void* pointer, size_t size) {
    return syscall(
		SYS_MUNMAP,
		pointer,
		size
	);
}

int Sysdeps<ClockGet>::operator()(int clock, time_t* secs, long* nanos) {
	struct {
		uint64_t tv_sec;
		uint64_t tv_nsec;
	} t;
	int res = syscall(SYS_CLOCK_GETTIME, clock, &t);
	if(res < 0) return -res;

	printf("%lx   %lx \n",t.tv_sec, t.tv_nsec);
	*secs = t.tv_sec;
	printf("%lx   %lx \n",t.tv_sec, t.tv_nsec);
	*nanos = t.tv_nsec;for(;;);
	return res;
}

pid_t Sysdeps<GetPid>::operator()() {
	return syscall(SYS_GETPID);
}

int Sysdeps<GetCwd>::operator()(char *buffer, size_t size) {
	short ret = syscall(SYS_GETCWD, buffer, size);
	
	return -ret;
}

int Sysdeps<Mkdir>::operator()(const char *path, mode_t mode) {
	return syscall(SYS_MKDIR, path, mode);
	
}

int Sysdeps<Rmdir>::operator()(const char* path) {
	return syscall(SYS_RMDIR, path);
}

int Sysdeps<Access>::operator()(const char *path, int mode) {
	return syscall(SYS_ACCESS, path, mode);
}

int Sysdeps<Openat>::operator()(int dirfd, const char *path, int flags, mode_t mode, int *fd) {
	long ret = syscall(SYS_OPENAT, dirfd, path, flags, mode);
	if (ret < 0)
		return (int)ret;

	if (fd)
		*fd = (int)ret;
	return 0;
}

int Sysdeps<Mkdirat>::operator()(int dirfd, const char *path, mode_t mode) {
	return syscall(SYS_MKDIRAT, dirfd, path, mode);
}

int Sysdeps<Unlinkat>::operator()(int dirfd, const char *path, int flags) {
	return syscall(SYS_UNLINKAT, dirfd, path, flags);
}

int Sysdeps<Dup>::operator()(int fd, int flags, int *newfd) {
	int ret = syscall(SYS_DUP, fd);
	
	if(ret<0) return -ret;

	*newfd = ret;
	return 0;
}

int Sysdeps<Dup2>::operator()(int fd, int flags, int newfd) {
	int ret = syscall(SYS_DUP2, fd, newfd);

	if(ret<0) return -ret;

	return ret;
}

int Sysdeps<Fork>::operator()(pid_t* child) {
	short ret = syscall(SYS_FORK);

	if(ret<0) return -ret;
	*child = ret;
	return ret;
}

int Sysdeps<Execve>::operator()(const char *path, char *const argv[], char *const envp[]) {
	syscall(SYS_EXECVE, path, argv, envp);
	return -1;
}
int Sysdeps<Uname>::operator()(struct utsname* buf) {
	return -syscall(SYS_UNAME, buf);
	
}

// int Sysdeps<Unlinkat>::operator()(int dirfd, const char *path, int flags) {
	
// }

} // namespace mlibc
