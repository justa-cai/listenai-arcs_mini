/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef CHERRYUSB_UAC_SPEAKER_H
#define CHERRYUSB_UAC_SPEAKER_H

#include "uac_stream.h"

int speaker_init(void);
uac_stream_t *speaker_stream(void);

#endif /* CHERRYUSB_UAC_SPEAKER_H */
