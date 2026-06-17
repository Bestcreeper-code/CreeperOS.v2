#pragma once

#include <stdint.h>

#define BIT(b) 1<<b

typedef unsigned short		umode_t;
typedef long long  loff_t;
typedef int64_t    ssize_t;
typedef long long  time64_t;
typedef long long  blkcnt_t;
typedef uint32_t   kuid_t;
typedef uint32_t   kgid_t;
typedef uint32_t   dev_t;

typedef uint32_t lba32;
typedef uint64_t lba64;

typedef int atomic_t;

typedef loff_t lsize_t;



#if !defined(S_IFMT)
#define S_IFDIR        0x4000
#define S_IFCHR        0x2000
#define S_IFBLK        0x6000
#define S_IFREG        0x8000
#define S_IFIFO        0x1000
#define S_IFLNK        0xA000
#define S_IFSOCK       0xC000
#define S_IFMT         0xF000

#define S_ISDIR(mode)  (((mode) & S_IFMT) == S_IFDIR)
#define S_ISCHR(mode)  (((mode) & S_IFMT) == S_IFCHR)
#define S_ISBLK(mode)  (((mode) & S_IFMT) == S_IFBLK)
#define S_ISREG(mode)  (((mode) & S_IFMT) == S_IFREG)
#define S_ISFIFO(mode) (((mode) & S_IFMT) == S_IFIFO)
#define S_ISLNK(mode)  (((mode) & S_IFMT) == S_IFLNK)
#define S_ISSOCK(mode) (((mode) & S_IFMT) == S_IFSOCK)

#define S_IRWXU 00700
#define S_IRUSR 00400
#define S_IWUSR 00200
#define S_IXUSR 00100

#define S_IRWXG 00070
#define S_IRGRP 00040
#define S_IWGRP 00020
#define S_IXGRP 00010

#define S_IRWXO 00007
#define S_IROTH 00004
#define S_IWOTH 00002
#define S_IXOTH 00001
#endif
