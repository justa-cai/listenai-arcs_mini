#include <string.h>

#include "ipc/acomp_ipc.h"

#include "acomp_wsp.h"
#include "acomp_err.h"
#include "gcl_cb_list/gcl_cb_list.h"
#include "private/wsp_ipc.h"
#include "acomp_stream_ipc.h"

#define TAG "acomp_wsp"
#include "lisa_log.h"


#define ACOMP_WSP_DEV_NAME "acomp.wsp"


typedef struct {
    uint32_t dev_index;
    gcl_cb_list_t event_callbacks;
    acomp_stream_t *stream;
} acomp_wsp_handle_t;

static acomp_wsp_handle_t *wsp_handle = NULL;

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
        else if(message->acomp_cmd == ACOMP_IPC_CMD_NOTIFY_STREAM_UPDATE){

            if (((void *)message->address != NULL) && (message->len > 0)) {
                acomp_ipc_stream_update_t *ipc_msg;
                uint32_t chn;
                ipc_msg = (acomp_ipc_stream_update_t *)message->address;
                chn = ipc_msg->index;
                if(chn < sizeof(handle->stream->ch)/sizeof(handle->stream->ch[0])){
                    gcl_cb_event_dispatch(handle->event_callbacks, 
                        WSP_CB_EVENT_STREAM_UPDATE, 
                        handle->stream->ch[chn], 
                        sizeof(acomp_stream_channel_t));
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

    wsp_handle->stream = acomp_stream_create(NULL);
    if(wsp_handle->stream == NULL){

        return ACOMP_ERR_CREATE_STREAM_FAILED;
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

int acomp_wsp_stream_ch_enable(int chn,acomp_stream_chn_create_desc_t *desc){

    int ret = 0;
    
    if ((wsp_handle == NULL) || (wsp_handle->stream == NULL)) {
       
        return ACOMP_ERR_INVALID_STATE;
    }

    if(chn >= ACOMP_STREAM_MAX_CHANNEL){
        return ACOMP_ERR_INVALID_ARG;
    }

    wsp_handle->stream->ch[chn] = acomp_stream_ipc_channel_create(wsp_handle->stream,chn,wsp_handle->dev_index,desc);
    if(wsp_handle->stream->ch[chn] == NULL){
        return ACOMP_ERR_CREATE_STREAM_FAILED;
    }
    LISA_LOGI(TAG,"acomp_wsp_stream_ch_enable chn(%s) index(%d),desc(%p)",desc->cname,chn,desc);
    return ret;
}

int acomp_wsp_stream_ch_disable(int chn){
    int ret;
    if (wsp_handle == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    if(chn >= ACOMP_STREAM_MAX_CHANNEL){
        return ACOMP_ERR_INVALID_ARG;
    }

    ret = acomp_stream_ipc_channel_destroy(chn);
    LISA_LOGI(TAG,"acomp_wsp_stream_ch_disable chn index(%d),ret(%d)",chn,ret);
    wsp_handle->stream->ch[chn] = NULL;
    return ret;
}

void* acomp_wsp_stream_rx_buffer_get(int chn, uint32_t* len, uint16_t* desc_idx){
    
    uint8_t *ptr;
    
    if (wsp_handle == NULL) {
        return NULL;
    }

    if(chn >= ACOMP_STREAM_MAX_CHANNEL){
        return NULL;
    }

    if(wsp_handle->stream->ch[chn] == NULL){
        return NULL;
    }

    ptr = wsp_handle->stream->ops.rx_buffer_get(wsp_handle->stream->ch[chn], len, desc_idx);
    return ptr;
}

int acomp_wsp_stream_rx_buffer_release(int chn, uint16_t desc_idx, uint32_t len,void* buffer){
    int ret;

    if (wsp_handle == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    if(chn >= ACOMP_STREAM_MAX_CHANNEL){
        return ACOMP_ERR_INVALID_ARG;
    }

    ret =  wsp_handle->stream->ops.rx_buffer_release(wsp_handle->stream->ch[chn],buffer, len, desc_idx);

    return ret;
}

void* acomp_wsp_stream_tx_buffer_alloc(int chn, uint32_t* len, uint16_t* desc_idx){

    uint8_t* buffer;

    if (wsp_handle == NULL) {
        return NULL;
    }

    if(chn >= ACOMP_STREAM_MAX_CHANNEL){
        return NULL;
    }

    if(wsp_handle->stream->ch[chn] == NULL){
        return NULL;
    }

    buffer = wsp_handle->stream->ops.tx_buffer_alloc(wsp_handle->stream->ch[chn], len, desc_idx);

    return buffer;
}


int acomp_wsp_stream_tx_buffer_submit(int chn, void* buffer, uint32_t len, uint16_t desc_idx){
    int ret;

    if (wsp_handle == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    if(chn >= ACOMP_STREAM_MAX_CHANNEL){
        return ACOMP_ERR_INVALID_ARG;
    }

    if(wsp_handle->stream->ch[chn] == NULL){
        return ACOMP_ERR_INVALID_STATE;
    }

    ret = wsp_handle->stream->ops.tx_buffer_submit(wsp_handle->stream->ch[chn], buffer, len, desc_idx);

    return ret;
}

int acomp_virtqueue_dump(int chn)
{
    virtqueue_dump(wsp_handle->stream->ch[chn]->vq);
    return 0;
}