#pragma once

#include "mlibc/sysdep-tags.hpp"
#include <mlibc/sysdep-signatures.hpp>

namespace mlibc {

struct CreeperOsSysdepTags :
	LibcPanic,
	LibcLog,
	Isatty,
	Write,
	TcbSet,
	AnonAllocate,
	AnonFree,
	Seek,
	Exit,
	Close,
	FutexWake,
	FutexWait,
	Read,
	Open,
	VmMap,
	VmProtect,
	VmUnmap,
	ClockGet,
	GetPid,
	GetCwd,
	Rmdir,
	Mkdir,
	Access,
	Openat,
	Mkdirat,
	Unlinkat,
	Dup,Dup2,
	Fork,
	Execve,
	Uname
{};

template<typename Tag>
using Sysdeps = SysdepOf<CreeperOsSysdepTags, Tag>;

struct SysdepTraits {
	static constexpr bool usesRtNetlink = false;
};

} // namespace mlibc
