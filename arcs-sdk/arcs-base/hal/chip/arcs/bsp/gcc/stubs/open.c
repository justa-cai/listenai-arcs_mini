/* See LICENSE of license details. */
#include <errno.h>

#undef errno
extern int errno;

__attribute__((weak)) int _open(const char* name, int flags, int mode)
{
    errno = ENOSYS;
    return -1;
}
