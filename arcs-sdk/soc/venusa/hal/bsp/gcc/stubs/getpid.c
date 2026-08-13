/* See LICENSE of license details. */
#include <errno.h>

__attribute__((weak)) int _getpid(void)
{
    return 1;
}
