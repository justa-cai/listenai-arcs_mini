/* See LICENSE of license details. */
#include <unistd.h>

__attribute__((weak)) int _isatty(int fd)
{
    return 1;
}
