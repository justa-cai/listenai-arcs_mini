/*
 * printk.c - Kernel debug output
 *
 * Uses vsnprintf + console_write to avoid the heavy stack usage of
 * newlib's vprintf/vfprintf (which allocates ~1 KB FILE buffer on stack).
 */

#include <stdio.h>
#include <stdarg.h>

#if CONFIG_CONSOLE
#include "console.h"
#endif

#define PRINTK_BUF_SIZE 256

#if defined(CFG_AMP_IPC) && defined(CFG_AMP_IPC_SLAVE) && defined(CONFIG_ARCS_HAL_IPC_PRINT)
#include "tinyprintf.h"
extern int32_t ipc_slave_print(char *string, int32_t len);

struct ipc_print_buffer {
    char *buf;
    int capacity;
    int len;
};

static void ipc_print_putcf(void *p, char c)
{
    struct ipc_print_buffer *data = (struct ipc_print_buffer *)p;

    if (data->len < data->capacity) {
        data->buf[data->len] = c;
    }
    data->len++;
}

static int tfp_try_ipc_output(const char *fmt, va_list ap)
{
    char buf[256];
    int out_len;
    struct ipc_print_buffer data = {
        .buf = buf,
        .capacity = sizeof(buf) - 1,
        .len = 0,
    };

    tfp_format(&data, ipc_print_putcf, fmt, ap);
    out_len = data.len < data.capacity ? data.len : data.capacity;
    buf[out_len] = '\0';

    return ipc_slave_print(buf, out_len);
}
#endif

int vprintk(const char *format, va_list args)
{
    char buf[PRINTK_BUF_SIZE];
    int len = vsnprintf(buf, sizeof(buf), format, args);
    if (len > 0) {
        if (len >= (int)sizeof(buf))
            len = sizeof(buf) - 1;
#if CONFIG_CONSOLE
        console_write(buf, len);
#elif defined(CFG_AMP_IPC) && defined(CFG_AMP_IPC_SLAVE) && defined(CONFIG_ARCS_HAL_IPC_PRINT)
        ipc_slave_print(buf, len);
#endif
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
#if defined(CFG_AMP_IPC) && defined(CFG_AMP_IPC_SLAVE) && defined(CONFIG_ARCS_HAL_IPC_PRINT)
    {
        va_list ipc_ap;

        va_copy(ipc_ap, ap);
        if (tfp_try_ipc_output(fmt, ipc_ap) == 0) {
            va_end(ipc_ap);
            return;
        }
        va_end(ipc_ap);
    }
#endif

    vprintk(fmt, ap);
}

void tfp_printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

#if defined(CFG_AMP_IPC) && defined(CFG_AMP_IPC_SLAVE) && defined(CONFIG_ARCS_HAL_IPC_PRINT)
    {
        va_list ipc_args;

        va_copy(ipc_args, args);
        if (tfp_try_ipc_output(fmt, ipc_args) == 0) {
            va_end(ipc_args);
            va_end(args);
            return;
        }
        va_end(ipc_args);
    }
#endif

    vprintk(fmt, args);
    va_end(args);
}
