/**
 * @file bt_audio_stream_lifecycle.h
 * @brief Internal stream lifecycle guard for BT audio scenarios.
 *
 * This helper owns only generic stream state and session lifetime guarding.
 * Source/Sink code must keep profile-specific behavior outside this layer.
 */

#ifndef BT_AUDIO_STREAM_LIFECYCLE_H_
#define BT_AUDIO_STREAM_LIFECYCLE_H_

#include "bt_audio_session.h"
#include "bt_audio_types.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BT_AUDIO_STREAM_IDLE = 0,
    BT_AUDIO_STREAM_OPENING,
    BT_AUDIO_STREAM_OPENED,
    BT_AUDIO_STREAM_CLOSING,
} bt_audio_stream_state_t;

typedef enum {
    BT_AUDIO_STREAM_SESSION_UPLINK = 0,
    BT_AUDIO_STREAM_SESSION_DOWNLINK,
    BT_AUDIO_STREAM_SESSION_MAX,
} bt_audio_stream_session_slot_t;

typedef struct {
    SemaphoreHandle_t mutex;
    bt_audio_stream_state_t state;
    bool ready;
    uint32_t active_ops;
    bt_audio_session_handle_t sessions[BT_AUDIO_STREAM_SESSION_MAX];
} bt_audio_stream_lifecycle_t;

bt_audio_error_t bt_audio_stream_lifecycle_init(bt_audio_stream_lifecycle_t *lc);
void bt_audio_stream_lifecycle_deinit(bt_audio_stream_lifecycle_t *lc);
void bt_audio_stream_lifecycle_reset(bt_audio_stream_lifecycle_t *lc);
bool bt_audio_stream_lifecycle_lock(bt_audio_stream_lifecycle_t *lc);
void bt_audio_stream_lifecycle_unlock(bt_audio_stream_lifecycle_t *lc);
void bt_audio_stream_lifecycle_set_state(bt_audio_stream_lifecycle_t *lc,
                                         bt_audio_stream_state_t state,
                                         bool ready);
bt_audio_stream_state_t bt_audio_stream_lifecycle_get_state(bt_audio_stream_lifecycle_t *lc);
void bt_audio_stream_lifecycle_set_session(bt_audio_stream_lifecycle_t *lc,
                                           bt_audio_stream_session_slot_t slot,
                                           bt_audio_session_handle_t session);
bt_audio_session_handle_t bt_audio_stream_lifecycle_get_session(bt_audio_stream_lifecycle_t *lc,
                                                                 bt_audio_stream_session_slot_t slot);
bool bt_audio_stream_lifecycle_begin(bt_audio_stream_lifecycle_t *lc,
                                     bt_audio_stream_session_slot_t slot,
                                     bt_audio_session_handle_t *session);
void bt_audio_stream_lifecycle_end(bt_audio_stream_lifecycle_t *lc);
void bt_audio_stream_lifecycle_wait_idle(bt_audio_stream_lifecycle_t *lc);

#ifdef __cplusplus
}
#endif

#endif /* BT_AUDIO_STREAM_LIFECYCLE_H_ */
