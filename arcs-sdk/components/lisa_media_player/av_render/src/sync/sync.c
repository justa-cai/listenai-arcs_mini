/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../av_render_internal.h"

#include <lisa_time.h>

void av_render_sync_on_audio_frame(av_render_t *render, uint32_t audio_pts_ms)
{
    if (!render) {
        return;
    }

    if (render->audio_start_tick_ms == 0) {
        uint64_t now = lisa_os_get_tick_ms();
        render->audio_start_tick_ms = now - (uint64_t)audio_pts_ms;
    }
}

void av_render_sync_wait_video(av_render_t *render, uint32_t video_pts_ms)
{
    if (!render) {
        return;
    }

    if (render->cfg.sync_mode != AV_RENDER_SYNC_AUDIO) {
        return;
    }

    if (!render->audio_started) {
        return;
    }

    uint64_t now = lisa_os_get_tick_ms();
    uint64_t should_play_at = render->audio_start_tick_ms + (uint64_t)video_pts_ms;
    if (now + 2 < should_play_at) {
        lisa_thread_mdelay((uint32_t)(should_play_at - now));
    }
}
