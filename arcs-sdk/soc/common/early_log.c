/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>

#include <soc/early_log.h>

#include "ClockManager.h"
#include "pinmux.h"
#include "uart_reg.h"

#if CONFIG_SOC_ARCS
#include "arcs_ap.h"
#elif CONFIG_SOC_VENUSA
#include "venusa_ap.h"
#else
#error "soc_early_log requires SOC_ARCS or SOC_VENUSA"
#endif

#define EARLY_LOG_UART_DIV_M_MAX 1023U
#define EARLY_LOG_UART_DIV_N_MAX 511U
#define EARLY_LOG_TX_WAIT_CYCLES 1000000U
#define EARLY_LOG_PRINTF_BUF_SIZE 256U

#define EARLY_LOG_UART_BAUDRATE CONFIG_CONSOLE_UART_BAUDRATE

#if CONFIG_CONSOLE_UART2_SELECT
#define EARLY_LOG_UART_PORT 2U
#elif CONFIG_CONSOLE_UART1_SELECT
#define EARLY_LOG_UART_PORT 1U
#else
#define EARLY_LOG_UART_PORT 0U
#endif

static UART_RegDef *early_log_uart;

static uint64_t early_log_gcd(uint64_t a, uint64_t b)
{
    while (b != 0) {
        uint64_t t = a % b;
        a = b;
        b = t;
    }

    return a;
}

static int early_log_compute_div(uint32_t uart_clk, uint32_t baudrate, uint32_t *m,
                                 uint32_t *n, uint32_t *divisor_mode)
{
    const uint32_t divisors[] = {4U, 16U};

    if (uart_clk == 0 || baudrate == 0 || m == 0 || n == 0 || divisor_mode == 0) {
        return -1;
    }

    for (uint32_t i = 0; i < sizeof(divisors) / sizeof(divisors[0]); i++) {
        uint32_t div = divisors[i];
        uint64_t target = (uint64_t)div * baudrate;
        uint64_t gcd = early_log_gcd(uart_clk, target);
        uint64_t div_m = uart_clk / gcd;
        uint64_t div_n = target / gcd;

        if (div_n == 0 || div_n > div_m / 2U) {
            continue;
        }

        if (div_m > EARLY_LOG_UART_DIV_M_MAX) {
            div_n = ((uint64_t)EARLY_LOG_UART_DIV_M_MAX * div_n + div_m / 2U) / div_m;
            div_m = EARLY_LOG_UART_DIV_M_MAX;
        }

        if (div_n == 0 || div_n > EARLY_LOG_UART_DIV_N_MAX || div_n > div_m / 2U) {
            continue;
        }

        *m = (uint32_t)div_m;
        *n = (uint32_t)div_n;
        *divisor_mode = div;
        return 0;
    }

    return -1;
}

static UART_RegDef *early_log_uart_get(uint32_t port)
{
    switch (port) {
    case 0:
        return IP_UART0;
    case 1:
        return IP_UART1;
    case 2:
        return IP_UART2;
    default:
        return 0;
    }
}

static void early_log_uart_pinmux(uint32_t port)
{
    switch (port) {
    case 0:
        lisa_uart0_pinmux();
        break;
    case 1:
        lisa_uart1_pinmux();
        break;
    case 2:
        lisa_uart2_pinmux();
        break;
    default:
        break;
    }
}

static void early_log_uart_clk_enable(uint32_t port)
{
    switch (port) {
    case 0:
        __HAL_CRM_UART0_CLK_ENABLE();
        break;
    case 1:
        __HAL_CRM_UART1_CLK_ENABLE();
        break;
    case 2:
        __HAL_CRM_UART2_CLK_ENABLE();
        break;
    default:
        break;
    }
}

static uint32_t early_log_uart_clk_get(uint32_t port)
{
    switch (port) {
    case 0:
        return CRM_GetUart0Freq();
    case 1:
        return CRM_GetUart1Freq();
    case 2:
        return CRM_GetUart2Freq();
    default:
        return 0;
    }
}

static void early_log_uart_clk_div_set(uint32_t port, uint32_t n, uint32_t m)
{
    switch (port) {
    case 0:
        HAL_CRM_SetUart0ClkDiv(n, m);
        break;
    case 1:
        HAL_CRM_SetUart1ClkDiv(n, m);
        break;
    case 2:
        HAL_CRM_SetUart2ClkDiv(n, m);
        break;
    default:
        break;
    }
}

static int early_log_wait_tx_space(UART_RegDef *uart)
{
    for (uint32_t i = 0; i < EARLY_LOG_TX_WAIT_CYCLES; i++) {
        if (uart->REG_STATUS.bit.TX_FIFO_SPACE) {
            return 0;
        }
    }

    return -1;
}

void soc_early_log_init(void)
{
    const uint32_t port = EARLY_LOG_UART_PORT;
    uint32_t m;
    uint32_t n;
    uint32_t div;
    UART_RegDef *uart = early_log_uart_get(port);

    if (uart == 0) {
        return;
    }

    early_log_uart_pinmux(port);

    early_log_uart_clk_enable(port);
    early_log_uart_clk_div_set(port, 1, 1);

    if (early_log_compute_div(early_log_uart_clk_get(port), EARLY_LOG_UART_BAUDRATE, &m, &n,
                              &div) != 0) {
        return;
    }

    early_log_uart_clk_div_set(port, n, m);

    uart->REG_IRQ_MASK.all = 0;
    uart->REG_CTRL.all = 0;
    uart->REG_CTRL.bit.DIVISOR_MODE = (div == 16U) ? 1U : 0U;
    uart->REG_CMD_SET.bit.TX_FIFO_RESET = 1;
    uart->REG_CMD_SET.bit.RX_FIFO_RESET = 1;
    uart->REG_CTRL.bit.DATA_BITS = 1;
    uart->REG_CTRL.bit.ENABLE = 1;
    uart->REG_STATUS.all = 1;

    early_log_uart = uart;
}

int soc_early_log_is_ready(void)
{
    return early_log_uart != 0;
}

void soc_early_log_write(const char *msg)
{
    UART_RegDef *uart = early_log_uart;

    if (uart == 0 || msg == 0) {
        return;
    }

    while (*msg != '\0') {
        if (early_log_wait_tx_space(uart) != 0) {
            return;
        }
        uart->REG_RXTX_BUFFER.all = (uint8_t)*msg++;
    }

    for (uint32_t i = 0; i < EARLY_LOG_TX_WAIT_CYCLES; i++) {
        if (!uart->REG_STATUS.bit.TX_ACTIVE) {
            break;
        }
    }
}

void soc_early_log_vprintf(const char *fmt, va_list ap)
{
    char buf[EARLY_LOG_PRINTF_BUF_SIZE];
    int len;

    if (!soc_early_log_is_ready() || fmt == 0) {
        return;
    }

    len = vsnprintf(buf, sizeof(buf), fmt, ap);
    if (len <= 0) {
        return;
    }

    soc_early_log_write(buf);
}
