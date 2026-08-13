/* See LICENSE of license details. */
#include <errno.h>

#undef errno
extern int errno;

__attribute__((weak)) int _close(int fd)
{
    errno = EBADF;
    return -1;
}
