#include <string.h>

#include "ipc/acomp_ipc.h"

#include "acomp_tuner.h"
#include "acomp_err.h"
#include "gcl_cb_list/gcl_cb_list.h"
#include "private/tuner_ipc.h"
#include "comm/stream/acomp_stream_ipc.h"

#define TAG "acomp_tuner"
#include "lisa_log.h"

#define ACOMP_TUNER_DEV_NAME "acomp.tuner"

typedef struct {
    uint32_t dev_index;
    gcl_cb_list_t event_callbacks;
    acomp_stream_t *stream;
} acomp_tuner_handle_t;

static acomp_tuner_handle_t *tuner_handle = NULL;

static int tuner_control_subcmd(tuner_ipc_control_subcmd_e subcmd, void *data, uint32_t data_len);

static int tuner_require_handle(void)
{
    if (tuner_handle == NULL) {
        LISA_LOGW(TAG, "acomp tuner invalid state: handle is null");
        return ACOMP_ERR_INVALID_STATE;
    }
    return ACOMP_ERR_OK;
}

static void tuner_event_callback(acomp_ipc_message_t *message, void *priv)
{
    acomp_tuner_handle_t *handle = (acomp_tuner_handle_t *)priv;
    if (handle == NULL) {
        return;
    }

    if (message->hdr.hdr.cmd != ACOMP_CONTEXT_IPC_GLB_NOTIFY) {
        return;
    }

    if (message->acomp_cmd == ACOMP_IPC_CMD_NOTIFY_SUBCMD) {
        if (((void *)message->address != NULL) && (message->len > 0)) {
            acomp_ipc_notify_subcmd_t *subcmd = (acomp_ipc_notify_subcmd_t *)message->address;
            tuner_ipc_notify_subcmd_hdr_t *hdr = (tuner_ipc_notify_subcmd_hdr_t *)subcmd->data;

            if (hdr->cmd == TUNER_IPC_NOTIFY_SUBCMD_STATUS) {
                tuner_ipc_notify_subcmd_status_t *status =
                    (tuner_ipc_notify_subcmd_status_t *)subcmd->data;
                gcl_cb_event_dispatch(handle->event_callbacks, TUNER_CB_EVENT_STATUS,
                                      &status->status, sizeof(status->status));
            }
        }
    } else if (message->acomp_cmd == ACOMP_IPC_CMD_NOTIFY_STREAM_UPDATE) {
        if (((void *)message->address != NULL) && (message->len > 0)) {
            acomp_ipc_stream_update_t *ipc_msg = (acomp_ipc_stream_update_t *)message->address;
            uint32_t chn = ipc_msg->index;
            if (chn < sizeof(handle->stream->ch) / sizeof(handle->stream->ch[0])) {
                gcl_cb_event_dispatch(handle->event_callbacks,
                    TUNER_CB_EVENT_STREAM_UPDATE,
                    handle->stream->ch[chn],
                    sizeof(acomp_stream_channel_t));
            }
        }
    }
}

int acomp_tuner_init(void)
{
    int callback_added = 0;
    LISA_LOGI(TAG, "acomp tuner init enter");

    if (tuner_handle != NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    tuner_handle = (acomp_tuner_handle_t *)psram_malloc(sizeof(acomp_tuner_handle_t));
    if (tuner_handle == NULL) {
        return ACOMP_ERR_NO_MEM;
    }
    memset(tuner_handle, 0, sizeof(acomp_tuner_handle_t));
    tuner_handle->event_callbacks = gcl_cb_list_create();

    int dev_index = acomp_ipc_get_dev_index(ACOMP_TUNER_DEV_NAME);
    if (dev_index < 0) {
        psram_free(tuner_handle);
        tuner_handle = NULL;
        LISA_LOGE(TAG, "acomp tuner dev index not found!");
        return ACOMP_ERR_NOT_FOUND;
    }
    tuner_handle->dev_index = (uint32_t)dev_index;
    LISA_LOGI(TAG, "acomp tuner dev index %d, name:%s", tuner_handle->dev_index, ACOMP_TUNER_DEV_NAME);

    int ret = acomp_ipc_add_callback(tuner_handle->dev_index, (ipc_event_cb_t)tuner_event_callback, tuner_handle);
    if (ret != ACOMP_ERR_OK) {
        goto err_destroy_cb;
    }
    callback_added = 1;

    ret = acomp_ipc_build_frame_send_sync(tuner_handle->dev_index,
                                          ACOMP_CONTEXT_IPC_GLB_NEW | IPC_HEADER_REQ_REPALY, 0,
                                          0, NULL, 0);
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp tuner init failed!");
        goto err_remove_ipc_cb;
    }

    tuner_handle->stream = acomp_stream_create(tuner_handle->dev_index);
    if (tuner_handle->stream == NULL) {
        ret = ACOMP_ERR_CREATE_STREAM_FAILED;
        goto err_remove_ipc_cb;
    }

    LISA_LOGI(TAG, "acomp tuner init exit");
    return 0;

err_remove_ipc_cb:
    if (callback_added) {
        acomp_ipc_remove_callback(tuner_handle->dev_index, (ipc_event_cb_t)tuner_event_callback);
    }
err_destroy_cb:
    if (tuner_handle->event_callbacks != NULL) {
        gcl_cb_list_delete(tuner_handle->event_callbacks);
    }
    psram_free(tuner_handle);
    tuner_handle = NULL;
    return ret;
}

int acomp_tuner_cleanup(void)
{
    if (tuner_handle != NULL) {
        int ret = acomp_ipc_build_frame_send_sync(tuner_handle->dev_index,
                                              ACOMP_CONTEXT_IPC_GLB_CONTROL | IPC_HEADER_REQ_REPALY,
                                              ACOMP_IPC_CMD_CLEANUP, 0, NULL, 0);
        acomp_ipc_remove_callback(tuner_handle->dev_index, (ipc_event_cb_t)tuner_event_callback);
        if (tuner_handle->stream != NULL) {
            acomp_stream_destroy(tuner_handle->stream);
            tuner_handle->stream = NULL;
        }
        if (tuner_handle->event_callbacks != NULL) {
            gcl_cb_list_delete(tuner_handle->event_callbacks);
            tuner_handle->event_callbacks = NULL;
        }
        psram_free(tuner_handle);
        tuner_handle = NULL;
        return ret;
    }

    return 0;
}

int acomp_tuner_start(void)
{
    LISA_LOGI(TAG, "acomp tuner start enter");
    int ret = tuner_require_handle();
    if (ret != ACOMP_ERR_OK) {
        return ret;
    }

    ret = acomp_ipc_build_frame_send_sync(tuner_handle->dev_index,
                                          ACOMP_CONTEXT_IPC_GLB_CONTROL | IPC_HEADER_REQ_REPALY,
                                          ACOMP_IPC_CMD_START, 0, NULL, 0);
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp tuner start failed!");
    }

    LISA_LOGI(TAG, "acomp tuner start exit");
    return ret;
}

int acomp_tuner_stop(void)
{
    LISA_LOGI(TAG, "acomp tuner stop enter");
    int ret = tuner_require_handle();
    if (ret != ACOMP_ERR_OK) {
        return ret;
    }

    ret = acomp_ipc_build_frame_send_sync(tuner_handle->dev_index,
                                          ACOMP_CONTEXT_IPC_GLB_CONTROL | IPC_HEADER_REQ_REPALY,
                                          ACOMP_IPC_CMD_STOP, 0, NULL, 0);
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp tuner stop failed!");
    }

    LISA_LOGI(TAG, "acomp tuner stop exit");
    return ret;
}

static int tuner_control_subcmd(tuner_ipc_control_subcmd_e subcmd, void *data, uint32_t data_len)
{
    int ret = tuner_require_handle();
    if (ret != ACOMP_ERR_OK) {
        return ret;
    }

    uint32_t size = sizeof(acomp_ipc_control_t) + data_len;
    size = TUNER_ALIGN_SIZE(size);

    acomp_ipc_control_t *ipc_control = psram_malloc_align(IPC_ALIGN_SIZE, size);
    if (ipc_control == NULL) {
        return ACOMP_ERR_NO_MEM;
    }

    ipc_control->control = subcmd;
    ipc_control->len = data_len;

    if (data_len > 0 && data != NULL) {
        memcpy(ipc_control->data, data, data_len);
    }

    ret = acomp_ipc_build_frame_send_sync(tuner_handle->dev_index,
                                          ACOMP_CONTEXT_IPC_GLB_CONTROL | IPC_HEADER_REQ_REPALY,
                                          ACOMP_IPC_CMD_CONTROL, 0, ipc_control, size);

    psram_free(ipc_control);
    return ret;
}

int acomp_tuner_enable(uint8_t en)
{
    LISA_LOGI(TAG, "acomp tuner enable: %d", en);

    tuner_ipc_control_subcmd_enable_t cmd = { .en = en };
    int ret = tuner_control_subcmd(TUNER_IPC_CONTROL_SUBCMD_ENABLE, &cmd, sizeof(cmd));
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp tuner enable failed!");
    }
    return ret;
}

int acomp_tuner_set_samplerate(float sr)
{
    LISA_LOGI(TAG, "acomp tuner set samplerate: %.0f", sr);

    tuner_ipc_control_subcmd_set_samplerate_t cmd = { .sample_rate = sr };
    int ret = tuner_control_subcmd(TUNER_IPC_CONTROL_SUBCMD_SET_SAMPLERATE, &cmd, sizeof(cmd));
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp tuner set samplerate failed!");
    }
    return ret;
}

int acomp_tuner_set_param(void *param, uint32_t len)
{
    LISA_LOGI(TAG, "acomp tuner set param, len=%u", len);

    if (param == NULL || len == 0) {
        return ACOMP_ERR_INVALID_ARG;
    }

    int ret = tuner_require_handle();
    if (ret != ACOMP_ERR_OK) {
        return ret;
    }

    uint32_t data_len = sizeof(tuner_ipc_control_subcmd_set_param_t) + len;
    uint32_t size = sizeof(acomp_ipc_control_t) + data_len;
    size = TUNER_ALIGN_SIZE(size);

    acomp_ipc_control_t *ipc_control = psram_malloc_align(IPC_ALIGN_SIZE, size);
    if (ipc_control == NULL) {
        return ACOMP_ERR_NO_MEM;
    }

    ipc_control->control = TUNER_IPC_CONTROL_SUBCMD_SET_PARAM;
    ipc_control->len = data_len;

    tuner_ipc_control_subcmd_set_param_t *set_param =
        (tuner_ipc_control_subcmd_set_param_t *)ipc_control->data;
    set_param->len = len;
    memcpy(set_param->data, param, len);

    ret = acomp_ipc_build_frame_send_sync(tuner_handle->dev_index,
                                          ACOMP_CONTEXT_IPC_GLB_CONTROL | IPC_HEADER_REQ_REPALY,
                                          ACOMP_IPC_CMD_CONTROL, 0, ipc_control, size);

    psram_free(ipc_control);

    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp tuner set param failed!");
    }
    return ret;
}

int acomp_tuner_set_limiter(void *param, uint32_t len)
{
    LISA_LOGI(TAG, "acomp tuner set limiter, len=%u", len);

    if (param == NULL || len == 0) {
        return ACOMP_ERR_INVALID_ARG;
    }

    int ret = tuner_require_handle();
    if (ret != ACOMP_ERR_OK) {
        return ret;
    }

    uint32_t data_len = sizeof(tuner_ipc_control_subcmd_set_limiter_t) + len;
    uint32_t size = sizeof(acomp_ipc_control_t) + data_len;
    size = TUNER_ALIGN_SIZE(size);

    acomp_ipc_control_t *ipc_control = psram_malloc_align(IPC_ALIGN_SIZE, size);
    if (ipc_control == NULL) {
        return ACOMP_ERR_NO_MEM;
    }

    ipc_control->control = TUNER_IPC_CONTROL_SUBCMD_SET_LIMITER;
    ipc_control->len = data_len;

    tuner_ipc_control_subcmd_set_limiter_t *set_limiter =
        (tuner_ipc_control_subcmd_set_limiter_t *)ipc_control->data;
    set_limiter->len = len;
    memcpy(set_limiter->data, param, len);

    ret = acomp_ipc_build_frame_send_sync(tuner_handle->dev_index,
                                          ACOMP_CONTEXT_IPC_GLB_CONTROL | IPC_HEADER_REQ_REPALY,
                                          ACOMP_IPC_CMD_CONTROL, 0, ipc_control, size);

    psram_free(ipc_control);

    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp tuner set limiter failed!");
    }
    return ret;
}

int acomp_tuner_set_volume(float vol)
{
    LISA_LOGI(TAG, "acomp tuner set volume: %.3f", vol);

    tuner_ipc_control_subcmd_set_volume_t cmd = { .volume = vol };
    int ret = tuner_control_subcmd(TUNER_IPC_CONTROL_SUBCMD_SET_VOLUME, &cmd, sizeof(cmd));
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp tuner set volume failed!");
    }
    return ret;
}

int acomp_tuner_add_callback(uint32_t events, tuner_event_cb_t cb, void *priv)
{
    if (tuner_handle == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }
    return gcl_cb_list_add_callback(tuner_handle->event_callbacks, events, cb, priv);
}

int acomp_tuner_remove_callback(tuner_event_cb_t cb)
{
    if (tuner_handle == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }
    return gcl_cb_list_remove_callback(tuner_handle->event_callbacks, cb);
}

int acomp_tuner_stream_ch_enable(int chn, acomp_stream_chn_create_desc_t *desc)
{
    if ((tuner_handle == NULL) || (tuner_handle->stream == NULL)) {
        return ACOMP_ERR_INVALID_STATE;
    }

    if (chn >= ACOMP_STREAM_MAX_CHANNEL) {
        return ACOMP_ERR_INVALID_ARG;
    }

    tuner_handle->stream->ch[chn] = acomp_stream_ipc_channel_create(
        tuner_handle->stream, chn, tuner_handle->dev_index, desc);
    if (tuner_handle->stream->ch[chn] == NULL) {
        return ACOMP_ERR_CREATE_STREAM_FAILED;
    }
    LISA_LOGI(TAG, "stream_ch_enable chn(%s) index(%d)", desc->cname, chn);
    return ACOMP_ERR_OK;
}

int acomp_tuner_stream_ch_disable(int chn)
{
    if (tuner_handle == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    if (chn >= ACOMP_STREAM_MAX_CHANNEL) {
        return ACOMP_ERR_INVALID_ARG;
    }

    int ret = acomp_stream_ipc_channel_destroy(tuner_handle->stream, tuner_handle->dev_index, (uint32_t)chn);
    LISA_LOGI(TAG, "stream_ch_disable chn(%d), ret(%d)", chn, ret);
    return ret;
}

void *acomp_tuner_stream_tx_buffer_alloc(int chn, uint32_t *len, uint16_t *desc_idx)
{
    if (tuner_handle == NULL || chn >= ACOMP_STREAM_MAX_CHANNEL) {
        return NULL;
    }

    if (tuner_handle->stream->ch[chn] == NULL) {
        return NULL;
    }

    return tuner_handle->stream->ops.tx_buffer_alloc(tuner_handle->stream->ch[chn], len, desc_idx);
}

int acomp_tuner_stream_tx_buffer_submit(int chn, void *buffer, uint32_t len, uint16_t desc_idx)
{
    if (tuner_handle == NULL || chn >= ACOMP_STREAM_MAX_CHANNEL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    acomp_stream_channel_t *channel = tuner_handle->stream->ch[chn];
    if (channel == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    return tuner_handle->stream->ops.tx_buffer_submit(channel, buffer, len, desc_idx);
}

int acomp_tuner_stream_kick(int chn)
{
    if (tuner_handle == NULL || chn >= ACOMP_STREAM_MAX_CHANNEL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    acomp_stream_channel_t *channel = tuner_handle->stream->ch[chn];
    if (channel == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    return tuner_handle->stream->ops.kick(channel);
}

void *acomp_tuner_stream_rx_buffer_get(int chn, uint32_t *len, uint16_t *desc_idx)
{
    if (tuner_handle == NULL || chn >= ACOMP_STREAM_MAX_CHANNEL) {
        return NULL;
    }

    if (tuner_handle->stream->ch[chn] == NULL) {
        return NULL;
    }

    return tuner_handle->stream->ops.rx_buffer_get(tuner_handle->stream->ch[chn], len, desc_idx);
}

int acomp_tuner_stream_rx_buffer_release(int chn, uint16_t desc_idx, uint32_t len, void *buffer)
{
    if (tuner_handle == NULL || chn >= ACOMP_STREAM_MAX_CHANNEL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    acomp_stream_channel_t *channel = tuner_handle->stream->ch[chn];
    if (channel == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    return tuner_handle->stream->ops.rx_buffer_release(channel, buffer, len, desc_idx);
}
