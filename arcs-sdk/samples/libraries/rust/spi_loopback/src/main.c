/*
 * Copyright (c) 2026, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 */
#define LOG_TAG "rust_spi_loopback"
#include <lisa_log.h>
#include "IOMuxManager.h"

extern int rust_main(void);

/*
 * The board's weak lisa_spi0_pinmux() (boards/arcs_evb/pinmux.c) is empty, so
 * route SPI0 to physical pads here: CLK=PA15, MOSI=PA14, MISO=PA13, CS=PA12
 * (ALTER5). Mirrors samples/drivers/devices/lisa_spi/master. Jumper PA14<->PA13
 * to exercise the MOSI->MISO loopback path; otherwise the demo still runs to
 * completion and reports "no loopback".
 */
#ifdef CONFIG_BOARD_ARCS_EVB
void lisa_spi0_pinmux(void)
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 15, CSK_IOMUX_FUNC_ALTER5);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 14, CSK_IOMUX_FUNC_ALTER5);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 13, CSK_IOMUX_FUNC_ALTER5);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 12, CSK_IOMUX_FUNC_ALTER5);
}
#endif

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    LOGI("=== Rust SPI loopback demo ===");
    int ret = rust_main();
    if (ret != 0) {
        LOGE("rust_main returned %d", ret);
    }
    return 0;
}
