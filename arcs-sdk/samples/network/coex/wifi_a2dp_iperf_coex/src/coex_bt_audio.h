/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef COEX_BT_AUDIO_H
#define COEX_BT_AUDIO_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool initialized;
    bool profile_open;
    bool streaming;
    uint8_t volume;
} coex_bt_audio_status_t;

int coex_bt_audio_init(void);
int coex_bt_audio_start(void);
int coex_bt_audio_stop(void);
int coex_bt_audio_set_volume(uint8_t volume);
void coex_bt_audio_get_status(coex_bt_audio_status_t *status);

#endif /* COEX_BT_AUDIO_H */
