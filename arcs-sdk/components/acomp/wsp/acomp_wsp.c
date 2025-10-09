#include <string.h>

#include "ipc/acomp_ipc.h"

#include "acomp_wsp.h"
#include "acomp_err.h"
#include "gcl_cb_list/gcl_cb_list.h"
#include "private/wsp_ipc.h"

#define TAG "acomp_wsp"
#include "lisa_log.h"

#define ACOMP_WSP_DEV_NAME "acomp.wsp"
typedef struct {
    uint32_t dev_index;
    gcl_cb_list_t event_callbacks;
} acomp_wsp_handle_t;

acomp_wsp_handle_t *wsp_handle = NULL;

void event_callback(acomp_ipc_message_t *message, void *priv)
{

    acomp_wsp_handle_t *handle = (acomp_wsp_handle_t *)priv;
    if (handle == NULL) {
        return;
    }

    if (message->hdr.hdr.cmd == ACOMP_CONTEXT_IPC_GLB_NOTIFY) {

        if (message->acomp_cmd == ACOMP_IPC_CMD_NOTIFY_RESULT) {

            if (((void *)message->address != NULL) && (message->len > 0)) {

                acomp_ipc_notify_result_t *result = (acomp_ipc_notify_result_t *)message->address;
                gcl_cb_event_dispatch(handle->event_callbacks, WSP_CB_EVENT_ENGINE_RLT, result->data, result->len);
            }
        } else if (message->acomp_cmd == ACOMP_IPC_CMD_NOTIFY_SUBCMD) {

            if (((void *)message->address != NULL) && (message->len > 0)) {
      
                acomp_ipc_notify_subcmd_t *subcmd = (acomp_ipc_notify_subcmd_t *)message->address;
                wsp_ipc_notify_subcmd_hdr_t *hdr = (wsp_ipc_notify_subcmd_hdr_t *)subcmd->data;

                if ((hdr->cmd == WSP_IPC_NOTIFY_SUBCMD_VAD_BEGIN) || (hdr->cmd == WSP_IPC_NOTIFY_SUBCMD_VAD_END)) {
                    wsp_ipc_notify_subcmd_vad_t *vad = (wsp_ipc_notify_subcmd_vad_t *)subcmd->data;
                    if (vad->hdr.cmd == WSP_IPC_NOTIFY_SUBCMD_VAD_BEGIN) {
                        gcl_cb_event_dispatch(handle->event_callbacks, WSP_CB_EVENT_ENGINE_VAD_BEGIN, &vad->frame_idx,
                                              sizeof(vad->frame_idx));
                    } else if (vad->hdr.cmd == WSP_IPC_NOTIFY_SUBCMD_VAD_END) {
                        gcl_cb_event_dispatch(handle->event_callbacks, WSP_CB_EVENT_ENGINE_VAD_END, &vad->frame_idx,
                                              sizeof(vad->frame_idx));
                    }
                }
                else{
                    LISA_LOGW(TAG,"unknow notify hdr cmd:%d",hdr->cmd);
                }
            }
        }
    }
}

int acomp_wsp_init(void)
{
    int ret = 0;

    if (wsp_handle != NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    wsp_handle = (acomp_wsp_handle_t *)psram_malloc(sizeof(acomp_wsp_handle_t));
    if (wsp_handle == NULL) {
        return ACOMP_ERR_NO_MEM;
    }
    memset(wsp_handle, 0, sizeof(acomp_wsp_handle_t));
    wsp_handle->event_callbacks = gcl_cb_list_create();
    wsp_handle->dev_index = acomp_ipc_get_dev_index(ACOMP_WSP_DEV_NAME);

    if (wsp_handle->dev_index < 0) {
        psram_free(wsp_handle);
        wsp_handle = NULL;
        LISA_LOGE(TAG, "acomp wsp dev index not found!");
        return ACOMP_ERR_NOT_FOUND;
    }
    LISA_LOGI(TAG, "acomp wsp dev index %d,name:%s", wsp_handle->dev_index, ACOMP_WSP_DEV_NAME);

    ret = acomp_ipc_add_callback(wsp_handle->dev_index, (ipc_event_cb_t)event_callback, wsp_handle);
    if (ret != ACOMP_ERR_OK) {
        return ret;
    }

    ret = acomp_ipc_build_frame_send_sync(wsp_handle->dev_index, ACOMP_CONTEXT_IPC_GLB_NEW | IPC_HEADER_REQ_REPALY, 0,
                                          0, NULL, 0);
    if (ret != ACOMP_ERR_OK) {
        return ret;
    }

    return 0;
}

int acomp_wsp_deinit(void)
{
    /*TODO*/
    return ACOMP_ERR_NOT_SUPPORTED;
}

int acomp_wsp_prepare(void)
{
#define ACOMP_WSP_RES_NUMBER  (2)
#define WSP_INDEX_MLP_ENCODER (1)
#define WSP_INDEX_MLP_DECODER (2)
    acomp_ipc_prepare_t *prepare;
    uint32_t size;
    int ret;

    size = sizeof(acomp_ipc_prepare_t) + sizeof(acomp_res_item_t) * ACOMP_WSP_RES_NUMBER;
    prepare = psram_malloc_align(IPC_ALIGN_SIZE, size);
    if (prepare == NULL) {
        return ACOMP_ERR_NO_MEM;
    }
    memset(prepare, 0, sizeof(acomp_ipc_prepare_t));
    prepare->number = ACOMP_WSP_RES_NUMBER;
    prepare->item[0].index = WSP_INDEX_MLP_ENCODER;
    prepare->item[0].addr = CONFIG_ACOMP_WSP_RES_ENCODER_ADDRESS;
    prepare->item[0].offset = 0;
    prepare->item[0].size = CONFIG_ACOMP_WSP_RES_ENCODER_LENGTH;
    prepare->item[1].index = WSP_INDEX_MLP_DECODER;
    prepare->item[1].addr = CONFIG_ACOMP_WSP_RES_DECODER_ADDRESS;
    prepare->item[1].offset = 0;
    prepare->item[1].size = CONFIG_ACOMP_WSP_RES_DECODER_LENGTH;

    ret = acomp_ipc_build_frame_send_sync(wsp_handle->dev_index, ACOMP_CONTEXT_IPC_GLB_CONTROL | IPC_HEADER_REQ_REPALY,
                                          ACOMP_IPC_CMD_PREPARE, 0, prepare, size);

    psram_free(prepare);
    return ret;
}

int acomp_wsp_cleanup(void)
{

    int ret;
    ret = acomp_ipc_build_frame_send_sync(wsp_handle->dev_index, ACOMP_CONTEXT_IPC_GLB_CONTROL | IPC_HEADER_REQ_REPALY,
                                          ACOMP_IPC_CMD_CLEANUP, 0, NULL, 0);
    return ret;
}

int acomp_wsp_start(void)
{

    int ret;
    ret = acomp_ipc_build_frame_send_sync(wsp_handle->dev_index, ACOMP_CONTEXT_IPC_GLB_CONTROL | IPC_HEADER_REQ_REPALY,
                                          ACOMP_IPC_CMD_START, 0, NULL, 0);
    return ret;
}

int acomp_wsp_stop(void)
{

    int ret;
    ret = acomp_ipc_build_frame_send_sync(wsp_handle->dev_index, ACOMP_CONTEXT_IPC_GLB_CONTROL | IPC_HEADER_REQ_REPALY,
                                          ACOMP_IPC_CMD_STOP, 0, NULL, 0);
    return ret;
}

int acomp_wsp_params_set(acomp_wsp_params_t *params)
{
    int ret;

    ret = acomp_ipc_build_frame_send_sync(wsp_handle->dev_index, ACOMP_CONTEXT_IPC_GLB_CONTROL | IPC_HEADER_REQ_REPALY,
                                          ACOMP_IPC_CMD_CONTROL, 0, params, sizeof(acomp_wsp_params_t));
    return ret;
}

int acomp_wsp_add_callback(uint32_t events, wsp_event_cb_t cb, void *priv)
{

    int ret;
    if (wsp_handle == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    ret = gcl_cb_list_add_callback(wsp_handle->event_callbacks, events, cb, priv);
    return ret;
}

int acomp_wsp_remove_callback(wsp_event_cb_t cb)
{
    int ret;
    if (wsp_handle == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    ret = gcl_cb_list_remove(wsp_handle->event_callbacks, cb);
    return ret;
}
