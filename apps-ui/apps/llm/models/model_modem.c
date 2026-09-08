#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define TAG "model_modem"

#include "model_modem.h"

#include "lisa_ui_log.h"

#ifdef LISA_UI_PLATFORM_ARCS
#include "lisa_ui_invoke.h"
#include "async_task.h"
#include "FreeRTOS.h"
#include "task.h"
#include "sys_network_manager.h"
#include "voice_msg.h"
#include "voice_player_comm.h"
#include "model_voice.h"
#endif

#define MODEL_MODEM_SIGNAL_RSSI_UNKNOWN 99
#define MODEL_MODEM_SIGNAL_BER_UNKNOWN  99
#define MODEL_MODEM_SIGNAL_REFRESH_MS   3000U

typedef struct {
    uint32_t seq;
    bool ok;
    int rssi;
    int ber;
    uint32_t elapsed_ms;
} model_modem_signal_result_t;

struct model_modem_context {
    uint8_t inited;
    uint8_t signal_query_pending;
    uint32_t signal_query_seq;
    model_modem_info_t info;
#ifdef LISA_UI_PLATFORM_ARCS
    TickType_t last_signal_query_tick;
    model_modem_signal_result_t signal_result;
#endif
};

static struct model_modem_context model_modem_ctx = {
    .inited = 0,
    .signal_query_pending = 0,
    .signal_query_seq = 0,
    .info = {
        .is_modem_mode = false,
        .active = false,
        .connected = false,
        .switching = false,
        .signal_rssi = MODEL_MODEM_SIGNAL_RSSI_UNKNOWN,
        .signal_ber = MODEL_MODEM_SIGNAL_BER_UNKNOWN,
        .signal_level = 0,
    },
#ifdef LISA_UI_PLATFORM_ARCS
    .last_signal_query_tick = 0,
#endif
};

#ifdef LISA_UI_PLATFORM_ARCS

static uint8_t model_modem_csq_to_level(int rssi)
{
    if (rssi == MODEL_MODEM_SIGNAL_RSSI_UNKNOWN || rssi < 0) {
        return 0;
    }

    if (rssi > 31) {
        rssi = 31;
    }

    return (uint8_t)(((rssi * 6) / 31) + 1);
}

static void model_modem_reset_signal(void)
{
    if (model_modem_ctx.signal_query_pending) {
        model_modem_ctx.signal_query_pending = 0;
        model_modem_ctx.signal_query_seq++;
    }

    model_modem_ctx.last_signal_query_tick = 0;
    model_modem_ctx.info.signal_rssi = MODEL_MODEM_SIGNAL_RSSI_UNKNOWN;
    model_modem_ctx.info.signal_ber = MODEL_MODEM_SIGNAL_BER_UNKNOWN;
    model_modem_ctx.info.signal_level = 0;
}

static void model_modem_clear_signal_result(void)
{
    model_modem_ctx.info.signal_rssi = MODEL_MODEM_SIGNAL_RSSI_UNKNOWN;
    model_modem_ctx.info.signal_ber = MODEL_MODEM_SIGNAL_BER_UNKNOWN;
    model_modem_ctx.info.signal_level = 0;
}

static void model_modem_sync_from_system(void)
{
    bool prev_active = model_modem_ctx.info.active;
    bool prev_connected = model_modem_ctx.info.connected;
    bool prev_switching = model_modem_ctx.info.switching;
    sys_network_status_t status;

    if (sys_network_get_status(&status) != 0) {
        model_modem_reset_signal();
        return;
    }

    model_modem_ctx.info.is_modem_mode = (status.mode == SYS_NETWORK_MODE_MODEM);
    model_modem_ctx.info.active = (status.active_bearer == SYS_NETWORK_BEARER_MODEM);
    model_modem_ctx.info.switching = status.switching;
    model_modem_ctx.info.connected = model_modem_ctx.info.active && status.connected;

    if (!model_modem_ctx.info.active ||
        !model_modem_ctx.info.connected ||
        model_modem_ctx.info.switching) {
        model_modem_reset_signal();
        return;
    }

    if (!prev_active ||
        !prev_connected ||
        prev_switching ||
        prev_active != model_modem_ctx.info.active ||
        prev_connected != model_modem_ctx.info.connected ||
        prev_switching != model_modem_ctx.info.switching) {
        model_modem_ctx.last_signal_query_tick = 0;
    }
}

static void model_modem_state_msg_handle(void *unused, uint32_t msg_id, void *data, uint32_t len, void *user_data)
{
    (void)unused;
    (void)msg_id;
    (void)data;
    (void)len;
    (void)user_data;

    LISA_UI_INVOKE_UI_ARG_NONE({
        model_modem_sync_from_system();
    });
}

/* The modem API is synchronous and may wait for the UART/dispatcher. Keep it
 * off the voice event bus so a slow CSQ query cannot stall audio upload. */
static void model_modem_signal_query_task(void *user_data, bool *should_stop)
{
    model_modem_signal_result_t *result = user_data;
    TickType_t start_tick;

    (void)should_stop;

    if (!result) {
        return;
    }

    start_tick = xTaskGetTickCount();
    result->ok = sys_network_get_signal_quality(&result->rssi, &result->ber);
    result->elapsed_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount() - start_tick);
}

static void model_modem_signal_query_complete(void *user_data, bool completed, bool interrupted)
{
    model_modem_signal_result_t result;

    if (!user_data) {
        return;
    }

    result = *(model_modem_signal_result_t *)user_data;
    if (!completed || interrupted) {
        result.ok = false;
    }

    LISA_UI_LOGI("modem CSQ query seq=%u ok=%d rssi=%d ber=%d elapsed=%u ms",
                 result.seq, result.ok, result.rssi, result.ber, result.elapsed_ms);

    LISA_UI_INVOKE_UI_ARG_BASE(result, {
        if (model_modem_ctx.signal_query_seq != _invoke_result.seq) {
            return;
        }

        model_modem_ctx.signal_query_pending = 0;

        if (!model_modem_ctx.info.active ||
            !model_modem_ctx.info.connected ||
            model_modem_ctx.info.switching) {
            return;
        }

        if (!_invoke_result.ok) {
            model_modem_clear_signal_result();
            return;
        }

        model_modem_ctx.info.signal_rssi = _invoke_result.rssi;
        model_modem_ctx.info.signal_ber = _invoke_result.ber;
        model_modem_ctx.info.signal_level = model_modem_csq_to_level(_invoke_result.rssi);
    });
}

static void model_modem_schedule_signal_refresh(void)
{
    if (!model_modem_ctx.info.active ||
        !model_modem_ctx.info.connected ||
        model_modem_ctx.info.switching ||
        /* A connected cloud session may move data through the same modem
         * AT client even when no voice turn is active. */
        model_voice_cloud_is_connected() ||
        model_voice_cloud_is_running() ||
        model_voice_tts_is_playing() ||
        model_voice_tts_is_pending() ||
        voice_player_is_music_active() ||
        model_modem_ctx.signal_query_pending) {
        return;
    }

    TickType_t now = xTaskGetTickCount();
    if (model_modem_ctx.last_signal_query_tick != 0) {
        uint32_t elapsed_ms = (uint32_t)pdTICKS_TO_MS(now - model_modem_ctx.last_signal_query_tick);
        if (elapsed_ms < MODEL_MODEM_SIGNAL_REFRESH_MS) {
            return;
        }
    }

    uint32_t query_seq = ++model_modem_ctx.signal_query_seq;
    model_modem_ctx.signal_query_pending = 1;
    model_modem_ctx.last_signal_query_tick = now;

    model_modem_signal_result_t *result = &model_modem_ctx.signal_result;
    result->seq = query_seq;
    result->ok = false;
    result->rssi = MODEL_MODEM_SIGNAL_RSSI_UNKNOWN;
    result->ber = MODEL_MODEM_SIGNAL_BER_UNKNOWN;
    result->elapsed_ms = 0;

    async_task_t *task = async_task_create("modem_csq", 2048, 5,
                                           model_modem_signal_query_task,
                                           model_modem_signal_query_complete,
                                           result);
    if (!task || async_task_start(task) != 0) {
        if (task) {
            async_task_destroy(task);
        }
        model_modem_ctx.signal_query_pending = 0;
        LISA_UI_LOGW("failed to start modem signal query task");
    }
}

#endif

int model_modem_init(void)
{
    if (model_modem_ctx.inited) {
        return 0;
    }

#ifdef LISA_UI_PLATFORM_ARCS
    model_modem_sync_from_system();
    voice_msg_sub(VOICE_MSG_SYSTEM_NETWORK_CONNECTED, model_modem_state_msg_handle, NULL);
    voice_msg_sub(VOICE_MSG_SYSTEM_NETWORK_DISCONNECTED, model_modem_state_msg_handle, NULL);
    voice_msg_sub(VOICE_MSG_SYSTEM_NETWORK_SWITCHING, model_modem_state_msg_handle, NULL);
    voice_msg_sub(VOICE_MSG_SYSTEM_NETWORK_SWITCH_DONE, model_modem_state_msg_handle, NULL);
    voice_msg_sub(VOICE_MSG_SYSTEM_NETWORK_SWITCH_FAIL, model_modem_state_msg_handle, NULL);
    voice_msg_sub(VOICE_MSG_SYSTEM_NETWORK_PROBE_SUCCESS, model_modem_state_msg_handle, NULL);
    voice_msg_sub(VOICE_MSG_SYSTEM_NETWORK_PROBE_FAIL, model_modem_state_msg_handle, NULL);
#endif

    model_modem_ctx.inited = 1;
    return 0;
}

int model_modem_deinit(void)
{
    if (!model_modem_ctx.inited) {
        return 0;
    }

    model_modem_ctx.inited = 0;
    model_modem_ctx.signal_query_pending = 0;
    model_modem_ctx.signal_query_seq = 0;
    memset(&model_modem_ctx.info, 0, sizeof(model_modem_ctx.info));
    model_modem_ctx.info.signal_rssi = MODEL_MODEM_SIGNAL_RSSI_UNKNOWN;
    model_modem_ctx.info.signal_ber = MODEL_MODEM_SIGNAL_BER_UNKNOWN;

#ifdef LISA_UI_PLATFORM_ARCS
    voice_msg_unsub(VOICE_MSG_SYSTEM_NETWORK_CONNECTED, model_modem_state_msg_handle);
    voice_msg_unsub(VOICE_MSG_SYSTEM_NETWORK_DISCONNECTED, model_modem_state_msg_handle);
    voice_msg_unsub(VOICE_MSG_SYSTEM_NETWORK_SWITCHING, model_modem_state_msg_handle);
    voice_msg_unsub(VOICE_MSG_SYSTEM_NETWORK_SWITCH_DONE, model_modem_state_msg_handle);
    voice_msg_unsub(VOICE_MSG_SYSTEM_NETWORK_SWITCH_FAIL, model_modem_state_msg_handle);
    voice_msg_unsub(VOICE_MSG_SYSTEM_NETWORK_PROBE_SUCCESS, model_modem_state_msg_handle);
    voice_msg_unsub(VOICE_MSG_SYSTEM_NETWORK_PROBE_FAIL, model_modem_state_msg_handle);
    model_modem_ctx.last_signal_query_tick = 0;
#endif

    return 0;
}

int model_modem_poll(void)
{
    if (!model_modem_ctx.inited) {
        return -1;
    }

#ifdef LISA_UI_PLATFORM_ARCS
    model_modem_sync_from_system();
    model_modem_schedule_signal_refresh();
#endif

    return 0;
}

int model_modem_get_info(model_modem_info_t *info)
{
    if (info == NULL) {
        return -1;
    }

    if (!model_modem_ctx.inited) {
        return -1;
    }

    *info = model_modem_ctx.info;
    return 0;
}
