/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef UI_PLAYBACK_H
#define UI_PLAYBACK_H

#include <stdbool.h>

void ui_playback_create(const char *path_or_url, bool is_url);
void ui_playback_destroy(void);

#endif /* UI_PLAYBACK_H */
