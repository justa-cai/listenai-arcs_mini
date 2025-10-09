/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "spiflash.h"
#include "arcs_ap.h"
#include "flash_if.h"

static FLASH_DEV remote_flash_dev = {
    .base_addr = CMN_FLASHC_BASE,
    .d_width = 4,
    .sclk_div = 0xff, // 0 means divider=2 //0xff,  //0xff means divider=1
    .run_mod = RUN_WITHOUT_INT,
    .timeout = 0x180000,
};

uint32_t arcs_flash_init(void)
{
    return flash_if_init(&remote_flash_dev, 0, 0);
}
