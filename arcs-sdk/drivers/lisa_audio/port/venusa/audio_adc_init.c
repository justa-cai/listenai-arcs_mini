/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "audio_adc_init.h"
#include "board.h"
#include "PowerManager.h"

static void audio_adc_set_ldova(void)
{
    IP_AON_CTRL->REG_AON_TUNE0.bit.SEL_VOUT_LDOVA = 0;
    IP_AON_CTRL->REG_AON_TUNE0.bit.TUNE_LDOVA = CONFIG_LISA_AUDIO_VENUSA_LDOVA_TUNE;
}

int audio_adc_gpio_init(void)
{
#if CONFIG_LISA_AUDIO_RECORD_USE_DMIC
    lisa_dmic_pinmux();
#else
    lisa_capture_pinmux();
#endif
    return 0;
}

int audio_adc_platform_init(void)
{
    audio_adc_gpio_init();
    audio_adc_set_ldova();
    return 0;
}
