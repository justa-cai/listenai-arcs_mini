#include <string.h>

#include "ipc/acomp_ipc.h"

#include "acomp_translation.h"
#include "acomp_err.h"
#include "gcl_cb_list/gcl_cb_list.h"
#include "private/trans_ipc.h"

#define TAG "acomp_translation"
#include "lisa_log.h"

#define ACOMP_TRANSLATION_DEV_NAME "acomp.trans"

typedef struct {
    uint32_t dev_index;
    gcl_cb_list_t event_callbacks;
} acomp_translation_handle_t;

static acomp_translation_handle_t *trans_handle = NULL;

static int trans_control_subcmd(trans_ipc_control_subcmd_e subcmd, void *data, uint32_t data_len);

static void trans_event_callback(acomp_ipc_message_t *message, void *priv)
{
    acomp_translation_handle_t *handle = (acomp_translation_handle_t *)priv;
    if (handle == NULL) {
        return;
    }

    if (message->hdr.hdr.cmd == ACOMP_CONTEXT_IPC_GLB_NOTIFY) {

        if (message->acomp_cmd == ACOMP_IPC_CMD_NOTIFY_RESULT) {

            if (((void *)message->address != NULL) && (message->len > 0)) {

                acomp_ipc_notify_result_t *result = (acomp_ipc_notify_result_t *)message->address;
                gcl_cb_event_dispatch(handle->event_callbacks, TRANS_CB_EVENT_RESULT, result->data, result->len);
            }
        } else if (message->acomp_cmd == ACOMP_IPC_CMD_NOTIFY_SUBCMD) {

            if (((void *)message->address != NULL) && (message->len > 0)) {

                acomp_ipc_notify_subcmd_t *subcmd = (acomp_ipc_notify_subcmd_t *)message->address;
                trans_ipc_notify_subcmd_hdr_t *hdr = (trans_ipc_notify_subcmd_hdr_t *)subcmd->data;

                if (hdr->cmd == TRANS_IPC_NOTIFY_SUBCMD_TRANS_STATUS) {
                    trans_ipc_notify_subcmd_status_t *status = (trans_ipc_notify_subcmd_status_t *)subcmd->data;
                    gcl_cb_event_dispatch(handle->event_callbacks, TRANS_CB_EVENT_STATUS, &status->status,
                                          sizeof(status->status));
                } else {
                    LISA_LOGW(TAG, "unknow notify hdr cmd:%d", hdr->cmd);
                }
            }
        }
    }
}

int acomp_translation_init(void)
{
    int ret = 0;
    LISA_LOGI(TAG, "acomp translation init enter");

    if (trans_handle != NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    trans_handle = (acomp_translation_handle_t *)psram_malloc(sizeof(acomp_translation_handle_t));
    if (trans_handle == NULL) {
        return ACOMP_ERR_NO_MEM;
    }
    memset(trans_handle, 0, sizeof(acomp_translation_handle_t));
    trans_handle->event_callbacks = gcl_cb_list_create();

    int dev_index = acomp_ipc_get_dev_index(ACOMP_TRANSLATION_DEV_NAME);
    if (dev_index < 0) {
        psram_free(trans_handle);
        trans_handle = NULL;
        LISA_LOGE(TAG, "acomp translation dev index not found!");
        return ACOMP_ERR_NOT_FOUND;
    }
    trans_handle->dev_index = (uint32_t)dev_index;
    LISA_LOGI(TAG, "acomp translation dev index %d,name:%s", trans_handle->dev_index, ACOMP_TRANSLATION_DEV_NAME);

    ret = acomp_ipc_add_callback(trans_handle->dev_index, (ipc_event_cb_t)trans_event_callback, trans_handle);
    if (ret != ACOMP_ERR_OK) {
        return ret;
    }

    ret = acomp_ipc_build_frame_send_sync(trans_handle->dev_index, ACOMP_CONTEXT_IPC_GLB_NEW | IPC_HEADER_REQ_REPALY, 0,
                                          0, NULL, 0);
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp translation init failed!");
        return ret;
    }

    LISA_LOGI(TAG, "acomp translation init exit");

    return 0;
}

int acomp_translation_prepare(acomp_ipc_prepare_t *prepare)
{
    LISA_LOGI(TAG, "acomp translation prepare enter");

    if (prepare == NULL) {
        return ACOMP_ERR_INVALID_ARG;
    }

    uint32_t size;
    int ret;

    size = sizeof(acomp_ipc_prepare_t) + sizeof(acomp_res_item_t) * prepare->number;
    size = ALIGN_SIZE(size);

    ret = acomp_ipc_build_frame_send_sync(trans_handle->dev_index, ACOMP_CONTEXT_IPC_GLB_CONTROL | IPC_HEADER_REQ_REPALY,
                                          ACOMP_IPC_CMD_PREPARE, 0, prepare, size);
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp translation prepare failed!");
    }

    LISA_LOGI(TAG, "acomp translation prepare exit");
    return ret;
}

int acomp_translation_cleanup(void)
{
    int ret = 0;

    if (trans_handle != NULL) {
        ret = acomp_ipc_build_frame_send_sync(trans_handle->dev_index,
                                              ACOMP_CONTEXT_IPC_GLB_CONTROL | IPC_HEADER_REQ_REPALY,
                                              ACOMP_IPC_CMD_CLEANUP, 0, NULL, 0);
        acomp_ipc_remove_callback(trans_handle->dev_index, (ipc_event_cb_t)trans_event_callback);
        if (trans_handle->event_callbacks != NULL) {
            gcl_cb_list_delete(trans_handle->event_callbacks);
            trans_handle->event_callbacks = NULL;
        }
        psram_free(trans_handle);
        trans_handle = NULL;
    }

    return ret;
}

int acomp_translation_start(void)
{
    LISA_LOGI(TAG, "acomp translation start enter");
    int ret;
    ret = acomp_ipc_build_frame_send_sync(trans_handle->dev_index, ACOMP_CONTEXT_IPC_GLB_CONTROL | IPC_HEADER_REQ_REPALY,
                                          ACOMP_IPC_CMD_START, 0, NULL, 0);
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp translation start failed!");
    }

    LISA_LOGI(TAG, "acomp translation start exit");
    return ret;
}

int acomp_translation_stop(void)
{
    LISA_LOGI(TAG, "acomp translation stop enter");
    int ret;
    ret = acomp_ipc_build_frame_send_sync(trans_handle->dev_index, ACOMP_CONTEXT_IPC_GLB_CONTROL | IPC_HEADER_REQ_REPALY,
                                          ACOMP_IPC_CMD_ABORT, 0, NULL, 0);
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp translation stop failed!");
    }

    LISA_LOGI(TAG, "acomp translation stop exit");
    return ret;
}

static int trans_control_subcmd(trans_ipc_control_subcmd_e subcmd, void *data, uint32_t data_len)
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

    ret = acomp_ipc_build_frame_send_sync(trans_handle->dev_index, ACOMP_CONTEXT_IPC_GLB_CONTROL | IPC_HEADER_REQ_REPALY,
                                            ACOMP_IPC_CMD_CONTROL, 0, ipc_control, size);

    psram_free(ipc_control);

    return ret;
}

int acomp_translation_translate(const char *text, uint32_t len)
{
    LISA_LOGI(TAG, "acomp translation translate enter, len=%u", len);

    if (text == NULL || len == 0) {
        return ACOMP_ERR_INVALID_ARG;
    }

    int ret;
    uint32_t data_len;
    uint32_t size;
    acomp_ipc_control_t *ipc_control;

    data_len = sizeof(trans_ipc_control_subcmd_translate_t) + len;
    size = sizeof(acomp_ipc_control_t) + data_len;
    size = ALIGN_SIZE(size);

    ipc_control = psram_malloc_align(IPC_ALIGN_SIZE, size);
    if (ipc_control == NULL) {
        return ACOMP_ERR_NO_MEM;
    }

    ipc_control->control = TRANS_IPC_CONTROL_SUBCMD_TRANSLATE;
    ipc_control->len = data_len;

    trans_ipc_control_subcmd_translate_t *translate = (trans_ipc_control_subcmd_translate_t *)ipc_control->data;
    translate->len = len;
    memcpy(translate->text, text, len);

    ret = acomp_ipc_build_frame_send_sync(trans_handle->dev_index, ACOMP_CONTEXT_IPC_GLB_CONTROL | IPC_HEADER_REQ_REPALY,
                                            ACOMP_IPC_CMD_CONTROL, 0, ipc_control, size);

    psram_free(ipc_control);

    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp translation translate failed!");
    }

    LISA_LOGI(TAG, "acomp translation translate exit");
    return ret;
}

int acomp_translation_set_res_type(int type)
{
    LISA_LOGI(TAG, "acomp translation set res type enter, type=%d", type);

    trans_ipc_control_subcmd_set_res_type_t res_type_set;
    res_type_set.type = type;

    int ret = trans_control_subcmd(TRANS_IPC_CONTROL_SUBCMD_SET_RES_TYPE, &res_type_set, sizeof(trans_ipc_control_subcmd_set_res_type_t));
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp translation set res type failed!");
    }

    LISA_LOGI(TAG, "acomp translation set res type exit");
    return ret;
}

int acomp_translation_add_callback(uint32_t events, trans_event_cb_t cb, void *priv)
{
    int ret;
    if (trans_handle == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    ret = gcl_cb_list_add_callback(trans_handle->event_callbacks, events, cb, priv);
    return ret;
}

int acomp_translation_remove_callback(trans_event_cb_t cb)
{
    int ret;
    if (trans_handle == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    ret = gcl_cb_list_remove_callback(trans_handle->event_callbacks, cb);
    return ret;
}
