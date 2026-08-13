/* See LICENSE of license details. */
#include <errno.h>
#undef errno
extern int errno;

__attribute__((weak)) int _kill(int pid, int sig)
{
    errno = EINVAL;
    return -1;
}
