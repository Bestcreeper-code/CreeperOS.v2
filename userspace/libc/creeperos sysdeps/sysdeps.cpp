#include "abi-bits/fcntl.h"
#include "mlibc/sysdep-tags.hpp"
#include "mlibc/tcb.hpp"
#include "options/internal/include/bits/ensure.h"
#include <abi-bits/errno.h>
#include <bits/ensure.h>
#include <bits/syscall.h>
#include <mlibc/all-sysdeps.hpp>
#include <string.h>


#define SYS_WRITE 1
#define SYS_BRK 12
#define SYS_ACCESS 21
#define SYS_GETPID 39
#define SYS_GETCWD 79
#define SYS_MKDIR 83
#define SYS_RMDIR 84
#define SYS_EXIT 60
#define SYS_UNLINK 87
#define SYS_ARCH_PRCTL 158
#define SYS_OPENAT 257
#define SYS_MKDIRAT 258
#define SYS_UNLINKAT 263
// #define SYS_MMAP 2

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
	sysdep<Write>(2, msg, strlen(msg), &unused);
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
int Sysdeps<VmMap>::operator()(void *, size_t, int, int, int, off_t, void **) {
	STUB(VmMap);
}
// int Sysdeps<VmProtect>::operator()(void *, size_t, int, int, int, off_t, void **) {
// 	STUB();
// }
int Sysdeps<VmUnmap>::operator()(void *, size_t) {
	STUB(VmUnmap);
}
int Sysdeps<ClockGet>::operator()(int, time_t *, long *) {
	STUB(ClockGet);
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
// int Sysdeps<Unlinkat>::operator()(int dirfd, const char *path, int flags) {
	
// }

} // namespace mlibc
