/*
 * Copyright (c) 2025, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "sys/chip_id.h"

void sys_arch_chip_id_get(uint8_t bytes[8]);
bool sys_arch_chip_id_uppercase(void);

const char *sys_chip_id_get(void)
{
    static char buf[16 + 1];
    uint8_t bytes[8] = {0};
    const char *fmt;

    sys_arch_chip_id_get(bytes);
    fmt = sys_arch_chip_id_uppercase() ? "%02X" : "%02x";
    for (int i = 0; i < 8; ++i) {
        snprintf(buf + i * 2, 3, fmt, bytes[i]);
    }
    return buf;
}
