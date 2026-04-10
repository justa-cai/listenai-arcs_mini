/*
 * printk.c - Kernel debug output
 *
 * Uses vsnprintf + console_write to avoid the heavy stack usage of
 * newlib's vprintf/vfprintf (which allocates ~1 KB FILE buffer on stack).
 */

#include <stdio.h>
#include <stdarg.h>
#include "console.h"

#define PRINTK_BUF_SIZE 256

int vprintk(const char *format, va_list args)
{
    char buf[PRINTK_BUF_SIZE];
    int len = vsnprintf(buf, sizeof(buf), format, args);
    if (len > 0) {
        if (len >= (int)sizeof(buf))
            len = sizeof(buf) - 1;
        console_write(buf, len);
    }
    return len;
}

int printk(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int len = vprintk(format, args);
    va_end(args);
    return len;
}

void tfp_vprintf(const char *fmt, va_list ap)
{
    vprintk(fmt, ap);
}

void tfp_printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vprintk(fmt, args);
    va_end(args);
}
