#include "stdint.h"
#include "log_print.h"
#include "syslog.h"

#include <stdio.h>
#include "uart_reg.h"
#include "chip.h"
#include "ClockManager.h"
#include "IOMuxManager.h"

static UART_RegDef *uart = NULL;
static syslog_fallback_writer_t fallback_writer = NULL;

#define SYSLOG_UART_WAIT_SPINS 1000000U

static inline bool syslog_uart_wait_fifo_space(void)
{
    uint32_t spins = SYSLOG_UART_WAIT_SPINS;

    while (!uart->REG_STATUS.bit.TX_FIFO_SPACE && spins--) {
    }

    return uart->REG_STATUS.bit.TX_FIFO_SPACE != 0;
}

static inline void syslog_uart_flush(void)
{
    uint32_t spins = SYSLOG_UART_WAIT_SPINS;

    while (spins--) {
        if (!uart->REG_STATUS.bit.TX_ACTIVE) {
            return;
        }
    }
}

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
    int32_t ret = 0;

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
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CONFIG_SYSLOG_UART_TX_PIN, CONFIG_SYSLOG_UART_TX_PIN_FUNC_SEL);
}

void syslog_fallback_writer_set(syslog_fallback_writer_t writer)
{
    fallback_writer = writer;
}

void syslog_fallback_writer_clear(void)
{
    fallback_writer = NULL;
}

int syslog_write(const char *data, int len)
{
    if (data == NULL || len == 0) {
        return 0;
    }

    if (uart == NULL && fallback_writer != NULL) {
        return fallback_writer(data, len);
    }

    if (uart == NULL) {
        return 0;
    }

    for (int i = 0; i < len; i++) {
        if (!syslog_uart_wait_fifo_space()) {
            return i;
        }

        uart->REG_RXTX_BUFFER.all = data[i];
        if (data[i] == '\n') {
            syslog_uart_flush();
        }
    }

    return len;
}

int syslog_init(int dbg, uint32_t baudrate)
{
    uint32_t m, n, div, uart_base, cmn_reg;

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
    default:
        uart = (UART_RegDef *)UART1_BASE;

        __HAL_CRM_UART1_CLK_ENABLE();
        HAL_CRM_SetUart1ClkDiv(n, m);
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
    static uint8_t buffer[512] = {0};
    uint32_t len = vsnprintf((char *)buffer, 512 - 1, format, args);
    buffer[len] = '\0';
    syslog_write(buffer, len);
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

#if CONFIG_SYSLOG_STDOUT
#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif
#ifndef STDERR_FILENO
#define STDERR_FILENO 2
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
        syslog_write(buf, cnt);
        return cnt;
    default:
        return _write(fd, buf, cnt);
    }
}
#endif

void __assert_func(const char *file, int line, const char *func, const char *expr)
{
    extern void trace_flush(void);
    // trace_flush();
    const char *fpos = __builtin_strrchr(file, '/');
    printk("\r\nassert at file:%s:%d, expr:%s\r\n", fpos ? fpos + 1 : file, line, expr);
    __builtin_trap();
    while (1);
}
