/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef UAC_MIC_H
#define UAC_MIC_H

#include "lisa_audio.h"
#include "uac_device.h"

int mic_init(lisa_device_t *audio_dev);
uac_stream_t *mic_stream(void);

#endif /* UAC_MIC_H */
