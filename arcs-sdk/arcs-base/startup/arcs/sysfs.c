#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/times.h>
#include <unistd.h>
#include <reent.h>
#include "core_feature_base.h"

/* Weak implementation attributes ensure these can be overridden */
/* by other implementations */

/* Base implementations of standard syscalls */
__attribute__((weak, used))
int _fstat(int file, struct stat *st) {
    if ((STDOUT_FILENO == file) || (STDERR_FILENO == file)) {
        st->st_mode = S_IFCHR;
        return 0;
    } else {
        errno = EBADF;
        return -1;
    }
}

__attribute__((weak, used))
int _getpid(void) {
    return 1;
}

__attribute__((weak, used))
int _gettimeofday(struct timeval *tp, void *tzp) {
    extern volatile uint32_t SystemCoreClock;  /* Defined in system.c */
    
    if (tp != NULL) {
        /* Get system tick counter and convert to seconds and microseconds */
        uint64_t cycles = __get_rv_cycle();
        
        tp->tv_sec = cycles / SystemCoreClock;
        tp->tv_usec = (cycles % SystemCoreClock) * 1000000 / SystemCoreClock;
    }
    
    return 0;
}

__attribute__((weak, used))
int _isatty(int fd) {
    if (fd >= 0 && fd <= 2) {
        return 1;
    }
    
    errno = EBADF;
    return 0;
}

__attribute__((weak, used))
int _kill(int pid, int sig) {
    errno = EINVAL;
    return -1;
}

__attribute__((weak, used))
int _stat(const char *file, struct stat *st) {
    st->st_mode = S_IFCHR;
    return 0;
}

__attribute__((weak, used))
int _lseek(int file, int ptr, int dir) {
    return 0;
}

__attribute__((weak, used))
int _open(const char* name, int flags, int mode)
{
    errno = ENOSYS;
    return -1;
}


__attribute__((weak, used))
int _close(int file) {
    return -1;
}

__attribute__((weak, used))
int _read(int file, char *ptr, int len) {
    return 0;
}

__attribute__((weak, used))
_ssize_t _write(int file, char *ptr, size_t len) {
    /* 实现基本的写函数，将数据写入到指定的文件描述符 */
    /* 在这个简单实现中，我们假设写入总是成功的 */
    return len;
}

/* Reentrant versions that call the base implementations */
__attribute__((weak, used))
int _fstat_r(struct _reent *r, int file, struct stat *st) {
    return _fstat(file, st);
}

__attribute__((weak, used))
int _getpid_r(struct _reent *r) {
    return _getpid();
}

__attribute__((weak, used))
int _gettimeofday_r(struct _reent *r, struct timeval *tp, void *tzp) {
    return _gettimeofday(tp, tzp);
}

__attribute__((weak, used))
int _isatty_r(struct _reent *r, int fd) {
    return _isatty(fd);
}

__attribute__((weak, used))
int _kill_r(struct _reent *r, int pid, int sig) {
    return _kill(pid, sig);
}

__attribute__((weak, used))
int _stat_r(struct _reent *r, const char *file, struct stat *st) {
    return _stat(file, st);
}

__attribute__((weak, used))
_ssize_t _write_r(struct _reent *r, int file, const void *ptr, size_t len) {
    return _write(file, (char *)ptr, len);
}

