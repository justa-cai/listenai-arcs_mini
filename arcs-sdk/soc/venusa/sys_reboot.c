/*
 * Copyright (c) 2026, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sys/reboot.h"

#include "PowerManager.h"
#include "core_feature_base.h"

void sys_arch_reboot(int type)
{
    (void)type;

    __HAL_PMU_AON_SOFTWARE_RESET_FULL_CHIP();

    for (;;) {
        __WFI();
    }
}
