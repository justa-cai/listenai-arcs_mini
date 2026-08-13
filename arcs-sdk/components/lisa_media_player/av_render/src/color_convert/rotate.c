/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "rotate.h"

static void rotate_rgb565_90_cw(uint16_t *dst, const uint16_t *src, uint16_t src_w, uint16_t src_h)
{
    const uint32_t dst_w = (uint32_t)src_h;
    for (uint16_t y = 0; y < src_h; y++) {
        const uint16_t *src_row = src + (uint32_t)y * (uint32_t)src_w;

        /* For CW: dst[y2=x][x2=src_h-1-y] = src[y][x]
         * Fix x2, step dst by one row each x.
         */
        uint16_t *d = dst + (uint32_t)(src_h - 1U - y);

        uint16_t x = 0;
        for (; (uint32_t)x + 3U < (uint32_t)src_w; x = (uint16_t)(x + 4U)) {
            d[0 * dst_w] = src_row[x + 0];
            d[1 * dst_w] = src_row[x + 1];
            d[2 * dst_w] = src_row[x + 2];
            d[3 * dst_w] = src_row[x + 3];
            d += 4U * dst_w;
        }
        for (; x < src_w; x++) {
            *d = src_row[x];
            d += dst_w;
        }
    }
}

static void rotate_rgb565_90_ccw(uint16_t *dst, const uint16_t *src, uint16_t src_w, uint16_t src_h)
{
    const uint32_t dst_w = (uint32_t)src_h;
    for (uint16_t y = 0; y < src_h; y++) {
        const uint16_t *src_row = src + (uint32_t)y * (uint32_t)src_w;

        /* For CCW: dst[y2=src_w-1-x][x2=y] = src[y][x]
         * Fix x2, step dst backwards by one row each x.
         */
        uint16_t *d = dst + ((uint32_t)(src_w - 1U) * dst_w) + (uint32_t)y;

        uint16_t x = 0;
        for (; (uint32_t)x + 3U < (uint32_t)src_w; x = (uint16_t)(x + 4U)) {
            d[0] = src_row[x + 0];
            d -= dst_w;
            d[0] = src_row[x + 1];
            d -= dst_w;
            d[0] = src_row[x + 2];
            d -= dst_w;
            d[0] = src_row[x + 3];
            d -= dst_w;
        }
        for (; x < src_w; x++) {
            *d = src_row[x];
            d -= dst_w;
        }
    }
}

bool av_render_rotate_select(av_render_rotate_mode_t mode, uint16_t src_w, uint16_t src_h, uint16_t display_w,
                            uint16_t display_h, bool *out_ccw)
{
    if (out_ccw) {
        *out_ccw = false;
    }

    const uint16_t rotated_w = src_h;
    const uint16_t rotated_h = src_w;

    const bool can_rotate_to_display =
        (display_w && display_h && rotated_w == display_w && rotated_h == display_h);

    bool do_rotate = false;
    bool rotate_ccw = false;
    switch (mode) {
    case AV_RENDER_ROTATE_AUTO_90_CW:
        do_rotate = can_rotate_to_display;
        rotate_ccw = false;
        break;
    case AV_RENDER_ROTATE_AUTO_90_CCW:
        do_rotate = can_rotate_to_display;
        rotate_ccw = true;
        break;
    case AV_RENDER_ROTATE_FORCE_90_CW:
        do_rotate = can_rotate_to_display;
        rotate_ccw = false;
        break;
    case AV_RENDER_ROTATE_FORCE_90_CCW:
        do_rotate = can_rotate_to_display;
        rotate_ccw = true;
        break;
    case AV_RENDER_ROTATE_NONE:
    default:
        do_rotate = false;
        rotate_ccw = false;
        break;
    }

    if (do_rotate && out_ccw) {
        *out_ccw = rotate_ccw;
    }
    return do_rotate;
}

void av_render_rotate_rgb565_90(uint16_t *dst, const uint16_t *src, uint16_t src_w, uint16_t src_h, bool ccw)
{
    if (ccw) {
        rotate_rgb565_90_ccw(dst, src, src_w, src_h);
    } else {
        rotate_rgb565_90_cw(dst, src, src_w, src_h);
    }
}
