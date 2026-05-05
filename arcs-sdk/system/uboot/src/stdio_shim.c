#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "tinyprintf.h"

#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif

struct _reent;

extern int _write_r(struct _reent *ptr, int fd, const void *buf, size_t cnt);

static int boot_stdio_write(const char *data, size_t len)
{
    if (data == NULL || len == 0) {
        return 0;
    }

    return (int)_write_r(NULL, STDOUT_FILENO, data, len);
}

int vsnprintf(char *str, size_t size, const char *format, va_list args)
{
    return tfp_vsnprintf(str, size, format, args);
}

int snprintf(char *str, size_t size, const char *format, ...)
{
    va_list args;
    int ret;

    va_start(args, format);
    ret = tfp_vsnprintf(str, size, format, args);
    va_end(args);

    return ret;
}

int vsprintf(char *str, const char *format, va_list args)
{
    return tfp_vsprintf(str, format, args);
}

int sprintf(char *str, const char *format, ...)
{
    va_list args;
    int ret;

    va_start(args, format);
    ret = tfp_vsprintf(str, format, args);
    va_end(args);

    return ret;
}

int vprintf(const char *format, va_list args)
{
    char buffer[256];
    int ret;
    size_t len;

    ret = tfp_vsnprintf(buffer, sizeof(buffer), format, args);
    if (ret <= 0) {
        return ret;
    }

    len = (size_t)ret;
    if (len >= sizeof(buffer)) {
        len = sizeof(buffer) - 1;
    }

    boot_stdio_write(buffer, len);
    return ret;
}

int printf(const char *format, ...)
{
    va_list args;
    int ret;

    va_start(args, format);
    ret = vprintf(format, args);
    va_end(args);

    return ret;
}

int puts(const char *str)
{
    if (str != NULL) {
        boot_stdio_write(str, strlen(str));
    }

    return boot_stdio_write("\n", 1) < 0 ? EOF : 0;
}

int putchar(int ch)
{
    char c = (char)ch;

    if (boot_stdio_write(&c, 1) < 0) {
        return EOF;
    }

    return (unsigned char)c;
}
