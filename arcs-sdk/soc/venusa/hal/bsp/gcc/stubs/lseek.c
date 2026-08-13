/* See LICENSE of license details. */
#include <errno.h>

#undef errno
extern int errno;

__attribute__((weak)) int _lseek(int file, int offset, int whence)
{
    return 0;
}
