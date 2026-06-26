#pragma once

#include "voice_msg_structure.h"

struct model_sd_music_sync_cb {
    void (*on_sd_music_sync_state_change)(const voice_msg_sd_music_sync_state_t *state,
                                           void *arg);
    void (*on_sd_music_card_removed)(void *arg);
    void (*on_sd_music_play_failed)(void *arg);
};

int model_sd_music_sync_init(void);
int model_sd_music_sync_cb_register(const struct model_sd_music_sync_cb *cb, void *arg);
int model_sd_music_sync_cb_unregister(const struct model_sd_music_sync_cb *cb);
int model_sd_music_sync_get_state_snapshot(voice_msg_sd_music_sync_state_t *state);
