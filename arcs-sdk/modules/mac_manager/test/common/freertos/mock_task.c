/**
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mock_task.h"

#include <time.h>


DEFINE_FAKE_VALUE_FUNC(unsigned int, xTaskGetTickCount);

static unsigned int custom_xTaskGetTickCount(void)
{
    return (unsigned int)time(NULL);
}


void mock_task_init(void)
{
    RESET_FAKE(xTaskGetTickCount);

    xTaskGetTickCount_fake.custom_fake = custom_xTaskGetTickCount;
}

void mock_task_reset(void)
{
    mock_task_init();
}