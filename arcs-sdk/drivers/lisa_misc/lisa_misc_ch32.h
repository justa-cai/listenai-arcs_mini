/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "lisa_ch32v003.h"

#include <stdbool.h>
#include <stdint.h>

#include "lisa_device.h"
#include "lisa_mutex.h"

#ifndef CONFIG_LISA_CH32V003_INIT_DELAY_MS
#define CONFIG_LISA_CH32V003_INIT_DELAY_MS 40
#endif

#ifndef CONFIG_LISA_CH32V003_EXPECTED_PROTOCOL_VERSION
#define CONFIG_LISA_CH32V003_EXPECTED_PROTOCOL_VERSION 14
#endif

#ifndef CONFIG_LISA_CH32V003_BUSY_WAIT_MS
#define CONFIG_LISA_CH32V003_BUSY_WAIT_MS 10
#endif

#ifndef CONFIG_LISA_CH32V003_BUSY_WAIT_RETRY
#define CONFIG_LISA_CH32V003_BUSY_WAIT_RETRY 3
#endif

#define CH32V003_STATUS_BUSY (1U << 0)

#define CH32V003_ID_REG   0x01
#define CH32V003_VER_REG  0x02
#define CH32V003_STA_REG  0x03

#define CH32V003_DEVICE_LOCK(priv)                                                                                \
    do {                                                                                                           \
        if ((priv)->mutex) {                                                                                       \
            lisa_mutex_lock((priv)->mutex, LISA_OS_WAIT_FOREVER);                                                  \
        }                                                                                                          \
    } while (0)

#define CH32V003_DEVICE_UNLOCK(priv)                                                                              \
    do {                                                                                                           \
        if ((priv)->mutex) {                                                                                       \
            lisa_mutex_unlock((priv)->mutex);                                                                      \
        }                                                                                                          \
    } while (0)

typedef struct {
    lisa_device_t *i2c_dev;
    lisa_mutex_t *mutex;
    const char *i2c_name;
    uint16_t i2c_addr;
    uint8_t chip_id;
    uint8_t version;
    bool probed;
} ch32v003_priv_t;

int ch32v003_get_priv(ch32v003_priv_t **out);
int ch32v003_read_reg_unlocked(ch32v003_priv_t *priv, uint8_t reg, uint8_t *value);
int ch32v003_write_checked_unlocked(ch32v003_priv_t *priv, uint8_t reg, uint8_t value);
int ch32v003_probe_unlocked(ch32v003_priv_t *priv);
