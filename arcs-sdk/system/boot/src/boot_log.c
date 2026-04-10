#include "stdint.h"
#include "log_print.h"

#include <stdio.h>
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
#define CONFIG_SYSLOG_UART_TX_PIN          3
#define CONFIG_SYSLOG_UART_TX_PIN_FUNC_SEL 0x2

static void __log_io_config(int dbg)
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CONFIG_SYSLOG_UART_TX_PIN, CONFIG_SYSLOG_UART_TX_PIN_FUNC_SEL);
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

int bootlog_init(int dbg, uint32_t baudrate)
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

static void int_to_str(int value, char *buf, size_t *written)
{
    if (value < 0) {
        buf[(*written)++] = '-';
        value = -value;
    }
    if (value == 0) {
        buf[(*written)++] = '0';
        return;
    }

    char temp[10];
    size_t index = 0;

    while (value > 0) {
        temp[index++] = (value % 10) + '0';
        value /= 10;
    }

    for (size_t i = 0; i < index; i++) {
        if (*written < 512 - 1) {
            buf[(*written)++] = temp[index - 1 - i];
        }
    }
}

static void uint_to_hex_str(unsigned int value, char *buf, size_t *written)
{
    if (value == 0) {
        buf[(*written)++] = '0';
        return;
    }

    char temp[8];
    size_t index = 0;

    while (value > 0) {
        unsigned int digit = value % 16;
        temp[index++] = (digit < 10) ? (digit + '0') : (digit - 10 + 'a');
        value /= 16;
    }

    for (size_t i = 0; i < index; i++) {
        if (*written < 512 - 1) {
            buf[(*written)++] = temp[index - 1 - i];
        }
    }
}

int bootlog_vsnprintf(char *buf, size_t size, const char *format, va_list args)
{
    size_t written = 0;
    const char *p;

    for (p = format; *p != '\0'; p++) {
        if (*p != '%') {
            if (written < size - 1) {
                buf[written++] = *p;
            }
            continue;
        }

        p++;
        switch (*p) {
        case 'd': {
            int value = va_arg(args, int);
            int_to_str(value, buf, &written);
            break;
        }
        case 'x': {
            unsigned int value = va_arg(args, unsigned int);
            uint_to_hex_str(value, buf, &written);
            break;
        }
        case 's': {
            const char *str = va_arg(args, const char *);
            while (*str && written < size - 1) {
                buf[written++] = *str++;
            }
            break;
        }
        default:
            if (written < size - 1) {
                buf[written++] = '%';
                buf[written++] = *p;
            }
            break;
        }
    }

    if (written < size) {
        buf[written] = '\0';
    } else if (size > 0) {
        buf[size - 1] = '\0';
    }

    return written;
}

void bootlog_output(const char *format, va_list args)
{
    static uint8_t buffer[512] = {0};
    uint32_t len = bootlog_vsnprintf((char *)buffer, 512 - 1, format, args);
    buffer[len] = '\0';
    syslog_write(buffer, len);
}

int vprintk(const char *format, va_list args)
{
    bootlog_output(format, args);

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
