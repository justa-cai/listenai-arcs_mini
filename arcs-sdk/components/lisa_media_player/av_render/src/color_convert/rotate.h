/*
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "av_render.h"

bool av_render_rotate_select(av_render_rotate_mode_t mode, uint16_t src_w, uint16_t src_h, uint16_t display_w,
                            uint16_t display_h, bool *out_ccw);

void av_render_rotate_rgb565_90(uint16_t *dst, const uint16_t *src, uint16_t src_w, uint16_t src_h, bool ccw);
