#include "stdint.h"
#include "log_print.h"
#include "tinyprintf.h"

#include <stdio.h>
#include <string.h>
#include "uart_reg.h"
#include "chip.h"
#include "ClockManager.h"
#include "IOMuxManager.h"

static UART_RegDef *uart = NULL;

static uint32_t compute_gcd(uint32_t a, uint32_t c)
{
    uint32_t t;
    while (c != 0) {
        t = a % c;
        a = c;
        c = t;
    }
    return a;
}

static int32_t __log_compute_div(int dbg, uint32_t baudrate, uint32_t *pm, uint32_t *pn, uint32_t div)
{
    uint32_t gcd, m, n, tmp;

    tmp = div * baudrate;
    uint32_t uart_clk;

    if (dbg == 0) {
        uart_clk = 24000000;
    } else if (dbg == 1) {
        uart_clk = 24000000;
    }

    gcd = compute_gcd(uart_clk, tmp);
    m = uart_clk / gcd;
    n = tmp / gcd;

    if ((m > 1023) || (n > 511)) {
        return -1;
    }

    *pm = m;
    *pn = n;

    return 0;
}

static int32_t log_compute_div(int dbg, uint32_t baudrate, uint32_t *pm, uint32_t *pn, uint32_t *pdiv)
{
    int32_t ret = 0;

    do {
        if (__log_compute_div(dbg, baudrate, pm, pn, 4) == 0) {
            *pdiv = 4;
            break;
        }
        if (__log_compute_div(dbg, baudrate, pm, pn, 16) == 0) {
            *pdiv = 16;
            break;
        }
        ret = -1;
    } while (0);

    return ret;
}

static void __log_io_config(int dbg)
{
    IOMuxManager_PinConfigure(CONFIG_SYSLOG_UART_TX_PORT, CONFIG_SYSLOG_UART_TX_PIN, CONFIG_SYSLOG_UART_TX_PIN_FUNC_SEL);
}

int syslog_write(const char *data, int len)
{
    if (uart == NULL || data == NULL || len == 0) {
        return 0;
    }

    for (int i = 0; i < len; i++) {
        uart->REG_RXTX_BUFFER.all = data[i];
        while (!uart->REG_STATUS.bit.TX_FIFO_SPACE)
            ;
    }

    return len;
}

int syslog_init(int dbg, uint32_t baudrate)
{
    uint32_t m, n, div;

    if (log_compute_div(dbg, baudrate, &m, &n, &div) < 0) {
        return -1;
    }

    __log_io_config(dbg);

    switch (dbg) {
    case 0:
        uart = (UART_RegDef *)UART0_BASE;

        __HAL_CRM_UART0_CLK_ENABLE();
        HAL_CRM_SetUart0ClkDiv(n, m);
        break;
    case 1:
    
        uart = (UART_RegDef *)UART1_BASE;

        __HAL_CRM_UART1_CLK_ENABLE();
        HAL_CRM_SetUart1ClkDiv(n, m);
        break;
    case 2:
        uart = (UART_RegDef *)UART2_BASE;

        __HAL_CRM_UART2_CLK_ENABLE();
        HAL_CRM_SetUart2ClkDiv(n, m);
        break;
    default:
        break;
    }

    uart->REG_IRQ_MASK.all = 0;
    uart->REG_CTRL.all = 0;

    if (div == 16) {
        uart->REG_CTRL.bit.DIVISOR_MODE = 1; // 0: sclk/4; 1: sclk/16
    } else {
        uart->REG_CTRL.bit.DIVISOR_MODE = 0;
    }

    uart->REG_CMD_SET.bit.TX_FIFO_RESET = 1; // reset tx fifo
    uart->REG_CMD_SET.bit.RX_FIFO_RESET = 1; // reset rx fifo

    uart->REG_CTRL.bit.DATA_BITS = 1; // 8bits
    uart->REG_CTRL.bit.ENABLE = 1;    // enable uart
    uart->REG_STATUS.all = 1;         // clear line error bits

    return 0;
}

void (*syslog_output_hook)(const char *, va_list) = NULL;

int syslog_hook_set(void (*hook)(const char *, va_list))
{
    syslog_output_hook = hook;
    return 0;
}

void syslog_output_default(const char *format, va_list args)
{
    static char buffer[512] = {0};
    uint32_t len = vsnprintf(buffer, 512 - 1, format, args);
    buffer[len] = '\0';
    syslog_write((const char *)buffer, len);
}

void syslog_raw_output_v(const char *format, va_list args)
{
    if (syslog_output_hook) {
        syslog_output_hook(format, args);
    } else {
        syslog_output_default(format, args);
    }
}

int vprintk(const char *format, va_list args)
{
#if CONFIG_SYSLOG_PRINTK_REDIRECT
    if (syslog_output_hook) {
        syslog_output_hook(format, args);
    } else {
        syslog_output_default(format, args);
    }
#else
    syslog_output_default(format, args);
#endif

    return 0;
}

int printk(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vprintk(format, args);
    va_end(args);
    return 0;
}

void tfp_vprintf(const char *fmt, va_list ap)
{
    if (syslog_output_hook) {
        syslog_output_hook(fmt, ap);
    } else {
        syslog_output_default(fmt, ap);
    }
}

void tfp_printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    tfp_vprintf(fmt, args);
    va_end(args);
}


#if CONFIG_SYSLOG_STDOUT
#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif
#ifndef STDERR_FILENO
#define STDERR_FILENO 2
#endif

#if CONFIG_SYSLOG_PRINTF_REDIRECT

#define PRINTF_TEMP_BUF_CHUNK_SIZE 128

static void redirect_printf_output(const char *format, ...)
{
    va_list dummy_args;
    va_start(dummy_args, format);
    syslog_raw_output_v(format, dummy_args);
    va_end(dummy_args);
}

static void redirect_printf(void *buf, size_t cnt)
{
    char temp_buf[PRINTF_TEMP_BUF_CHUNK_SIZE + 1];  // +1 for null terminator
    char *input = (char *)buf;
    size_t remaining = cnt;
    size_t offset = 0;
    va_list dummy_args;
    memset(&dummy_args, 0, sizeof(dummy_args));
    
    // 分批处理数据
    while (remaining > 0) {
        size_t chunk_len = (remaining > PRINTF_TEMP_BUF_CHUNK_SIZE) ? PRINTF_TEMP_BUF_CHUNK_SIZE : remaining;
        
        memcpy(temp_buf, input + offset, chunk_len);
        temp_buf[chunk_len] = '\0';
        
        redirect_printf_output(temp_buf, dummy_args);
        
        offset += chunk_len;
        remaining -= chunk_len;
    }
}
#endif

_ssize_t _write_r(struct _reent *ptr, int fd, void *buf, size_t cnt)
{
    extern int _write(int file, char *data, int len);
    if (!buf || cnt == 0) {
        return 0;
    }

    switch (fd) {
    case STDOUT_FILENO:
    case STDERR_FILENO:
#if CONFIG_SYSLOG_PRINTF_REDIRECT
        redirect_printf(buf, cnt);
#else
        syslog_write(buf, cnt);
#endif
        return cnt;
    default:
        return _write(fd, buf, cnt);
    }
}
#endif

void __assert_func(const char *file, int line, const char *func, const char *expr)
{
    CLOG_FLUSH();
    CLOG("assert at file:%s:%d, expr:%s\r\n", file, line, expr);
    __builtin_trap();
}


