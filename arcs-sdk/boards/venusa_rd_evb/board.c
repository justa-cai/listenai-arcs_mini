/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file board.c
 * @brief 板级初始化实现
 */

#include "board.h"

const char* board_get_name(void)
{
    return CONFIG_BOARD_NAME;
}
