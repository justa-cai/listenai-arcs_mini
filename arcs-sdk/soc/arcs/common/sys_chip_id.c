/*
 * Copyright (c) 2025, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stdint.h>

#include "chip.h"

void sys_arch_chip_id_get(uint8_t bytes[8])
{
    uint32_t w0 = IP_EFUSE_CTRL->REG_AUTO_LOAD_02.all;
    uint32_t w1 = IP_EFUSE_CTRL->REG_AUTO_LOAD_03.all;

    bytes[0] = (uint8_t)(w0);
    bytes[1] = (uint8_t)(w0 >> 8);
    bytes[2] = (uint8_t)(w0 >> 16);
    bytes[3] = (uint8_t)(w0 >> 24);
    bytes[4] = (uint8_t)(w1);
    bytes[5] = (uint8_t)(w1 >> 8);
    bytes[6] = (uint8_t)(w1 >> 16);
    bytes[7] = (uint8_t)(w1 >> 24);
}

/* ARCS 因历史原因（u-boot device_id_str_get / boot ADB device serial 用小写
 * %02x 输出），sys_chip_id_get() 必须返回小写以保持与既有 boot 序列号一致。 */
bool sys_arch_chip_id_uppercase(void)
{
    return false;
}
