#include <string.h>

#include "ipc/acomp_ipc.h"
#include "gcl_cb_list/gcl_cb_list.h"
#include "comm/stream/acomp_stream_ipc.h"

#include "acomp_err.h"
#include "private/palm_ipc.h"
#include "acomp_palm.h"

#define TAG "acomp_palm"
#include "lisa_log.h"

#define ACOMP_PALM_DEV_NAME "acomp.palm"
#define ALIGN_SIZE(len) (((len) + IPC_ALIGN_SIZE - 1U) / IPC_ALIGN_SIZE * IPC_ALIGN_SIZE)

typedef struct {
    int32_t dev_index;
    gcl_cb_list_t event_callbacks;
    acomp_stream_t *stream;
} acomp_palm_handle_t;

static acomp_palm_handle_t *palm_handle;

int acomp_palm_stream_ch_disable(int chn);

static int acomp_palm_res_item_valid(const acomp_palm_res_item_t *item)
{
    if (item == NULL || item->size == 0U) {
        return 0;
    }

    if (item->storage == ACOMP_PALM_RES_STORAGE_SD) {
        return 1;
    }

    return item->addr != 0U;
}

static int acomp_palm_channel_valid(int chn)
{
    return chn >= 0 && chn < ACOMP_STREAM_MAX_CHANNEL;
}

static void palm_event_callback(acomp_ipc_message_t *message, void *priv)
{
    acomp_palm_handle_t *handle = (acomp_palm_handle_t *)priv;

    if (handle == NULL || message == NULL) {
        return;
    }

    if (message->hdr.hdr.cmd != ACOMP_CONTEXT_IPC_GLB_NOTIFY ||
        message->acomp_cmd != ACOMP_IPC_CMD_NOTIFY_RESULT ||
        message->address == 0U || message->len == 0U) {
        return;
    }

    acomp_ipc_notify_result_t *result = (acomp_ipc_notify_result_t *)message->address;
    palm_ipc_notify_subcmd_palm_result_hdr_t *palm_result =
        (palm_ipc_notify_subcmd_palm_result_hdr_t *)result->data;

    gcl_cb_event_dispatch(handle->event_callbacks, PALM_CB_EVENT_ENGINE_RLT,
                          palm_result, result->len);
}

int acomp_palm_init(void)
{
    int ret;

    LISA_LOGI(TAG, "acomp_palm_init enter");

    if (palm_handle != NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    palm_handle = (acomp_palm_handle_t *)psram_malloc(sizeof(acomp_palm_handle_t));
    if (palm_handle == NULL) {
        return ACOMP_ERR_NO_MEM;
    }
    memset(palm_handle, 0, sizeof(*palm_handle));

    palm_handle->event_callbacks = gcl_cb_list_create();
    if (palm_handle->event_callbacks == NULL) {
        psram_free(palm_handle);
        palm_handle = NULL;
        return ACOMP_ERR_NO_MEM;
    }

    palm_handle->dev_index = acomp_ipc_get_dev_index(ACOMP_PALM_DEV_NAME);
    if (palm_handle->dev_index < 0) {
        gcl_cb_list_delete(palm_handle->event_callbacks);
        psram_free(palm_handle);
        palm_handle = NULL;
        LISA_LOGE(TAG, "device not found: %s", ACOMP_PALM_DEV_NAME);
        return ACOMP_ERR_NOT_FOUND;
    }

    ret = acomp_ipc_add_callback((uint32_t)palm_handle->dev_index,
                                 (ipc_event_cb_t)palm_event_callback,
                                 palm_handle);
    if (ret != ACOMP_ERR_OK) {
        gcl_cb_list_delete(palm_handle->event_callbacks);
        psram_free(palm_handle);
        palm_handle = NULL;
        return ret;
    }

    ret = acomp_ipc_build_frame_send_sync(palm_handle->dev_index,
                                          ACOMP_CONTEXT_IPC_GLB_NEW | IPC_HEADER_REQ_REPALY,
                                          0, 0, NULL, 0);
    if (ret != ACOMP_ERR_OK) {
        (void)acomp_ipc_remove_callback((uint32_t)palm_handle->dev_index,
                                        (ipc_event_cb_t)palm_event_callback);
        gcl_cb_list_delete(palm_handle->event_callbacks);
        psram_free(palm_handle);
        palm_handle = NULL;
        return ret;
    }

    palm_handle->stream = acomp_stream_create((uint32_t)palm_handle->dev_index);
    if (palm_handle->stream == NULL) {
        (void)acomp_ipc_remove_callback((uint32_t)palm_handle->dev_index,
                                        (ipc_event_cb_t)palm_event_callback);
        gcl_cb_list_delete(palm_handle->event_callbacks);
        psram_free(palm_handle);
        palm_handle = NULL;
        return ACOMP_ERR_CREATE_STREAM_FAILED;
    }

    LISA_LOGI(TAG, "acomp_palm_init exit");
    return ACOMP_ERR_OK;
}

int acomp_palm_deinit(void)
{
    if (palm_handle == NULL) {
        return ACOMP_ERR_OK;
    }

    for (int chn = 0; chn < ACOMP_STREAM_MAX_CHANNEL; chn++) {
        if (palm_handle->stream != NULL && palm_handle->stream->ch[chn] != NULL) {
            (void)acomp_palm_stream_ch_disable(chn);
        }
    }

    (void)acomp_ipc_remove_callback((uint32_t)palm_handle->dev_index,
                                    (ipc_event_cb_t)palm_event_callback);
    if (palm_handle->stream != NULL) {
        acomp_stream_destroy(palm_handle->stream);
        palm_handle->stream = NULL;
    }
    if (palm_handle->event_callbacks != NULL) {
        gcl_cb_list_delete(palm_handle->event_callbacks);
        palm_handle->event_callbacks = NULL;
    }

    psram_free(palm_handle);
    palm_handle = NULL;
    return ACOMP_ERR_OK;
}

static int acomp_palm_prepare_send(const acomp_palm_resource_config_t *config)
{
    acomp_ipc_prepare_t *prepare;
    uint32_t size;
    int ret;

    if (config == NULL) {
        return ACOMP_ERR_INVALID_ARG;
    }
    if (palm_handle == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    size = sizeof(acomp_ipc_prepare_t) + sizeof(acomp_res_item_t) * ACOMP_PALM_RES_NUMBER;
    size = ALIGN_SIZE(size);
    prepare = (acomp_ipc_prepare_t *)psram_malloc_align(IPC_ALIGN_SIZE, size);
    if (prepare == NULL) {
        return ACOMP_ERR_NO_MEM;
    }
    memset(prepare, 0, size);

    prepare->number = ACOMP_PALM_RES_NUMBER;
    prepare->item[0].index = RES_PALM_DETECT;
    prepare->item[0].attr.hdr.storage = config->detect.storage;
    prepare->item[0].addr = (uint32_t)config->detect.addr;
    prepare->item[0].offset = 0;
    prepare->item[0].size = config->detect.size;

    prepare->item[1].index = RES_PALM_VERIFY;
    prepare->item[1].attr.hdr.storage = config->verify.storage;
    prepare->item[1].addr = (uint32_t)config->verify.addr;
    prepare->item[1].offset = 0;
    prepare->item[1].size = config->verify.size;

    ret = acomp_ipc_build_frame_send_sync(palm_handle->dev_index,
                                          ACOMP_CONTEXT_IPC_GLB_CONTROL | IPC_HEADER_REQ_REPALY,
                                          ACOMP_IPC_CMD_PREPARE,
                                          0,
                                          prepare,
                                          size);
    psram_free(prepare);
    return ret;
}

int acomp_palm_prepare(void)
{
    const acomp_palm_resource_config_t config = {
        .detect = {
            .storage = ACOMP_PALM_RES_STORAGE_FLASH,
            .addr = CONFIG_ACOMP_PALM_RES_DETECT_ADDRESS,
            .size = CONFIG_ACOMP_PALM_RES_DETECT_LENGTH,
        },
        .verify = {
            .storage = ACOMP_PALM_RES_STORAGE_FLASH,
            .addr = CONFIG_ACOMP_PALM_RES_VERIFY_ADDRESS,
            .size = CONFIG_ACOMP_PALM_RES_VERIFY_LENGTH,
        },
    };

    return acomp_palm_prepare_with_resources(&config);
}

int acomp_palm_prepare_with_resources(const acomp_palm_resource_config_t *config)
{
    if (config == NULL || !acomp_palm_res_item_valid(&config->detect) ||
        !acomp_palm_res_item_valid(&config->verify)) {
        return ACOMP_ERR_INVALID_ARG;
    }

    return acomp_palm_prepare_send(config);
}

int acomp_palm_cleanup(void)
{
    if (palm_handle == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    return acomp_ipc_build_frame_send_sync(palm_handle->dev_index,
                                           ACOMP_CONTEXT_IPC_GLB_CONTROL | IPC_HEADER_REQ_REPALY,
                                           ACOMP_IPC_CMD_CLEANUP,
                                           0,
                                           NULL,
                                           0);
}

int acomp_palm_start(void)
{
    if (palm_handle == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    return acomp_ipc_build_frame_send_sync(palm_handle->dev_index,
                                           ACOMP_CONTEXT_IPC_GLB_CONTROL | IPC_HEADER_REQ_REPALY,
                                           ACOMP_IPC_CMD_START,
                                           0,
                                           NULL,
                                           0);
}

int acomp_palm_stop(void)
{
    if (palm_handle == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    return acomp_ipc_build_frame_send_sync(palm_handle->dev_index,
                                           ACOMP_CONTEXT_IPC_GLB_CONTROL | IPC_HEADER_REQ_REPALY,
                                           ACOMP_IPC_CMD_STOP,
                                           0,
                                           NULL,
                                           0);
}

static int acomp_palm_control_subcmd(palm_ipc_control_subcmd_e subcmd, void *data, uint32_t data_len)
{
    acomp_ipc_control_t *ipc_control;
    uint32_t size;
    int ret;

    if (palm_handle == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    size = ALIGN_SIZE(sizeof(acomp_ipc_control_t) + data_len);
    ipc_control = (acomp_ipc_control_t *)psram_malloc_align(IPC_ALIGN_SIZE, size);
    if (ipc_control == NULL) {
        return ACOMP_ERR_NO_MEM;
    }
    memset(ipc_control, 0, size);
    ipc_control->control = subcmd;
    ipc_control->len = data_len;
    if (data != NULL && data_len > 0U) {
        memcpy(ipc_control->data, data, data_len);
    }

    ret = acomp_ipc_build_frame_send_sync(palm_handle->dev_index,
                                          ACOMP_CONTEXT_IPC_GLB_CONTROL | IPC_HEADER_REQ_REPALY,
                                          ACOMP_IPC_CMD_CONTROL,
                                          0,
                                          ipc_control,
                                          size);
    psram_free(ipc_control);
    return ret;
}

int acomp_palm_params_set(const acomp_palm_param_t *params, uint32_t params_cnt)
{
    palm_ipc_control_subcmd_parameter_set_t *params_set;
    uint32_t len;
    int ret;

    if (params == NULL || params_cnt == 0U) {
        return ACOMP_ERR_INVALID_ARG;
    }

    len = sizeof(*params_set) + sizeof(acomp_palm_param_t) * params_cnt;
    params_set = (palm_ipc_control_subcmd_parameter_set_t *)psram_malloc(len);
    if (params_set == NULL) {
        return ACOMP_ERR_NO_MEM;
    }

    params_set->params_cnt = params_cnt;
    memcpy(params_set->data, params, sizeof(acomp_palm_param_t) * params_cnt);
    ret = acomp_palm_control_subcmd(PALM_IPC_CONTROL_SUBCMD_PARAMETER_SET, params_set, len);
    psram_free(params_set);
    return ret;
}

int acomp_palm_params_get(const acomp_palm_param_t *params, uint32_t *params_cnt)
{
    (void)params;
    (void)params_cnt;
    return ACOMP_ERR_NOT_SUPPORTED;
}

int acomp_palm_features_load(const acomp_palm_feature_result_t *features, uint32_t count)
{
    palm_ipc_control_subcmd_features_load_t *features_load;
    uint32_t len;
    int ret;

    if (features == NULL || count == 0U || count > ACOMP_PALM_MAX_RESULT_CNT) {
        return ACOMP_ERR_INVALID_ARG;
    }

    len = sizeof(*features_load) + sizeof(acomp_palm_feature_result_t) * count;
    features_load = (palm_ipc_control_subcmd_features_load_t *)psram_malloc(len);
    if (features_load == NULL) {
        return ACOMP_ERR_NO_MEM;
    }

    features_load->feature_cnt = count;
    memcpy(features_load->data, features, sizeof(acomp_palm_feature_result_t) * count);
    ret = acomp_palm_control_subcmd(PALM_IPC_CONTROL_SUBCMD_FEATURES_LOAD, features_load, len);
    psram_free(features_load);
    return ret;
}

int acomp_palm_add_callback(uint32_t events, palm_event_cb_t cb, void *priv)
{
    if (palm_handle == NULL || cb == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    return gcl_cb_list_add_callback(palm_handle->event_callbacks, events, cb, priv);
}

int acomp_palm_remove_callback(palm_event_cb_t cb)
{
    if (palm_handle == NULL || cb == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    return gcl_cb_list_remove_callback(palm_handle->event_callbacks, cb);
}

int acomp_palm_stream_ch_enable(int chn, acomp_stream_chn_create_desc_t *desc)
{
    if (palm_handle == NULL || palm_handle->stream == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }
    if (desc == NULL || !acomp_palm_channel_valid(chn)) {
        return ACOMP_ERR_INVALID_ARG;
    }

    palm_handle->stream->ch[chn] = acomp_stream_ipc_channel_create(palm_handle->stream,
                                                                   chn,
                                                                   (uint32_t)palm_handle->dev_index,
                                                                   desc);
    if (palm_handle->stream->ch[chn] == NULL) {
        return ACOMP_ERR_CREATE_STREAM_FAILED;
    }

    return ACOMP_ERR_OK;
}

int acomp_palm_stream_ch_disable(int chn)
{
    if (palm_handle == NULL || palm_handle->stream == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }
    if (!acomp_palm_channel_valid(chn)) {
        return ACOMP_ERR_INVALID_ARG;
    }

    return acomp_stream_ipc_channel_destroy(palm_handle->stream,
                                            (uint32_t)palm_handle->dev_index,
                                            (uint32_t)chn);
}

void *acomp_palm_stream_rx_buffer_get(int chn, uint32_t *len, uint16_t *desc_idx)
{
    if (palm_handle == NULL || palm_handle->stream == NULL || !acomp_palm_channel_valid(chn) ||
        palm_handle->stream->ch[chn] == NULL) {
        return NULL;
    }

    return palm_handle->stream->ops.rx_buffer_get(palm_handle->stream->ch[chn], len, desc_idx);
}

int acomp_palm_stream_rx_buffer_release(int chn, uint16_t desc_idx, uint32_t len, void *buffer)
{
    if (palm_handle == NULL || palm_handle->stream == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }
    if (!acomp_palm_channel_valid(chn) || palm_handle->stream->ch[chn] == NULL) {
        return ACOMP_ERR_INVALID_ARG;
    }

    return palm_handle->stream->ops.rx_buffer_release(palm_handle->stream->ch[chn], buffer, len, desc_idx);
}

void *acomp_palm_stream_tx_buffer_alloc(int chn, uint32_t *len, uint16_t *desc_idx)
{
    if (palm_handle == NULL || palm_handle->stream == NULL || !acomp_palm_channel_valid(chn) ||
        palm_handle->stream->ch[chn] == NULL) {
        return NULL;
    }

    return palm_handle->stream->ops.tx_buffer_alloc(palm_handle->stream->ch[chn], len, desc_idx);
}

int acomp_palm_stream_tx_buffer_submit(int chn, void *buffer, uint32_t len, uint16_t desc_idx)
{
    if (palm_handle == NULL || palm_handle->stream == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }
    if (!acomp_palm_channel_valid(chn) || palm_handle->stream->ch[chn] == NULL || buffer == NULL) {
        return ACOMP_ERR_INVALID_ARG;
    }

    return palm_handle->stream->ops.tx_buffer_submit(palm_handle->stream->ch[chn], buffer, len, desc_idx);
}
