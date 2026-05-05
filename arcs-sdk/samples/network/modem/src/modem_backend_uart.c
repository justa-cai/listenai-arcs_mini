/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "IOMuxManager.h"
#include "modem_backend.h"

#define SAMPLE_MODEM_UART2_TX_PAD   CSK_IOMUX_PAD_A
#define SAMPLE_MODEM_UART2_TX_PIN   15
#define SAMPLE_MODEM_UART2_RX_PAD   CSK_IOMUX_PAD_A
#define SAMPLE_MODEM_UART2_RX_PIN   16
#define SAMPLE_MODEM_UART2_FUNC     CSK_IOMUX_FUNC_ALTER4

/*
 * Override the weak board default so this sample owns the UART2 pin choice
 * instead of the AT transport layer.
 */
void lisa_uart2_pinmux(void)
{
    IOMuxManager_PinConfigure(SAMPLE_MODEM_UART2_TX_PAD, SAMPLE_MODEM_UART2_TX_PIN,
                              SAMPLE_MODEM_UART2_FUNC);
    IOMuxManager_PinConfigure(SAMPLE_MODEM_UART2_RX_PAD, SAMPLE_MODEM_UART2_RX_PIN,
                              SAMPLE_MODEM_UART2_FUNC);
}

const char *sample_modem_backend_name(void)
{
    return "uart";
}

lisa_modem_t *sample_modem_open(const char *uart_dev)
{
    return lisa_modem_create_uart(uart_dev, 0);
}

void sample_modem_close(lisa_modem_t *modem)
{
    lisa_modem_destroy(modem);
}
