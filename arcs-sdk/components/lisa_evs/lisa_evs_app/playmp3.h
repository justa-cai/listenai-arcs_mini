/*
 * Copyright (c) 2023, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif
#include "stdint.h"

typedef struct {
    void (*lisa_play_start)(void);
    void (*lisa_play_stop)(void);
} lisa_mp3_cbs_t;

void mp3_play_init(lisa_mp3_cbs_t *cbs);
void mp3_play_start(void);
void lisa_mp3_send(const void *data, uint32_t len);
void mp3_play_stop(void);

#ifdef __cplusplus
}
#endif

