// #include <zephyr.h>
#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <sys/times.h>

#include <fcntl.h>

clock_t _times(struct tms* tms)
{
    (void)tms;
    errno = ENOSYS;
    return (clock_t) -1;
}
