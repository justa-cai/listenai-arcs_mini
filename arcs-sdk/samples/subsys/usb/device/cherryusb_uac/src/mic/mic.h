/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef CHERRYUSB_UAC_MIC_H
#define CHERRYUSB_UAC_MIC_H

#include "lisa_audio.h"
#include "uac_stream.h"

int mic_init(lisa_device_t *audio_dev);
uac_stream_t *mic_stream(void);

#endif /* CHERRYUSB_UAC_MIC_H */
