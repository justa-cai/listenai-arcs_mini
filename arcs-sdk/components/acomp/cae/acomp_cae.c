#include <string.h>

#include "ipc/acomp_ipc.h"

#include "acomp_cae.h"
#include "acomp_err.h"
#include "gcl_cb_list/gcl_cb_list.h"
#include "private/cae_ipc.h"
#include "acomp_stream_ipc.h"

#define TAG "acomp_cae"
#include "lisa_log.h"

#define ACOMP_CAE_DEV_NAME "acomp.cae"

typedef struct {
    int dev_index;
    gcl_cb_list_t event_callbacks;
    acomp_stream_t *stream;
    int angle_data[ACOMP_CAE_ANGLE_MAX_CNT];
    uint32_t angle_cnt;
} acomp_cae_handle_t;

acomp_cae_handle_t *cae_handle = NULL;

static int acomp_cae_control_subcmd(cae_ipc_control_subcmd_e subcmd, void *data, uint32_t data_len);
int acomp_cae_stream_ch_disable(int chn);

static int acomp_cae_ready(void)
{
    return cae_handle != NULL && cae_handle->stream != NULL;
}

static int acomp_cae_res_item_valid(const acomp_cae_res_item_t *item)
{
    if (item == NULL || item->size == 0U) {
        return 0;
    }

    if (item->storage == ACOMP_CAE_RES_STORAGE_SD) {
        return 1;
    }

    return item->addr != 0U;
}

static void cae_event_callback(acomp_ipc_message_t *message, void *priv)
{
    acomp_cae_handle_t *handle = (acomp_cae_handle_t *)priv;
    if (handle == NULL || message->hdr.hdr.cmd != ACOMP_CONTEXT_IPC_GLB_NOTIFY) {
        return;
    }

    if (message->address == 0 || message->len == 0) {
        return;
    }

    if (message->acomp_cmd == ACOMP_IPC_CMD_NOTIFY_SUBCMD) {
        acomp_ipc_notify_subcmd_t *subcmd = (acomp_ipc_notify_subcmd_t *)message->address;
        if (subcmd->len < sizeof(cae_ipc_notify_subcmd_hdr_t)) {
            LISA_LOGW(TAG, "invalid notify subcmd len:%u", subcmd->len);
            return;
        }

        cae_ipc_notify_subcmd_hdr_t *hdr = (cae_ipc_notify_subcmd_hdr_t *)subcmd->data;

        switch (hdr->cmd) {
        case CAE_IPC_NOTIFY_SUBCMD_PCM_DATA: {
            if (subcmd->len < sizeof(cae_ipc_notify_subcmd_pcm_data_t)) {
                return;
            }
            cae_ipc_notify_subcmd_pcm_data_t *pcm = (cae_ipc_notify_subcmd_pcm_data_t *)subcmd->data;
            if (pcm->len > subcmd->len - sizeof(cae_ipc_notify_subcmd_pcm_data_t)) {
                return;
            }
            gcl_cb_event_dispatch(handle->event_callbacks, CAE_CB_EVENT_STREAM_UPDATE,
                                  pcm->data, pcm->len);
            break;
        }
        case CAE_IPC_NOTIFY_SUBCMD_ANGLE: {
            if (subcmd->len < sizeof(cae_ipc_notify_subcmd_angle_t)) {
                return;
            }
            cae_ipc_notify_subcmd_angle_t *angle = (cae_ipc_notify_subcmd_angle_t *)subcmd->data;
            if (angle->angle_cnt > ACOMP_CAE_ANGLE_MAX_CNT ||
                angle->angle_cnt * sizeof(uint16_t) > subcmd->len - sizeof(cae_ipc_notify_subcmd_angle_t)) {
                return;
            }
            handle->angle_cnt = angle->angle_cnt;
            for (uint32_t i = 0; i < handle->angle_cnt; i++) {
                handle->angle_data[i] = angle->angle_data[i];
            }
            gcl_cb_event_dispatch(handle->event_callbacks, CAE_CB_EVENT_ENGINE_ANGLE,
                                  angle->angle_data, angle->angle_cnt * sizeof(short));
            break;
        }
        case CAE_IPC_NOTIFY_SUBCMD_ERROR: {
            if (subcmd->len < sizeof(cae_ipc_notify_subcmd_error_t)) {
                return;
            }
            cae_ipc_notify_subcmd_error_t *err = (cae_ipc_notify_subcmd_error_t *)subcmd->data;
            gcl_cb_event_dispatch(handle->event_callbacks, CAE_CB_EVENT_ENGINE_ERROR,
                                  err, subcmd->len);
            break;
        }
        default:
            LISA_LOGW(TAG, "unknown notify hdr cmd:%d", hdr->cmd);
            break;
        }
        return;
    }

    if (message->acomp_cmd == ACOMP_IPC_CMD_NOTIFY_STREAM_UPDATE) {
        if (handle->stream == NULL || message->len < sizeof(acomp_ipc_stream_update_t)) {
            return;
        }
        acomp_ipc_stream_update_t *ipc_msg = (acomp_ipc_stream_update_t *)message->address;
        uint32_t chn = ipc_msg->index;
        uint32_t max_chn = sizeof(handle->stream->ch) / sizeof(handle->stream->ch[0]);

        if (chn < max_chn) {
            gcl_cb_event_dispatch(handle->event_callbacks, CAE_CB_EVENT_STREAM_UPDATE,
                                  handle->stream->ch[chn], sizeof(acomp_stream_channel_t));
        }
    }
}

int acomp_cae_init(void)
{
    int ret = 0;
    int callback_added = 0;
    int remote_created = 0;
    LISA_LOGI(TAG, "acomp cae init enter");

    if (cae_handle != NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    cae_handle = (acomp_cae_handle_t *)psram_malloc(sizeof(acomp_cae_handle_t));
    if (cae_handle == NULL) {
        return ACOMP_ERR_NO_MEM;
    }
    memset(cae_handle, 0, sizeof(acomp_cae_handle_t));
    cae_handle->event_callbacks = gcl_cb_list_create();
    if (cae_handle->event_callbacks == NULL) {
        ret = ACOMP_ERR_NO_MEM;
        goto fail;
    }
    cae_handle->dev_index = acomp_ipc_get_dev_index(ACOMP_CAE_DEV_NAME);

    if (cae_handle->dev_index < 0) {
        LISA_LOGE(TAG, "acomp cae dev index not found!");
        ret = ACOMP_ERR_NOT_FOUND;
        goto fail;
    }
    LISA_LOGI(TAG, "acomp cae dev index %d, name:%s", cae_handle->dev_index, ACOMP_CAE_DEV_NAME);

    ret = acomp_ipc_add_callback(cae_handle->dev_index, (ipc_event_cb_t)cae_event_callback, cae_handle);
    if (ret != ACOMP_ERR_OK) {
        goto fail;
    }
    callback_added = 1;

    ret = acomp_ipc_build_frame_send_sync(cae_handle->dev_index, ACOMP_CONTEXT_IPC_GLB_NEW | IPC_HEADER_REQ_REPALY, 0,
                                          0, NULL, 0);
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp cae init failed!");
        goto fail;
    }
    remote_created = 1;

    cae_handle->stream = acomp_stream_create(cae_handle->dev_index);
    if (cae_handle->stream == NULL) {
        ret = ACOMP_ERR_CREATE_STREAM_FAILED;
        goto fail;
    }

    LISA_LOGI(TAG, "acomp cae init exit");

    return 0;

fail:
    if (cae_handle != NULL) {
        if (remote_created) {
            (void)acomp_ipc_build_frame_send_sync(cae_handle->dev_index,
                                                  ACOMP_CONTEXT_IPC_GLB_FREE | IPC_HEADER_REQ_REPALY,
                                                  0, 0, NULL, 0);
        }
        if (callback_added) {
            acomp_ipc_remove_callback(cae_handle->dev_index, (ipc_event_cb_t)cae_event_callback);
        }
        if (cae_handle->stream != NULL) {
            acomp_stream_destroy(cae_handle->stream);
        }
        if (cae_handle->event_callbacks != NULL) {
            gcl_cb_list_delete(cae_handle->event_callbacks);
        }
        psram_free(cae_handle);
        cae_handle = NULL;
    }
    return ret;
}

int acomp_cae_deinit(void)
{
    int ret = ACOMP_ERR_OK;

    if (cae_handle == NULL) {
        return ACOMP_ERR_OK;
    }

    for (int chn = 0; chn < ACOMP_STREAM_MAX_CHANNEL; chn++) {
        if (cae_handle->stream != NULL && cae_handle->stream->ch[chn] != NULL) {
            (void)acomp_cae_stream_ch_disable(chn);
        }
    }

    ret = acomp_ipc_build_frame_send_sync(cae_handle->dev_index,
                                          ACOMP_CONTEXT_IPC_GLB_FREE | IPC_HEADER_REQ_REPALY,
                                          0, 0, NULL, 0);
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGW(TAG, "acomp cae remote free failed:%d", ret);
    }

    acomp_ipc_remove_callback(cae_handle->dev_index, (ipc_event_cb_t)cae_event_callback);
    if (cae_handle->stream != NULL) {
        acomp_stream_destroy(cae_handle->stream);
        cae_handle->stream = NULL;
    }
    if (cae_handle->event_callbacks != NULL) {
        gcl_cb_list_delete(cae_handle->event_callbacks);
        cae_handle->event_callbacks = NULL;
    }
    psram_free(cae_handle);
    cae_handle = NULL;

    return ret;
}

static int acomp_cae_prepare_send(const acomp_cae_resource_config_t *config)
{
    acomp_ipc_prepare_t *prepare = NULL;
    uint32_t size;
    int ret;

    if (config == NULL) {
        return ACOMP_ERR_INVALID_ARG;
    }

    if (cae_handle == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    size = sizeof(acomp_ipc_prepare_t) + sizeof(acomp_res_item_t) * ACOMP_CAE_RES_NUMBER;
    size = ALIGN_SIZE(size);

    prepare = psram_malloc_align(IPC_ALIGN_SIZE, size);
    if (prepare == NULL) {
        return ACOMP_ERR_NO_MEM;
    }
    memset(prepare, 0, size);

    prepare->number = ACOMP_CAE_RES_NUMBER;
    prepare->item[0].index = CAE_INDEX_AES;
    prepare->item[0].attr.hdr.storage = config->aes.storage;
    prepare->item[0].addr = (uint32_t)config->aes.addr;
    prepare->item[0].offset = 0;
    prepare->item[0].size = config->aes.size;

    prepare->item[1].index = CAE_INDEX_JSON;
    prepare->item[1].attr.hdr.storage = config->json.storage;
    prepare->item[1].addr = (uint32_t)config->json.addr;
    prepare->item[1].offset = 0;
    prepare->item[1].size = config->json.size;

    ret = acomp_ipc_build_frame_send_sync(cae_handle->dev_index, ACOMP_CONTEXT_IPC_GLB_CONTROL | IPC_HEADER_REQ_REPALY,
                                          ACOMP_IPC_CMD_PREPARE, 0, prepare, size);
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp cae prepare failed!");
    }

    psram_free(prepare);
    return ret;
}

int acomp_cae_prepare(void)
{
    const acomp_cae_resource_config_t config = {
        .aes = {
            .storage = ACOMP_CAE_RES_STORAGE_FLASH,
            .addr = CONFIG_ACOMP_CAE_RES_CAE_AES_ADDRESS,
            .size = CONFIG_ACOMP_CAE_RES_CAE_AES_LENGTH,
        },
        .json = {
            .storage = ACOMP_CAE_RES_STORAGE_FLASH,
            .addr = CONFIG_ACOMP_CAE_RES_CAE_JSON_ADDRESS,
            .size = CONFIG_ACOMP_CAE_RES_CAE_JSON_LENGTH,
        },
    };

    return acomp_cae_prepare_with_resources(&config);
}

int acomp_cae_prepare_with_resources(const acomp_cae_resource_config_t *config)
{
    int ret;

    LISA_LOGI(TAG, "acomp cae prepare enter");

    if (config == NULL || !acomp_cae_res_item_valid(&config->aes) ||
        !acomp_cae_res_item_valid(&config->json)) {
        return ACOMP_ERR_INVALID_ARG;
    }

    ret = acomp_cae_prepare_send(config);
    LISA_LOGI(TAG, "acomp cae prepare exit");
    return ret;
}

int acomp_cae_cleanup(void)
{
    if (cae_handle == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    return acomp_ipc_build_frame_send_sync(cae_handle->dev_index,
                                           ACOMP_CONTEXT_IPC_GLB_CONTROL | IPC_HEADER_REQ_REPALY,
                                           ACOMP_IPC_CMD_CLEANUP, 0, NULL, 0);
}

int acomp_cae_start(void)
{
    LISA_LOGI(TAG, "acomp cae start enter");
    int ret;
    if (cae_handle == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    ret = acomp_ipc_build_frame_send_sync(cae_handle->dev_index, ACOMP_CONTEXT_IPC_GLB_CONTROL | IPC_HEADER_REQ_REPALY,
                                          ACOMP_IPC_CMD_START, 0, NULL, 0);
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp cae start failed!");
    }

    LISA_LOGI(TAG, "acomp cae start exit");
    return ret;
}

int acomp_cae_stop(void)
{
    LISA_LOGI(TAG, "acomp cae stop enter");
    int ret;
    if (cae_handle == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    ret = acomp_ipc_build_frame_send_sync(cae_handle->dev_index, ACOMP_CONTEXT_IPC_GLB_CONTROL | IPC_HEADER_REQ_REPALY,
                                          ACOMP_IPC_CMD_STOP, 0, NULL, 0);
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp cae stop failed!");
    }

    LISA_LOGI(TAG, "acomp cae stop exit");
    return ret;
}

static int acomp_cae_control_subcmd(cae_ipc_control_subcmd_e subcmd, void *data, uint32_t data_len)
{
    int ret;
    uint32_t size;
    acomp_ipc_control_t *ipc_control;

    if (cae_handle == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    if (data_len > 0 && data == NULL) {
        return ACOMP_ERR_INVALID_ARG;
    }

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

    ret = acomp_ipc_build_frame_send_sync(cae_handle->dev_index, ACOMP_CONTEXT_IPC_GLB_CONTROL | IPC_HEADER_REQ_REPALY,
                                          ACOMP_IPC_CMD_CONTROL, 0, ipc_control, size);

    psram_free(ipc_control);

    return ret;
}

int acomp_cae_set_beam(acomp_cae_beam_e beam)
{
    LISA_LOGI(TAG, "acomp cae set beam enter, beam=%d", beam);

    if (beam < ACOMP_CAE_BEAM_0_60 || beam > ACOMP_CAE_BEAM_120_180) {
        LISA_LOGE(TAG, "invalid beam: %d", beam);
        return ACOMP_ERR_INVALID_ARG;
    }

    cae_ipc_control_subcmd_beam_set_t beam_set;
    beam_set.beam_index = (uint8_t)beam;

    int ret = acomp_cae_control_subcmd(CAE_IPC_CONTROL_SUBCMD_BEAM_SET, &beam_set, sizeof(cae_ipc_control_subcmd_beam_set_t));
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp cae set beam failed!");
    }

    LISA_LOGI(TAG, "acomp cae set beam exit");
    return ret;
}

int acomp_cae_get_angle(int *angle_data, uint32_t *angle_cnt)
{
    if (cae_handle == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    if (angle_data == NULL || angle_cnt == NULL || *angle_cnt == 0U) {
        return ACOMP_ERR_INVALID_ARG;
    }

    uint32_t copy_cnt = cae_handle->angle_cnt;
    if (copy_cnt > *angle_cnt) {
        copy_cnt = *angle_cnt;
    }

    for (uint32_t i = 0; i < copy_cnt; i++) {
        angle_data[i] = cae_handle->angle_data[i];
    }
    *angle_cnt = copy_cnt;

    return ACOMP_ERR_OK;
}

int acomp_cae_add_callback(uint32_t events, cae_event_cb_t cb, void *priv)
{
    if (cae_handle == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }
    return gcl_cb_list_add_callback(cae_handle->event_callbacks, events, cb, priv);
}

int acomp_cae_remove_callback(cae_event_cb_t cb)
{
    if (cae_handle == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }
    return gcl_cb_list_remove_callback(cae_handle->event_callbacks, cb);
}

int acomp_cae_stream_ch_enable(int chn, acomp_stream_chn_create_desc_t *desc)
{
    int ret = 0;

    if (!acomp_cae_ready()) {
        return ACOMP_ERR_INVALID_STATE;
    }

    if (chn < 0 || chn >= ACOMP_STREAM_MAX_CHANNEL || desc == NULL) {
        return ACOMP_ERR_INVALID_ARG;
    }

    if (cae_handle->stream->ch[chn] != NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    cae_handle->stream->ch[chn] = acomp_stream_ipc_channel_create(cae_handle->stream, chn, cae_handle->dev_index, desc);
    if (cae_handle->stream->ch[chn] == NULL) {
        return ACOMP_ERR_CREATE_STREAM_FAILED;
    }
    LISA_LOGI(TAG, "acomp_cae_stream_ch_enable chn(%s) index(%d), desc(%p)", desc->cname, chn, desc);
    return ret;
}

int acomp_cae_stream_ch_disable(int chn)
{
    int ret;

    if (!acomp_cae_ready()) {
        return ACOMP_ERR_INVALID_STATE;
    }

    if (chn < 0 || chn >= ACOMP_STREAM_MAX_CHANNEL) {
        return ACOMP_ERR_INVALID_ARG;
    }

    ret = acomp_stream_ipc_channel_destroy(cae_handle->stream, cae_handle->dev_index, (uint32_t)chn);
    if (ret == ACOMP_ERR_OK) {
        cae_handle->stream->ch[chn] = NULL;
    }
    LISA_LOGI(TAG, "acomp_cae_stream_ch_disable chn index(%d), ret(%d)", chn, ret);
    return ret;
}

void *acomp_cae_stream_rx_buffer_get(int chn, uint32_t *len, uint16_t *desc_idx)
{
    if (!acomp_cae_ready() || chn < 0 || chn >= ACOMP_STREAM_MAX_CHANNEL ||
        cae_handle->stream->ch[chn] == NULL ||
        cae_handle->stream->ops.rx_buffer_get == NULL) {
        return NULL;
    }
    return cae_handle->stream->ops.rx_buffer_get(cae_handle->stream->ch[chn], len, desc_idx);
}

int acomp_cae_stream_rx_buffer_release(int chn, uint16_t desc_idx, uint32_t len, void *buffer)
{
    if (!acomp_cae_ready()) {
        return ACOMP_ERR_INVALID_STATE;
    }
    if (chn < 0 || chn >= ACOMP_STREAM_MAX_CHANNEL) {
        return ACOMP_ERR_INVALID_ARG;
    }
    if (cae_handle->stream->ch[chn] == NULL ||
        cae_handle->stream->ops.rx_buffer_release == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }
    return cae_handle->stream->ops.rx_buffer_release(cae_handle->stream->ch[chn], buffer, len, desc_idx);
}

void *acomp_cae_stream_tx_buffer_alloc(int chn, uint32_t *len, uint16_t *desc_idx)
{
    if (!acomp_cae_ready() || chn < 0 || chn >= ACOMP_STREAM_MAX_CHANNEL ||
        cae_handle->stream->ch[chn] == NULL ||
        cae_handle->stream->ops.tx_buffer_alloc == NULL) {
        return NULL;
    }
    return cae_handle->stream->ops.tx_buffer_alloc(cae_handle->stream->ch[chn], len, desc_idx);
}

int acomp_cae_stream_tx_buffer_submit(int chn, void *buffer, uint32_t len, uint16_t desc_idx)
{
    if (!acomp_cae_ready()) {
        return ACOMP_ERR_INVALID_STATE;
    }
    if (chn < 0 || chn >= ACOMP_STREAM_MAX_CHANNEL) {
        return ACOMP_ERR_INVALID_ARG;
    }
    if (cae_handle->stream->ch[chn] == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }
    if (cae_handle->stream->ops.tx_buffer_submit == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }
    return cae_handle->stream->ops.tx_buffer_submit(cae_handle->stream->ch[chn], buffer, len, desc_idx);
}
