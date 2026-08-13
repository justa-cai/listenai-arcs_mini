/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef UAC_SPEAKER_H
#define UAC_SPEAKER_H

#include "uac_device.h"

int speaker_init(void);
uac_stream_t *speaker_stream(void);

#endif /* UAC_SPEAKER_H */
