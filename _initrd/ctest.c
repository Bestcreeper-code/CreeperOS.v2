/*
 * syscall_test.c
 *
 * Probes which POSIX/libc entry points are actually backed by working
 * syscalls on your target (custom kernel + mlibc), given that the kernel
 * only implements:
 *
 *   open write read close lseek brk getpid fork execve exit
 *   getcwd mkdir create(=open+O_CREAT) rmdir unlink access
 *   dup dup2 chdir getdents64
 *
 * and NOT (as far as we know): stat/fstat, openat family, signals,
 * sockets, mmap/mlock, uid/gid, fcntl, termios, timers, sysvipc, threads,
 * wait/waitpid.
 *
 * Each test is self-contained, never aborts the whole run on failure,
 * and prints PASS / FAIL / SKIP with errno decoding.
 *
 * Build (cross toolchain against your sysroot):
 *   x86_64-yourtarget-gcc -static -o syscall_test syscall_test.c
 *
 * Run:
 *   ./syscall_test           -> runs full suite
 *   ./syscall_test child     -> internal helper mode used by the execve test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/uio.h>

typedef long loff_t ;
#define SYS_BRK  12   

static inline long raw_syscall1(long num, long a1) {
    long ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(a1)
        : "rcx", "r11", "memory"
    );
    return ret;
}

/* Some of these may not exist in every header set; guard them so the file
 * still compiles even if a given prototype is missing on your sysroot. */
#if __has_include(<sys/stat.h>)
#include <sys/stat.h>
#define HAVE_STAT_H 1
#endif
#if __has_include(<sys/socket.h>)
#include <sys/socket.h>
#define HAVE_SOCKET_H 1
#endif
#if __has_include(<sys/mman.h>)
#include <sys/mman.h>
#define HAVE_MMAN_H 1
#endif
#if __has_include(<signal.h>)
#include <signal.h>
#define HAVE_SIGNAL_H 1
#endif
#if __has_include(<sys/wait.h>)
#include <sys/wait.h>
#define HAVE_WAIT_H 1
#endif

/* ---------- tiny test-reporting harness ---------- */

static int g_pass = 0, g_fail = 0, g_skip = 0;

#define REPORT_PASS(name, fmt, ...) \
    do { g_pass++; printf("[PASS] %-28s " fmt "\n", name, ##__VA_ARGS__); } while (0)

#define REPORT_FAIL(name, fmt, ...) \
    do { g_fail++; printf("[FAIL] %-28s " fmt "\n", name, ##__VA_ARGS__); } while (0)

#define REPORT_SKIP(name, fmt, ...) \
    do { g_skip++; printf("[SKIP] %-28s " fmt "\n", name, ##__VA_ARGS__); } while (0)

#define REPORT_UNIMPL(name, fmt, ...) \
    do { g_skip++; printf("[N/I ] %-28s " fmt " (errno=%d %s)\n", name, ##__VA_ARGS__, errno, strerror(errno)); } while (0)

/* ================================================================
 *  Group 1: syscalls directly present in the kernel table
 * ================================================================ */

static void test_open_write_read_close(void) {
    const char *name = "open/write/read/close";
    const char *path = "/tmp/synctest_basic.txt";
    const char *msg = "hello syscall test\n";
    char buf[64] = {0};

    int fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd < 0) { REPORT_FAIL(name, "open() failed: %s", strerror(errno)); return; }

    ssize_t w = write(fd, msg, strlen(msg));
    if (w != (ssize_t)strlen(msg)) { REPORT_FAIL(name, "write() returned %zd", w); close(fd); return; }

    if (lseek(fd, 0, SEEK_SET) != 0) { REPORT_FAIL(name, "lseek(0,SEEK_SET) before read failed"); close(fd); return; }

    ssize_t r = read(fd, buf, sizeof(buf) - 1);
    if (r != w || memcmp(buf, msg, w) != 0) {
        REPORT_FAIL(name, "read back mismatch (r=%zd)", r);
        close(fd);
        return;
    }

    if (close(fd) != 0) { REPORT_FAIL(name, "close() failed: %s", strerror(errno)); return; }

    unlink(path);
    REPORT_PASS(name, "wrote/read back %zd bytes correctly", w);
}

static void test_lseek(void) {
    const char *name = "lseek(SEEK_SET/CUR/END)";
    const char *path = "/tmp/synctest_lseek.txt";
    int fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd < 0) { REPORT_FAIL(name, "open() failed: %s", strerror(errno)); return; }

    write(fd, "0123456789", 10);

    loff_t p1 = lseek(fd, 5, SEEK_SET);
    loff_t p2 = lseek(fd, 2, SEEK_CUR);
    loff_t p3 = lseek(fd, 0, SEEK_END);

    close(fd);
    unlink(path);

    if (p1 == 5 && p2 == 7 && p3 == 10) {
        REPORT_PASS(name, "SET->5 CUR+2->7 END->10");
    } else {
        REPORT_FAIL(name, "got SET=%lld CUR=%lld END=%lld", (long long)p1, (long long)p2, (long long)p3);
    }
}

static void test_getpid(void) {
    pid_t p = getpid();
    if (p > 0) REPORT_PASS("getpid", "pid=%d", (int)p);
    else REPORT_FAIL("getpid", "returned %d", (int)p);
}

static void test_brk_sbrk(void) {
    const char *name = "brk (raw syscall)";

    /* Many brk() ABIs: call with 0 to query current break, or with a
     * target address to set it and get back the new (or old, check your
     * kernel's semantics) break pointer. Adjust interpretation once you
     * see what your sys_brk actually returns. */
    long current = raw_syscall1(SYS_BRK, 0);
    if (current <= 0) {
        REPORT_FAIL(name, "query brk(0) returned %ld", current);
        return;
    }

    long requested = current + 4096;
    long result = raw_syscall1(SYS_BRK, requested);
    if (result < requested) {
        REPORT_FAIL(name, "brk(%ld) returned %ld, heap didn't grow", requested, result);
        return;
    }

    volatile char *p = (volatile char *)current;
    p[0] = 0x42;
    p[4095] = 0x24;

    if (p[0] == 0x42 && p[4095] == 0x24) {
        REPORT_PASS(name, "grew heap via raw brk syscall (0x%lx -> 0x%lx) and wrote to it",
                    (unsigned long)current, (unsigned long)result);
    } else {
        REPORT_FAIL(name, "heap write-back mismatch after growth");
    }
}

static void test_getcwd_chdir_mkdir_rmdir(void) {
    const char *name = "getcwd/chdir/mkdir/rmdir";
    char cwd_before[512], cwd_inside[512], cwd_after[512];

    if (!getcwd(cwd_before, sizeof(cwd_before))) {
        REPORT_FAIL(name, "getcwd() (initial) failed: %s", strerror(errno));
        return;
    }

    const char *dir = "/tmp/synctest_dir";
    if (mkdir(dir, 0755) != 0 && errno != EEXIST) {
        REPORT_FAIL(name, "mkdir() failed: %s", strerror(errno));
        return;
    }

    if (chdir(dir) != 0) {
        REPORT_FAIL(name, "chdir() into new dir failed: %s", strerror(errno));
        rmdir(dir);
        return;
    }

    if (!getcwd(cwd_inside, sizeof(cwd_inside))) {
        REPORT_FAIL(name, "getcwd() (inside) failed: %s", strerror(errno));
        chdir(cwd_before);
        rmdir(dir);
        return;
    }

    if (chdir(cwd_before) != 0) {
        REPORT_FAIL(name, "chdir() back failed: %s", strerror(errno));
        return;
    }

    if (!getcwd(cwd_after, sizeof(cwd_after))) {
        REPORT_FAIL(name, "getcwd() (after) failed: %s", strerror(errno));
        return;
    }

    if (rmdir(dir) != 0) {
        REPORT_FAIL(name, "rmdir() failed: %s", strerror(errno));
        return;
    }

    if (strcmp(cwd_before, cwd_after) == 0 && strstr(cwd_inside, "synctest_dir")) {
        REPORT_PASS(name, "before='%s' inside='%s'", cwd_before, cwd_inside);
    } else {
        REPORT_FAIL(name, "cwd mismatch: before='%s' inside='%s' after='%s'",
                    cwd_before, cwd_inside, cwd_after);
    }
}

static void test_unlink(void) {
    const char *name = "unlink";
    const char *path = "/tmp/synctest_unlink.txt";
    int fd = open(path, O_CREAT | O_RDWR, 0644);
    if (fd < 0) { REPORT_FAIL(name, "open() failed: %s", strerror(errno)); return; }
    close(fd);

    if (unlink(path) != 0) { REPORT_FAIL(name, "unlink() failed: %s", strerror(errno)); return; }

    /* confirm it's gone */
    int fd2 = open(path, O_RDONLY);
    if (fd2 >= 0) {
        REPORT_FAIL(name, "file still openable after unlink");
        close(fd2);
        return;
    }
    REPORT_PASS(name, "file removed, reopen correctly failed with %s", strerror(errno));
}

static void test_access(void) {
    const char *name = "access";
    const char *path = "/tmp/synctest_access.txt";
    int fd = open(path, O_CREAT | O_RDWR, 0644);
    if (fd < 0) { REPORT_FAIL(name, "open() failed: %s", strerror(errno)); return; }
    close(fd);

    int f_ok = access(path, F_OK);
    int rw_ok = access(path, R_OK | W_OK);
    unlink(path);
    int gone_ok = access(path, F_OK);

    if (f_ok == 0 && rw_ok == 0 && gone_ok != 0) {
        REPORT_PASS(name, "F_OK/R_OK/W_OK correct before+after unlink");
    } else {
        REPORT_FAIL(name, "f_ok=%d rw_ok=%d gone_ok=%d", f_ok, rw_ok, gone_ok);
    }
}

static void test_dup_dup2(void) {
    const char *name = "dup/dup2";
    const char *path = "/tmp/synctest_dup.txt";
    int fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd < 0) { REPORT_FAIL(name, "open() failed: %s", strerror(errno)); return; }

    int fd_dup = dup(fd);
    if (fd_dup < 0) { REPORT_FAIL(name, "dup() failed: %s", strerror(errno)); close(fd); return; }

    int target = fd_dup + 50; /* arbitrary unused-ish fd number */
    if (dup2(fd, target) != target) {
        REPORT_FAIL(name, "dup2() didn't return requested fd: %s", strerror(errno));
        close(fd); close(fd_dup);
        return;
    }

    /* write through original, read through the dup2'd fd */
    write(fd, "dupdata", 7);
    lseek(target, 0, SEEK_SET);
    char buf[8] = {0};
    ssize_t r = read(target, buf, 7);

    close(fd);
    close(fd_dup);
    close(target);
    unlink(path);

    if (r == 7 && memcmp(buf, "dupdata", 7) == 0) {
        REPORT_PASS(name, "shared file offset/data visible across dup'd fds");
    } else {
        REPORT_FAIL(name, "r=%zd buf='%.*s'", r, (int)r, buf);
    }
}

static void test_getdents64(void) {
    const char *name = "getdents64 (via opendir/readdir)";
    const char *dir = "/tmp/synctest_gd";
    mkdir(dir, 0755);

    char f1[128], f2[128];
    snprintf(f1, sizeof(f1), "%s/a.txt", dir);
    snprintf(f2, sizeof(f2), "%s/b.txt", dir);
    close(open(f1, O_CREAT | O_RDWR, 0644));
    close(open(f2, O_CREAT | O_RDWR, 0644));

    DIR *d = opendir(dir);
    if (!d) { REPORT_FAIL(name, "opendir() failed: %s", strerror(errno)); return; }

    int seen_a = 0, seen_b = 0, total = 0;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        total++;
        if (strcmp(ent->d_name, "a.txt") == 0) seen_a = 1;
        if (strcmp(ent->d_name, "b.txt") == 0) seen_b = 1;
    }
    closedir(d);

    unlink(f1);
    unlink(f2);
    rmdir(dir);

    if (seen_a && seen_b) {
        REPORT_PASS(name, "listed %d entries incl. a.txt/b.txt", total);
    } else {
        REPORT_FAIL(name, "missing entries, saw %d total (a=%d b=%d)", total, seen_a, seen_b);
    }
}

/* fork/execve/exit: no wait()/waitpid() available in your table, so we
 * can't reap the child's exit status -- we just verify fork() gives
 * distinct pids and that execve() actually replaces the child image. */
static void test_fork_execve_exit(const char *self_path) {
    const char *name = "fork/execve/exit";
    pid_t me = getpid();
    pid_t pid = fork();

    if (pid < 0) {
        REPORT_FAIL(name, "fork() failed: %s", strerror(errno));
        return;
    }

    if (pid == 0) {
        /* child: re-exec ourselves in "child" mode, which just prints
         * a marker and exits(42). If execve is broken this process
         * instead falls through and exits(99) so the parent can tell. */
        char *const argv[] = { (char *)self_path, (char *)"child", NULL };
        extern char **environ;
        execve(self_path, argv, environ);
        /* only reached if execve failed */
        fprintf(stderr, "[child] execve failed: %s\n", strerror(errno));
        exit(99);
    }

    /* parent */
    if (pid == me || pid <= 0) {
        REPORT_FAIL(name, "fork() returned suspicious child pid %d (parent=%d)", (int)pid, (int)me);
    } else {
        REPORT_PASS(name, "fork() ok (parent=%d child=%d); execve output should appear above/below",
                    (int)me, (int)pid);
    }

    /* no wait() available -- give the child a moment to run/print before
     * we move on and eventually call our own exit(). This is best-effort
     * since there's no real sleep/wait syscall guaranteed either. */
    for (volatile long i = 0; i < 50000000L; i++) { }
}

/* ================================================================
 *  Group 2: syscalls you asked about that are composable from what
 *  the kernel provides (readv/writev via read/write loop, pwrite via
 *  lseek+write). These only "work" if mlibc's generic sysdeps already
 *  provide that fallback -- this test tells you whether it does.
 * ================================================================ */

static void test_readv_writev(void) {
    const char *name = "readv/writev (composed)";
    const char *path = "/tmp/synctest_iov.txt";
    int fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd < 0) { REPORT_FAIL(name, "open() failed: %s", strerror(errno)); return; }

    const char *part1 = "AAAA";
    const char *part2 = "BBBBBB";
    struct iovec wiov[2] = {
        { .iov_base = (void *)part1, .iov_len = 4 },
        { .iov_base = (void *)part2, .iov_len = 6 },
    };

    ssize_t w = writev(fd, wiov, 2);
    if (w != 10) {
        REPORT_UNIMPL(name, "writev() returned %zd, expected 10", w);
        close(fd); unlink(path);
        return;
    }

    lseek(fd, 0, SEEK_SET);
    char b1[4] = {0}, b2[6] = {0};
    struct iovec riov[2] = {
        { .iov_base = b1, .iov_len = 4 },
        { .iov_base = b2, .iov_len = 6 },
    };
    ssize_t r = readv(fd, riov, 2);

    close(fd);
    unlink(path);

    if (r == 10 && memcmp(b1, "AAAA", 4) == 0 && memcmp(b2, "BBBBBB", 6) == 0) {
        REPORT_PASS(name, "writev+readv round-tripped 10 bytes across 2 iovecs");
    } else {
        REPORT_FAIL(name, "r=%zd b1='%.4s' b2='%.6s'", r, b1, b2);
    }
}

static void test_pwrite_pread(void) {
    const char *name = "pwrite/pread (composed)";
    const char *path = "/tmp/synctest_pwrite.txt";
    int fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd < 0) { REPORT_FAIL(name, "open() failed: %s", strerror(errno)); return; }

    write(fd, "0123456789", 10);          /* offset now at 10 */
    ssize_t pw = pwrite(fd, "XY", 2, 3);  /* should not move the fd's offset */
    if (pw != 2) {
        REPORT_UNIMPL(name, "pwrite() returned %zd", pw);
        close(fd); unlink(path);
        return;
    }

    off_t off_after = lseek(fd, 0, SEEK_CUR);

    char pbuf[3] = {0};
    ssize_t pr = pread(fd, pbuf, 2, 3);

    close(fd);
    unlink(path);

    if (pw == 2 && off_after == 10 && pr == 2 && memcmp(pbuf, "XY", 2) == 0) {
        REPORT_PASS(name, "pwrite left fd offset untouched (=10) and wrote correct bytes");
    } else {
        REPORT_FAIL(name, "pw=%zd off_after=%lld pr=%zd pbuf='%.2s'",
                    pw, (long long)off_after, pr, pbuf);
    }
}

/* ================================================================
 *  Group 3: things you likely do NOT have -- these should fail
 *  cleanly (ENOSYS or similar), not crash. We just report N/I.
 * ================================================================ */

static void test_unimplemented_probes(void) {
#ifdef HAVE_STAT_H
    struct stat st;
    if (stat("/tmp", &st) == 0) REPORT_PASS("stat", "unexpectedly works");
    else REPORT_UNIMPL("stat", "");
#else
    REPORT_SKIP("stat", "sys/stat.h not found at compile time");
#endif

#ifdef HAVE_SOCKET_H
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s >= 0) { REPORT_PASS("socket", "unexpectedly works (fd=%d)", s); close(s); }
    else REPORT_UNIMPL("socket", "");
#else
    REPORT_SKIP("socket", "sys/socket.h not found at compile time");
#endif

#ifdef HAVE_MMAN_H
    void *m = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (m != MAP_FAILED) { REPORT_PASS("mmap", "unexpectedly works"); munmap(m, 4096); }
    else REPORT_UNIMPL("mmap", "");
#else
    REPORT_SKIP("mmap", "sys/mman.h not found at compile time");
#endif

#ifdef HAVE_SIGNAL_H
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    if (sigaction(SIGUSR1, &sa, NULL) == 0) REPORT_PASS("sigaction", "unexpectedly works");
    else REPORT_UNIMPL("sigaction", "");
#else
    REPORT_SKIP("sigaction", "signal.h not found at compile time");
#endif

#ifdef HAVE_WAIT_H
    int wstatus;
    pid_t w = waitpid(-1, &wstatus, WNOHANG);
    if (w >= 0) REPORT_PASS("waitpid", "unexpectedly returned %d", (int)w);
    else REPORT_UNIMPL("waitpid", "");
#else
    REPORT_SKIP("waitpid", "sys/wait.h not found at compile time");
#endif
}

/* ================================================================ */

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "child") == 0) {
        /* This branch only runs when execve() in test_fork_execve_exit
         * successfully re-executed this binary. */
        printf("[child] execve() succeeded, pid=%d\n", (int)getpid());
        fflush(stdout);
        exit(42);
    }
    

    printf("=== syscall capability test ===\n");
    printf("(argv[0] = %s)\n\n", argv[0]);

    test_open_write_read_close();
    test_lseek();
    test_getpid();
    test_brk_sbrk();
    test_getcwd_chdir_mkdir_rmdir();
    test_unlink();
    test_access();
    test_dup_dup2();
    test_getdents64();
    // test_fork_execve_exit(argv[0]);

    printf("\n-- composed/wrapper syscalls --\n");
    test_readv_writev();
    test_pwrite_pread();

    printf("\n-- probes for syscalls likely NOT implemented --\n");
    test_unimplemented_probes();

    printf("\n=== summary: %d passed, %d failed, %d skipped/unimplemented ===\n",
           g_pass, g_fail, g_skip);

    return g_fail > 0 ? 1 : 0;
}