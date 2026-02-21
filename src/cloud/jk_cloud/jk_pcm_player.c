#define TAG "jk_pcm_player"

#include "jk_pcm_player.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "pa_manager.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

static jk_pcm_player_t *s_pcm_player = NULL;

// ============================================================================
// Singleton PCM Player Design
// ============================================================================
// The player is created once and keeps running in singleton mode.
//
// State Machine:
//   - is_playing:    Controls whether data is sent to underlying lisa_player
//   - is_prepared:   Controls if lisa_player has been configured (monotonic)
//   - is_preparing:  Tracks if lisa_player is currently preparing
//
// Key Design Decisions:
//   1. No EOF sent in singleton mode - keeps stream open for next TTS
//   2. Only is_playing flag controls data flow, player stays ready
//   3. TTS channel acquired once, released only on error
//   4. State checked and reset before new operations to handle edge cases
// ============================================================================

// Forward declarations
static int _pcm_player_callback(PlayerEvt evt, int arg1, int arg2, int id);
static int _ensure_player_ready(jk_pcm_player_t *player);
static int _recover_player_handle(jk_pcm_player_t *player);
static bool _is_player_in_terminal_state(jk_pcm_player_t *player);
static const char *_player_evt_to_string(PlayerEvt evt);
static const char *_player_state_to_string(PlayerState state);

static int _pcm_player_callback(PlayerEvt evt, int arg1, int arg2, int id) {
    if (!s_pcm_player) {
        LISA_LOGW(TAG, "Callback invoked but player is NULL");
        return 0;
    }

    // Clear preparing state for all events
    s_pcm_player->is_preparing = false;

    LISA_LOGI(TAG, "PCM player evt: %s (is_playing=%d, is_prepared=%d)",
              _player_evt_to_string(evt),
              s_pcm_player->is_playing,
              s_pcm_player->is_prepared);

    switch (evt) {
    case PLAYER_EVT_PREPARED:
        s_pcm_player->is_prepared = true;
        // Small delay to ensure player is fully ready
        vTaskDelay(pdMS_TO_TICKS(10));
        lisa_player_play(s_pcm_player->player);
        LISA_LOGI(TAG, "PCM player prepared and started");
        break;

    case PLAYER_EVT_PLAYING:
        LISA_LOGD(TAG, "PCM player playing event");
        if (s_pcm_player->cbs.on_play_start && s_pcm_player->total_written > 0) {
            s_pcm_player->cbs.on_play_start(s_pcm_player);
        }
        break;

    case PLAYER_EVT_PLAYBACK_COMPLETE:
        LISA_LOGI(TAG, "PCM player playback complete (total_written=%u bytes)",
                  s_pcm_player->total_written);

        // Singleton mode: keep player ready, only stop accepting new data
        s_pcm_player->is_playing = false;
        // Keep is_prepared=true - player stays ready for next TTS
        // Keep TTS channel acquired - no need to release/reacquire

        if (s_pcm_player->total_written > 0) {
            pa_manager_refresh(PA_MGR_OFF, LS_PA_BASE_TIME, "pcm_player_end");
            if (s_pcm_player->cbs.on_play_complete) {
                s_pcm_player->cbs.on_play_complete(s_pcm_player);
            }
            LISA_LOGI(TAG, "PCM complete: state ready (is_playing=false, is_prepared=true)");
        } else {
            LISA_LOGW(TAG, "PCM playback complete with no data (spurious completion)");
            s_pcm_player->total_written = 0;
        }
        break;

    case PLAYER_EVT_STOPED:
        // Should NOT happen in singleton mode - we don't call lisa_player_stop
        LISA_LOGW(TAG, "PCM player STOPPED event (unexpected in singleton mode)");
        s_pcm_player->is_playing = false;
        // Keep is_prepared=true - player stays ready
        break;

    case PLAYER_EVT_ERROR:
        LISA_LOGE(TAG, "PCM player error occurred");

        // Error state: need full reset and re-prepare next time
        s_pcm_player->is_playing = false;
        s_pcm_player->is_prepared = false;
        s_pcm_player->is_preparing = false;
        s_pcm_player->total_written = 0;

        // Release resources on error
        pa_manager_refresh(PA_MGR_OFF, LS_PA_BASE_TIME, "pcm_player_error");
        listen_audiomgr_release_channel(s_pcm_player->audio_mgr, TTS);

        // Reset the underlying player for recovery
        lisa_player_reset(s_pcm_player->player);

        if (s_pcm_player->cbs.on_error) {
            s_pcm_player->cbs.on_error(s_pcm_player, "Playback error");
        }
        break;

    case PLAYER_EVT_PAUSED:
        LISA_LOGD(TAG, "PCM player paused event");
        break;

    default:
        LISA_LOGD(TAG, "PCM player unhandled event: %d", evt);
        break;
    }

    return 0;
}

// ============================================================================
// Helper Functions
// ============================================================================

static const char *_player_evt_to_string(PlayerEvt evt) {
    switch (evt) {
    case PLAYER_EVT_PREPARED:          return "PREPARED";
    case PLAYER_EVT_PLAYING:           return "PLAYING";
    case PLAYER_EVT_PLAYBACK_COMPLETE: return "PLAYBACK_COMPLETE";
    case PLAYER_EVT_STOPED:            return "STOPED";
    case PLAYER_EVT_ERROR:             return "ERROR";
    case PLAYER_EVT_PAUSED:            return "PAUSED";
    default:                           return "UNKNOWN";
    }
}

static bool _is_player_in_terminal_state(jk_pcm_player_t *player) {
    if (!player || !player->player) return false;

    PlayerState state = lisa_player_get_state(player->player);
    return (state == PLAYER_ST_PLAYBACK_COMPLETE ||
            state == PLAYER_ST_STOPED ||
            state == PLAYER_ST_ERROR);
}

static int _ensure_player_ready(jk_pcm_player_t *player) {
    if (!player || !player->player) {
        LISA_LOGE(TAG, "Invalid player handle");
        return -1;
    }

    // Check if player is in terminal state and needs reset
    PlayerState state = lisa_player_get_state(player->player);
    LISA_LOGD(TAG, "Current lisa_player state: %s",
              _player_state_to_string(state));

    if (state == PLAYER_ST_PLAYBACK_COMPLETE ||
        state == PLAYER_ST_STOPED ||
        state == PLAYER_ST_ERROR) {
        LISA_LOGI(TAG, "Player in terminal state (%s), resetting",
                  _player_state_to_string(state));
        int ret = lisa_player_reset(player->player);
        if (ret != PLAYER_OK) {
            LISA_LOGE(TAG, "Failed to reset player: ret=%d", ret);
            return -1;
        }
    }

    return 0;
}

static const char *_player_state_to_string(PlayerState state) {
    switch (state) {
    case PLAYER_ST_NONE:               return "NONE";
    case PLAYER_ST_PREPARED:           return "PREPARED";
    case PLAYER_ST_READY_TO_PLAY:      return "READY_TO_PLAY";
    case PLAYER_ST_PLAYING:            return "PLAYING";
    case PLAYER_ST_PAUSED:             return "PAUSED";
    case PLAYER_ST_STOPED:             return "STOPED";
    case PLAYER_ST_PLAYBACK_COMPLETE:  return "PLAYBACK_COMPLETE";
    case PLAYER_ST_ERROR:              return "ERROR";
    default:                           return "UNKNOWN";
    }
}

// ============================================================================
// Recovery Functions
// ============================================================================

// Recovery mechanism: Recreate the player handle when it becomes NULL or invalid
// This can happen due to internal lisa_player state corruption or external resets
static int _recover_player_handle(jk_pcm_player_t *player) {
    if (!player) {
        LISA_LOGE(TAG, "Cannot recover NULL player");
        return -1;
    }

    LISA_LOGW(TAG, "Attempting to recover player handle (is_prepared=%d, is_playing=%d)",
              player->is_prepared, player->is_playing);

    // Release resources if they were acquired
    if (player->is_prepared) {
        listen_audiomgr_release_channel(player->audio_mgr, TTS);
        pa_manager_refresh(PA_MGR_OFF, LS_PA_FOREVER, "pcm_player_recover");
    }

    // Destroy old handle if it exists (but might be invalid)
    if (player->player) {
        LISA_LOGW(TAG, "Destroying old player handle");
        lisa_player_stop_sync(player->player);
        lisa_player_destory(player->player);
        player->player = NULL;
    }

    // Create new player handle
    player->player = lisa_player_create("pcm_player", 0);
    if (!player->player) {
        LISA_LOGE(TAG, "Failed to recreate player handle");
        return -1;
    }

    // Reconfigure the new player
    lisa_player_set_callback(player->player, _pcm_player_callback);
    lisa_player_set_vol(player->player, 60);

    // Reset state - will need to re-prepare
    player->is_prepared = false;
    player->is_preparing = false;
    player->total_written = 0;

    LISA_LOGI(TAG, "Player handle recovered successfully");
    return 0;
}

// ============================================================================
// Public API Implementation
// ============================================================================

jk_pcm_player_t *jk_pcm_player_create(listen_audiomgr_t *audio_mgr,
                                         jk_pcm_player_callbacks_t *cbs) {
    if (!audio_mgr) {
        LISA_LOGE(TAG, "audio_mgr is NULL");
        return NULL;
    }

    if (s_pcm_player) {
        // Check if the existing instance is still valid
        if (!s_pcm_player->player) {
            LISA_LOGW(TAG, "PCM player exists but handle is NULL, creating new instance");
            s_pcm_player = NULL;
        } else {
            LISA_LOGI(TAG, "PCM player already exists (singleton), returning existing instance");
            return s_pcm_player;
        }
    }

    jk_pcm_player_t *player = lisa_mem_calloc(1, sizeof(jk_pcm_player_t));
    if (!player) {
        LISA_LOGE(TAG, "Failed to allocate player");
        return NULL;
    }

    player->player = lisa_player_create("pcm_player", 0);
    if (!player->player) {
        LISA_LOGE(TAG, "Failed to create lisa player");
        lisa_mem_free(player);
        return NULL;
    }

    lisa_player_set_callback(player->player, _pcm_player_callback);

    // Set volume to a reasonable default (0-100 range)
    lisa_player_set_vol(player->player, 60);
    LISA_LOGI(TAG, "PCM player volume set to 60");

    player->audio_mgr = audio_mgr;
    player->is_playing = false;
    player->is_prepared = false;
    player->is_preparing = false;
    player->total_written = 0;

    if (cbs) {
        memcpy(&player->cbs, cbs, sizeof(jk_pcm_player_callbacks_t));
    }

    s_pcm_player = player;
    LISA_LOGI(TAG, "PCM player created (singleton)");
    return player;
}

void jk_pcm_player_destroy(jk_pcm_player_t *player) {
    if (!player) {
        LISA_LOGW(TAG, "Cannot destroy NULL player");
        return;
    }

    LISA_LOGI(TAG, "Destroying PCM player");

    // Release TTS channel if acquired
    if (player->is_prepared) {
        listen_audiomgr_release_channel(player->audio_mgr, TTS);
        pa_manager_refresh(PA_MGR_OFF, LS_PA_BASE_TIME, "pcm_player_destroy");
    }

    // Stop and destroy underlying player
    if (player->player) {
        lisa_player_stop_sync(player->player);
        lisa_player_destory(player->player);
        player->player = NULL;
    }

    if (s_pcm_player == player) {
        s_pcm_player = NULL;
    }

    lisa_mem_free(player);
    LISA_LOGI(TAG, "PCM player destroyed");
}

int jk_pcm_player_write(jk_pcm_player_t *player,
                         const int16_t *samples,
                         uint32_t count) {
    if (!player) {
        LISA_LOGE(TAG, "Invalid player");
        return -1;
    }

    if (!samples || count == 0) {
        LISA_LOGW(TAG, "Invalid samples: samples=%p, count=%u", samples, count);
        return -1;
    }

    // STATE MACHINE: Only write if is_playing is true
    // If is_playing is false, silently discard data (singleton: player stays ready)
    if (!player->is_playing) {
        LISA_LOGD(TAG, "Not playing (is_playing=false), discarding %u samples", count);
        return 0;  // Return success - data discarded gracefully
    }

    // ============================================================================
    // Recovery Mechanism: Handle is NULL but is_prepared=true
    // This can happen when lisa_player is reset/destroyed externally
    // ============================================================================
    if (!player->player) {
        LISA_LOGW(TAG, "Player handle is NULL! is_prepared=%d, is_playing=%d - triggering recovery",
                  player->is_prepared, player->is_playing);

        if (_recover_player_handle(player) != 0) {
            LISA_LOGE(TAG, "Recovery failed");
            player->is_playing = false;
            return -1;
        }

        // After recovery, player will need to re-prepare
        // Fall through to preparation logic below
    }

    // Prepare player if not already prepared (or after recovery)
    // ALSO: If is_prepared=true but player is in terminal state, need to re-prepare
    bool need_prepare = false;

    if (!player->is_prepared && !player->is_preparing) {
        LISA_LOGI(TAG, "PCM player not prepared, preparing now...");
        need_prepare = true;
    } else if (player->is_prepared && !player->is_preparing && player->player) {
        // Check if underlying player is in terminal state even though is_prepared=true
        PlayerState state = lisa_player_get_state(player->player);
        if (state == PLAYER_ST_PLAYBACK_COMPLETE ||
            state == PLAYER_ST_STOPED ||
            state == PLAYER_ST_ERROR ||
            state == PLAYER_ST_NONE) {
            LISA_LOGW(TAG, "Player is_prepared=true but in terminal state (%s), forcing re-prepare",
                      _player_state_to_string(state));
            need_prepare = true;
            // Reset is_prepared to trigger re-preparation
            player->is_prepared = false;
        }
    }

    if (need_prepare) {
        // Ensure player is in valid state before preparing
        if (_ensure_player_ready(player) != 0) {
            LISA_LOGE(TAG, "Failed to ensure player ready");
            player->is_playing = false;
            return -1;
        }

        // Set PCM stream URL (16kHz, mono, 16-bit)
        int ret = lisa_player_seturl(player->player,
                                    "stream://type=pcm&rate=16000&channel=1&bits=16");
        if (ret != PLAYER_OK) {
            LISA_LOGE(TAG, "Failed to set stream URL: ret=%d", ret);
            player->is_playing = false;
            player->is_preparing = false;
            return -1;
        }

        player->is_preparing = true;
        player->total_written = 0;

        pa_manager_refresh(PA_MGR_ON, LS_PA_FOREVER, "pcm_player_start");
        listen_audiomgr_acquire_channel(player->audio_mgr, TTS);
        LISA_LOGI(TAG, "PCM player preparing, TTS channel acquired");

        // Continue to write first data frame - this helps trigger the PREPARED event
        // Don't return here, fall through to write the data
    }

    // Write PCM data to lisa_player stream
    // IMPORTANT: Write data even during preparation to trigger PREPARED event
    // The lisa_player needs data to complete the preparation process

    uint32_t size = count * sizeof(int16_t);
    int ret = lisa_player_put_stream_data(player->player,
                                          (uint8_t *)samples,
                                          size,
                                          1000);
    if (ret < 0) {
        // Write failed - check if handle became NULL during write
        if (!player->player) {
            LISA_LOGE(TAG, "Player handle became NULL during write! Attempting recovery...");
            if (_recover_player_handle(player) == 0) {
                // Recovery successful - tell caller to retry
                LISA_LOGI(TAG, "Recovery successful, data not written yet - caller should retry");
                return -2;  // Special return code: retry recommended
            }
            player->is_playing = false;
            return -1;
        }

        // If write fails during preparation, it might be expected
        // Only log as error if we're already prepared
        if (player->is_prepared) {
            LISA_LOGE(TAG, "Failed to write PCM data: ret=%d (samples=%u, size=%u)",
                      ret, count, size);
            return -1;
        } else {
            LISA_LOGI(TAG, "Write during preparation failed: ret=%d (will retry)", ret);
            // Don't fail the write during preparation - just discard and continue
            return 0;
        }
    }

    player->total_written += size;

    // Log first few writes for debugging
    static int write_count = 0;
    if (write_count++ < 5) {
        LISA_LOGI(TAG, "PCM write: %u samples, %u bytes, total: %u (preparing=%d, prepared=%d)",
                  count, size, player->total_written, player->is_preparing, player->is_prepared);
    }

    return 0;
}

int jk_pcm_player_start(jk_pcm_player_t *player) {
    if (!player) {
        LISA_LOGE(TAG, "Cannot start NULL player");
        return -1;
    }

    LISA_LOGI(TAG, "PCM player start: is_playing=true (prepared=%d, preparing=%d)",
              player->is_prepared, player->is_preparing);
    player->is_playing = true;
    // Don't reset is_prepared - keep player ready for next write
    return 0;
}

int jk_pcm_player_stop(jk_pcm_player_t *player) {
    if (!player) {
        LISA_LOGE(TAG, "Cannot stop NULL player");
        return -1;
    }

    if (player->is_playing) {
        LISA_LOGI(TAG, "PCM player stop: is_playing=false (singleton mode)");
        player->is_playing = false;
        // CRITICAL: DON'T send EOF to lisa_player in singleton mode!
        // Sending EOF (NULL, 0) closes the stream and invalidates the handle
        // We just stop accepting new data, but keep the stream open for next TTS
        // The lisa_player will continue consuming any buffered data
    }

    return 0;
}

int jk_pcm_player_end_stream(jk_pcm_player_t *player) {
    if (!player || !player->player) {
        LISA_LOGE(TAG, "Cannot end stream: invalid player");
        return -1;
    }

    LISA_LOGD(TAG, "End stream called (singleton mode: just stopping, not sending EOF)");
    // In singleton mode, we don't send EOF to keep the player ready
    // Just stop accepting new data - let the player finish what it has
    return 0;
}

void jk_pcm_player_reset(jk_pcm_player_t *player) {
    if (!player) {
        LISA_LOGW(TAG, "Cannot reset NULL player");
        return;
    }

    LISA_LOGI(TAG, "PCM player reset: clearing state");
    player->is_playing = false;
    player->is_preparing = false;
    player->total_written = 0;
    // Note: is_prepared is NOT reset - player stays ready
}

bool jk_pcm_player_is_playing(jk_pcm_player_t *player) {
    return player && player->is_playing;
}

bool jk_pcm_player_is_prepared(jk_pcm_player_t *player) {
    return player && player->is_prepared;
}

bool jk_pcm_player_is_preparing(jk_pcm_player_t *player) {
    return player && player->is_preparing;
}

uint32_t jk_pcm_player_get_buffered(jk_pcm_player_t *player) {
    return player ? player->total_written : 0;
}
