#include <string.h>

#include "ipc/acomp_ipc.h"

#include "acomp_xtts.h"
#include "acomp_err.h"
#include "gcl_cb_list/gcl_cb_list.h"
#include "private/xtts_ipc.h"
#include "comm/stream/acomp_stream_ipc.h"

#define TAG "acomp_xtts"
#include "lisa_log.h"

#define ACOMP_XTTS_DEV_NAME "acomp.xtts"

typedef struct {
    uint32_t dev_index;
    gcl_cb_list_t event_callbacks;
    acomp_stream_t *stream;
} acomp_xtts_handle_t;

acomp_xtts_handle_t *xtts_handle = NULL;

static int xtts_control_subcmd(xtts_ipc_control_subcmd_e subcmd, void *data, uint32_t data_len);

void xtts_event_callback(acomp_ipc_message_t *message, void *priv)
{
    acomp_xtts_handle_t *handle = (acomp_xtts_handle_t *)priv;
    if (handle == NULL) {
        return;
    }

    if (message->hdr.hdr.cmd == ACOMP_CONTEXT_IPC_GLB_NOTIFY) {

        if (message->acomp_cmd == ACOMP_IPC_CMD_NOTIFY_SUBCMD) {

            if (((void *)message->address != NULL) && (message->len > 0)) {

                acomp_ipc_notify_subcmd_t *subcmd = (acomp_ipc_notify_subcmd_t *)message->address;
                xtts_ipc_notify_subcmd_hdr_t *hdr = (xtts_ipc_notify_subcmd_hdr_t *)subcmd->data;

                if (hdr->cmd == XTTS_IPC_NOTIFY_SUBCMD_SYNTH_STATUS) {
                    xtts_ipc_notify_subcmd_status_t *status = (xtts_ipc_notify_subcmd_status_t *)subcmd->data;
                    gcl_cb_event_dispatch(handle->event_callbacks, XTTS_CB_EVENT_STATUS, &status->status,
                                          sizeof(status->status));
                } else {
                    LISA_LOGW(TAG, "unknow notify hdr cmd:%d", hdr->cmd);
                }
            }
        } else if (message->acomp_cmd == ACOMP_IPC_CMD_NOTIFY_STREAM_UPDATE) {

            if (((void *)message->address != NULL) && (message->len > 0)) {
                acomp_ipc_stream_update_t *ipc_msg;
                uint32_t chn;
                ipc_msg = (acomp_ipc_stream_update_t *)message->address;
                chn = ipc_msg->index;
                if (chn < sizeof(handle->stream->ch) / sizeof(handle->stream->ch[0])) {
                    gcl_cb_event_dispatch(handle->event_callbacks,
                        XTTS_CB_EVENT_STREAM_UPDATE,
                        handle->stream->ch[chn],
                        sizeof(acomp_stream_channel_t));
                }
            }
        }
    }
}

int acomp_xtts_init(void)
{
    int ret = 0;
    LISA_LOGI(TAG, "acomp xtts init enter");

    if (xtts_handle != NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    xtts_handle = (acomp_xtts_handle_t *)psram_malloc(sizeof(acomp_xtts_handle_t));
    if (xtts_handle == NULL) {
        return ACOMP_ERR_NO_MEM;
    }
    memset(xtts_handle, 0, sizeof(acomp_xtts_handle_t));
    xtts_handle->event_callbacks = gcl_cb_list_create();

    int dev_index = acomp_ipc_get_dev_index(ACOMP_XTTS_DEV_NAME);
    if (dev_index < 0) {
        psram_free(xtts_handle);
        xtts_handle = NULL;
        LISA_LOGE(TAG, "acomp xtts dev index not found!");
        return ACOMP_ERR_NOT_FOUND;
    }
    xtts_handle->dev_index = (uint32_t)dev_index;
    LISA_LOGI(TAG, "acomp xtts dev index %d,name:%s", xtts_handle->dev_index, ACOMP_XTTS_DEV_NAME);

    ret = acomp_ipc_add_callback(xtts_handle->dev_index, (ipc_event_cb_t)xtts_event_callback, xtts_handle);
    if (ret != ACOMP_ERR_OK) {
        return ret;
    }

    ret = acomp_ipc_build_frame_send_sync(xtts_handle->dev_index, ACOMP_CONTEXT_IPC_GLB_NEW | IPC_HEADER_REQ_REPALY, 0,
                                          0, NULL, 0);
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp xtts init failed!");
        return ret;
    }

    xtts_handle->stream = acomp_stream_create(xtts_handle->dev_index);
    if (xtts_handle->stream == NULL) {
        return ACOMP_ERR_CREATE_STREAM_FAILED;
    }

    LISA_LOGI(TAG, "acomp xtts init exit");

    return 0;
}

int acomp_xtts_prepare(acomp_ipc_prepare_t *prepare)
{
    LISA_LOGI(TAG, "acomp xtts prepare enter");

    if (prepare == NULL) {
        return ACOMP_ERR_INVALID_ARG;
    }

    uint32_t size;
    int ret;

    size = sizeof(acomp_ipc_prepare_t) + sizeof(acomp_res_item_t) * prepare->number;
    size = ALIGN_SIZE(size);

    ret = acomp_ipc_build_frame_send_sync(xtts_handle->dev_index, ACOMP_CONTEXT_IPC_GLB_CONTROL | IPC_HEADER_REQ_REPALY,
                                          ACOMP_IPC_CMD_PREPARE, 0, prepare, size);
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp xtts prepare failed!");
    }

    LISA_LOGI(TAG, "acomp xtts prepare exit");
    return ret;
}

int acomp_xtts_cleanup(void)
{
    int ret;
    ret = acomp_ipc_build_frame_send_sync(xtts_handle->dev_index, ACOMP_CONTEXT_IPC_GLB_CONTROL | IPC_HEADER_REQ_REPALY,
                                          ACOMP_IPC_CMD_CLEANUP, 0, NULL, 0);
    return ret;
}

int acomp_xtts_start(void)
{
    LISA_LOGI(TAG, "acomp xtts start enter");
    int ret;
    ret = acomp_ipc_build_frame_send_sync(xtts_handle->dev_index, ACOMP_CONTEXT_IPC_GLB_CONTROL | IPC_HEADER_REQ_REPALY,
                                          ACOMP_IPC_CMD_START, 0, NULL, 0);
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp xtts start failed!");
    }

    LISA_LOGI(TAG, "acomp xtts start exit");
    return ret;
}

int acomp_xtts_stop(void)
{
    LISA_LOGI(TAG, "acomp xtts stop enter");
    int ret;
    ret = acomp_ipc_build_frame_send_sync(xtts_handle->dev_index, ACOMP_CONTEXT_IPC_GLB_CONTROL | IPC_HEADER_REQ_REPALY,
                                          ACOMP_IPC_CMD_STOP, 0, NULL, 0);
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp xtts stop failed!");
    }

    LISA_LOGI(TAG, "acomp xtts stop exit");
    return ret;
}

static int xtts_control_subcmd(xtts_ipc_control_subcmd_e subcmd, void *data, uint32_t data_len)
{
    int ret;
    uint32_t size;
    acomp_ipc_control_t *ipc_control;

    size = sizeof(acomp_ipc_control_t) + data_len;
    size = ALIGN_SIZE(size);

    ipc_control = psram_malloc_align(IPC_ALIGN_SIZE, size);
    if (ipc_control == NULL) {
        return ACOMP_ERR_NO_MEM;
    }

    ipc_control->control = subcmd;
    ipc_control->len = data_len;

    uint8_t *ipc_data = ipc_control->data;
    if (data_len > 0) {
        memcpy(ipc_data, data, data_len);
    }

    ret = acomp_ipc_build_frame_send_sync(xtts_handle->dev_index, ACOMP_CONTEXT_IPC_GLB_CONTROL | IPC_HEADER_REQ_REPALY,
                                            ACOMP_IPC_CMD_CONTROL, 0, ipc_control, size);

    psram_free(ipc_control);

    return ret;
}

int acomp_xtts_synth_text(const char *text, uint32_t len)
{
    LISA_LOGI(TAG, "acomp xtts synth text enter, len=%u", len);

    if (text == NULL || len == 0) {
        return ACOMP_ERR_INVALID_ARG;
    }

    int ret;
    uint32_t data_len;
    uint32_t size;
    acomp_ipc_control_t *ipc_control;

    data_len = sizeof(xtts_ipc_control_subcmd_synth_text_t) + len;
    size = sizeof(acomp_ipc_control_t) + data_len;
    size = ALIGN_SIZE(size);

    ipc_control = psram_malloc_align(IPC_ALIGN_SIZE, size);
    if (ipc_control == NULL) {
        return ACOMP_ERR_NO_MEM;
    }

    ipc_control->control = XTTS_IPC_CONTROL_SUBCMD_SYNTH_TEXT;
    ipc_control->len = data_len;

    xtts_ipc_control_subcmd_synth_text_t *synth = (xtts_ipc_control_subcmd_synth_text_t *)ipc_control->data;
    synth->len = len;
    memcpy(synth->text, text, len);

    ret = acomp_ipc_build_frame_send_sync(xtts_handle->dev_index, ACOMP_CONTEXT_IPC_GLB_CONTROL | IPC_HEADER_REQ_REPALY,
                                            ACOMP_IPC_CMD_CONTROL, 0, ipc_control, size);

    psram_free(ipc_control);

    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp xtts synth text failed!");
    }

    LISA_LOGI(TAG, "acomp xtts synth text exit");
    return ret;
}

int acomp_xtts_set_speed(int speed)
{
    LISA_LOGI(TAG, "acomp xtts set speed enter, speed=%d", speed);

    xtts_ipc_control_subcmd_set_speed_t speed_set;
    speed_set.speed = speed;

    int ret = xtts_control_subcmd(XTTS_IPC_CONTROL_SUBCMD_SET_SPEED, &speed_set, sizeof(xtts_ipc_control_subcmd_set_speed_t));
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp xtts set speed failed!");
    }

    LISA_LOGI(TAG, "acomp xtts set speed exit");
    return ret;
}

int acomp_xtts_set_volume(int volume)
{
    LISA_LOGI(TAG, "acomp xtts set volume enter, volume=%d", volume);

    xtts_ipc_control_subcmd_set_volume_t volume_set;
    volume_set.volume = volume;

    int ret = xtts_control_subcmd(XTTS_IPC_CONTROL_SUBCMD_SET_VOLUME, &volume_set, sizeof(xtts_ipc_control_subcmd_set_volume_t));
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp xtts set volume failed!");
    }

    LISA_LOGI(TAG, "acomp xtts set volume exit");
    return ret;
}

int acomp_xtts_set_role(int role)
{
    LISA_LOGI(TAG, "acomp xtts set role enter, role=%d", role);

    xtts_ipc_control_subcmd_set_role_t role_set;
    role_set.role = role;

    int ret = xtts_control_subcmd(XTTS_IPC_CONTROL_SUBCMD_SET_ROLE, &role_set, sizeof(xtts_ipc_control_subcmd_set_role_t));
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp xtts set role failed!");
    }

    LISA_LOGI(TAG, "acomp xtts set role exit");
    return ret;
}

int acomp_xtts_add_callback(uint32_t events, xtts_event_cb_t cb, void *priv)
{
    int ret;
    if (xtts_handle == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    ret = gcl_cb_list_add_callback(xtts_handle->event_callbacks, events, cb, priv);
    return ret;
}

int acomp_xtts_remove_callback(xtts_event_cb_t cb)
{
    int ret;
    if (xtts_handle == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    ret = gcl_cb_list_remove_callback(xtts_handle->event_callbacks, cb);
    return ret;
}

int acomp_xtts_stream_ch_enable(int chn, acomp_stream_chn_create_desc_t *desc)
{
    int ret = 0;

    if ((xtts_handle == NULL) || (xtts_handle->stream == NULL)) {
        return ACOMP_ERR_INVALID_STATE;
    }

    if (chn >= ACOMP_STREAM_MAX_CHANNEL) {
        return ACOMP_ERR_INVALID_ARG;
    }

    xtts_handle->stream->ch[chn] = acomp_stream_ipc_channel_create(xtts_handle->stream, chn, xtts_handle->dev_index, desc);
    if (xtts_handle->stream->ch[chn] == NULL) {
        return ACOMP_ERR_CREATE_STREAM_FAILED;
    }
    LISA_LOGI(TAG, "acomp_xtts_stream_ch_enable chn(%s) index(%d),desc(%p)", desc->cname, chn, desc);
    return ret;
}

int acomp_xtts_stream_ch_disable(int chn)
{
    int ret;
    if (xtts_handle == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    if (chn >= ACOMP_STREAM_MAX_CHANNEL) {
        return ACOMP_ERR_INVALID_ARG;
    }

    ret = acomp_stream_ipc_channel_destroy(chn);
    LISA_LOGI(TAG, "acomp_xtts_stream_ch_disable chn index(%d),ret(%d)", chn, ret);
    xtts_handle->stream->ch[chn] = NULL;
    return ret;
}

void *acomp_xtts_stream_rx_buffer_get(int chn, uint32_t *len, uint16_t *desc_idx)
{
    uint8_t *ptr;

    if (xtts_handle == NULL) {
        return NULL;
    }

    if (chn >= ACOMP_STREAM_MAX_CHANNEL) {
        return NULL;
    }

    if (xtts_handle->stream->ch[chn] == NULL) {
        return NULL;
    }

    ptr = xtts_handle->stream->ops.rx_buffer_get(xtts_handle->stream->ch[chn], len, desc_idx);
    return ptr;
}

int acomp_xtts_stream_rx_buffer_release(int chn, uint16_t desc_idx, uint32_t len, void *buffer)
{
    int ret;

    if (xtts_handle == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    if (chn >= ACOMP_STREAM_MAX_CHANNEL) {
        return ACOMP_ERR_INVALID_ARG;
    }

    ret = xtts_handle->stream->ops.rx_buffer_release(xtts_handle->stream->ch[chn], buffer, len, desc_idx);

    return ret;
}
