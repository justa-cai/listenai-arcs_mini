/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "led"
#include <lisa_log.h>

#include "app_registry.h"

static int led_init(void)
{
    LOGI("LED module initialized");
    return 0;
}

/* 在本文件中注册，无需修改 main.c */
APP_MODULE_REGISTER(led, led_init);
