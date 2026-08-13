/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file ic_message_auto_init.c
 * @brief SYS_INIT based ic_message initialization.
 */

#include "ic_message.h"
#include "sys_init.h"
#include "lisa_log.h"

static int ic_message_auto_init(void)
{
    int ret = ic_message_init();

    if (ret == IC_MESSAGE_ERR_NONE) {
        ret = 0;
    } else {
        ret = -1;
    }

    LOGI("ic message init done, r: %d", ret);

    return ret;
}

SYS_INIT(ic_message_auto_init, SYS_INIT_LEVEL_PRE_DEVICES_INIT, SYS_INIT_SUB_PRIORITY_LATE);
