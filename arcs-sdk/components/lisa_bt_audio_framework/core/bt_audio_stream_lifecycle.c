/**
 * @file bt_audio_stream_lifecycle.c
 * @brief Internal stream lifecycle guard for BT audio scenarios.
 */

#include "bt_audio_stream_lifecycle.h"
#include "task.h"
#include <string.h>

static bool bt_audio_stream_lifecycle_valid_slot(bt_audio_stream_session_slot_t slot)
{
    return slot >= BT_AUDIO_STREAM_SESSION_UPLINK && slot < BT_AUDIO_STREAM_SESSION_MAX;
}

bt_audio_error_t bt_audio_stream_lifecycle_init(bt_audio_stream_lifecycle_t *lc)
{
    if (!lc) {
        return BT_AUDIO_ERR_INVALID_PARAM;
    }

    memset(lc, 0, sizeof(*lc));
    lc->mutex = xSemaphoreCreateMutex();
    if (!lc->mutex) {
        return BT_AUDIO_ERR_NO_MEMORY;
    }

    lc->state = BT_AUDIO_STREAM_IDLE;
    return BT_AUDIO_OK;
}

void bt_audio_stream_lifecycle_deinit(bt_audio_stream_lifecycle_t *lc)
{
    SemaphoreHandle_t mutex;

    if (!lc) {
        return;
    }

    mutex = lc->mutex;
    if (mutex) {
        vSemaphoreDelete(mutex);
    }
    memset(lc, 0, sizeof(*lc));
}

void bt_audio_stream_lifecycle_reset(bt_audio_stream_lifecycle_t *lc)
{
    SemaphoreHandle_t mutex;

    if (!lc) {
        return;
    }

    mutex = lc->mutex;
    memset(lc, 0, sizeof(*lc));
    lc->mutex = mutex;
    lc->state = BT_AUDIO_STREAM_IDLE;
}

bool bt_audio_stream_lifecycle_lock(bt_audio_stream_lifecycle_t *lc)
{
    return lc && lc->mutex && xSemaphoreTake(lc->mutex, portMAX_DELAY) == pdTRUE;
}

void bt_audio_stream_lifecycle_unlock(bt_audio_stream_lifecycle_t *lc)
{
    if (lc && lc->mutex) {
        xSemaphoreGive(lc->mutex);
    }
}

void bt_audio_stream_lifecycle_set_state(bt_audio_stream_lifecycle_t *lc,
                                         bt_audio_stream_state_t state,
                                         bool ready)
{
    if (!bt_audio_stream_lifecycle_lock(lc)) {
        return;
    }

    lc->state = state;
    lc->ready = ready;
    bt_audio_stream_lifecycle_unlock(lc);
}

bt_audio_stream_state_t bt_audio_stream_lifecycle_get_state(bt_audio_stream_lifecycle_t *lc)
{
    bt_audio_stream_state_t state = BT_AUDIO_STREAM_IDLE;

    if (!bt_audio_stream_lifecycle_lock(lc)) {
        return state;
    }

    state = lc->state;
    bt_audio_stream_lifecycle_unlock(lc);
    return state;
}

void bt_audio_stream_lifecycle_set_session(bt_audio_stream_lifecycle_t *lc,
                                           bt_audio_stream_session_slot_t slot,
                                           bt_audio_session_handle_t session)
{
    if (!bt_audio_stream_lifecycle_valid_slot(slot) || !bt_audio_stream_lifecycle_lock(lc)) {
        return;
    }

    lc->sessions[slot] = session;
    bt_audio_stream_lifecycle_unlock(lc);
}

bt_audio_session_handle_t bt_audio_stream_lifecycle_get_session(bt_audio_stream_lifecycle_t *lc,
                                                                 bt_audio_stream_session_slot_t slot)
{
    bt_audio_session_handle_t session = NULL;

    if (!bt_audio_stream_lifecycle_valid_slot(slot) || !bt_audio_stream_lifecycle_lock(lc)) {
        return NULL;
    }

    session = lc->sessions[slot];
    bt_audio_stream_lifecycle_unlock(lc);
    return session;
}

bool bt_audio_stream_lifecycle_begin(bt_audio_stream_lifecycle_t *lc,
                                     bt_audio_stream_session_slot_t slot,
                                     bt_audio_session_handle_t *session)
{
    bool ok = false;

    if (!session || !bt_audio_stream_lifecycle_valid_slot(slot) ||
        !bt_audio_stream_lifecycle_lock(lc)) {
        return false;
    }

    if (lc->ready && lc->state == BT_AUDIO_STREAM_OPENED && lc->sessions[slot]) {
        *session = lc->sessions[slot];
        lc->active_ops++;
        ok = true;
    }

    bt_audio_stream_lifecycle_unlock(lc);
    return ok;
}

void bt_audio_stream_lifecycle_end(bt_audio_stream_lifecycle_t *lc)
{
    if (!bt_audio_stream_lifecycle_lock(lc)) {
        return;
    }

    if (lc->active_ops > 0) {
        lc->active_ops--;
    }
    bt_audio_stream_lifecycle_unlock(lc);
}

void bt_audio_stream_lifecycle_wait_idle(bt_audio_stream_lifecycle_t *lc)
{
    TickType_t delay_ticks = pdMS_TO_TICKS(1);

    if (delay_ticks == 0) {
        delay_ticks = 1;
    }

    while (true) {
        uint32_t active_ops = 0;

        if (!bt_audio_stream_lifecycle_lock(lc)) {
            return;
        }
        active_ops = lc->active_ops;
        bt_audio_stream_lifecycle_unlock(lc);

        if (active_ops == 0) {
            return;
        }
        vTaskDelay(delay_ticks);
    }
}
