#ifndef __VOICE_CAMERA_PREVIEW_STATE_H__
#define __VOICE_CAMERA_PREVIEW_STATE_H__

#include <stdbool.h>
#include <stdint.h>

#include "voice_msg_structure.h"

static inline bool voice_camera_preview_state_parse(voice_msg_camera_preview_state_t *state_out,
                                                    const void *data, uint32_t len)
{
    if (!state_out || !data || len < sizeof(*state_out)) {
        return false;
    }

    *state_out = *(const voice_msg_camera_preview_state_t *)data;
    return true;
}

static inline bool
voice_camera_preview_state_is_mcp_mode(const voice_msg_camera_preview_state_t *state)
{
    return state && state->mode == VOICE_MSG_CAMERA_PREVIEW_MODE_MCP_PHOTO;
}

static inline bool
voice_camera_preview_state_is_button_mode(const voice_msg_camera_preview_state_t *state)
{
    return state && state->mode == VOICE_MSG_CAMERA_PREVIEW_MODE_BUTTON_PHOTO;
}

static inline bool voice_camera_preview_state_is_active(const voice_msg_camera_preview_state_t *state)
{
    return voice_camera_preview_state_is_mcp_mode(state) &&
           state->phase != VOICE_MSG_CAMERA_FLOW_PHASE_NONE;
}

static inline bool voice_camera_preview_state_is_locked(const voice_msg_camera_preview_state_t *state)
{
    return voice_camera_preview_state_is_mcp_mode(state) &&
           (state->phase == VOICE_MSG_CAMERA_FLOW_PHASE_PREVIEW ||
            state->phase == VOICE_MSG_CAMERA_FLOW_PHASE_PROCESSING);
}

static inline bool
voice_camera_preview_state_is_result_tts_active(const voice_msg_camera_preview_state_t *state)
{
    return voice_camera_preview_state_is_mcp_mode(state) &&
           state->phase == VOICE_MSG_CAMERA_FLOW_PHASE_RESULT_TTS;
}

static inline bool
voice_camera_preview_state_is_button_preview_active(const voice_msg_camera_preview_state_t *state)
{
    return voice_camera_preview_state_is_button_mode(state) && state->active;
}

#endif // __VOICE_CAMERA_PREVIEW_STATE_H__
