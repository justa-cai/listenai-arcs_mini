/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "../lisa_misc/lisa_misc_ch32.h"

#define CH32V003_DIR_REG_BASE       0x10
#define CH32V003_OUT_REG_BASE       0x11
#define CH32V003_OD_REG_BASE        0x12
#define CH32V003_IN_REG_BASE        0x13
#define CH32V003_PULL_EN_REG_BASE   0x14
#define CH32V003_PULL_CTRL_REG_BASE 0x15

#define CH32V003_DIR_REG(index)       (CH32V003_DIR_REG_BASE + ((index) * 0x10))
#define CH32V003_OUT_REG(index)       (CH32V003_OUT_REG_BASE + ((index) * 0x10))
#define CH32V003_OD_REG(index)        (CH32V003_OD_REG_BASE + ((index) * 0x10))
#define CH32V003_IN_REG(index)        (CH32V003_IN_REG_BASE + ((index) * 0x10))
#define CH32V003_PULL_EN_REG(index)   (CH32V003_PULL_EN_REG_BASE + ((index) * 0x10))
#define CH32V003_PULL_CTRL_REG(index) (CH32V003_PULL_CTRL_REG_BASE + ((index) * 0x10))

#define CH32V003_GPIO_MAX_PINS 8
