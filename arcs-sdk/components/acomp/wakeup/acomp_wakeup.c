#include <string.h>

#include "ipc/acomp_ipc.h"

#include "acomp_wakeup.h"
#include "acomp_err.h"
#include "gcl_cb_list/gcl_cb_list.h"
#include "private/wakeup_ipc.h"
#include "comm/stream/acomp_stream_ipc.h"

#define TAG "acomp_wakeup"
#include "lisa_log.h"

#define ACOMP_WAKEUP_DEV_NAME "acomp.wakeup"

typedef struct {
    uint32_t dev_index;
    gcl_cb_list_t event_callbacks;
    acomp_stream_t *stream;
} acomp_wakeup_handle_t;

acomp_wakeup_handle_t *wakeup_handle = NULL;

static int acomp_wakeup_control_subcmd(wakeup_ipc_control_subcmd_e subcmd, void *data, uint32_t data_len);

void event_callback(acomp_ipc_message_t *message, void *priv)
{

    acomp_wakeup_handle_t *handle = (acomp_wakeup_handle_t *)priv;
    if (handle == NULL) {
        return;
    }

    if (message->hdr.hdr.cmd == ACOMP_CONTEXT_IPC_GLB_NOTIFY) {

        if (message->acomp_cmd == ACOMP_IPC_CMD_NOTIFY_RESULT) {

            if (((void *)message->address != NULL) && (message->len > 0)) {

                acomp_ipc_notify_result_t *result = (acomp_ipc_notify_result_t *)message->address;
                gcl_cb_event_dispatch(handle->event_callbacks, WAKEUP_CB_EVENT_ENGINE_RLT, result->data, result->len);
            }
        } else if (message->acomp_cmd == ACOMP_IPC_CMD_NOTIFY_SUBCMD) {

            if (((void *)message->address != NULL) && (message->len > 0)) {
      
                acomp_ipc_notify_subcmd_t *subcmd = (acomp_ipc_notify_subcmd_t *)message->address;
                wakeup_ipc_notify_subcmd_hdr_t *hdr = (wakeup_ipc_notify_subcmd_hdr_t *)subcmd->data;

                if (hdr->cmd == WAKEUP_IPC_NOTIFY_SUBCMD_PCM_DATA) {
                    wakeup_ipc_notify_subcmd_pcm_data_t *pcm_data = (wakeup_ipc_notify_subcmd_pcm_data_t *)subcmd->data;
                    gcl_cb_event_dispatch(handle->event_callbacks, WAKEUP_CB_EVENT_STREAM_UPDATE, pcm_data->data,
                                              pcm_data->len);

                } else if (hdr->cmd == WAKEUP_IPC_NOTIFY_SUBCMD_WAKEUP_TIMEOUT) {
                    gcl_cb_event_dispatch(handle->event_callbacks, WAKEUP_CB_EVENT_ENGINE_TIMEOUT, NULL,
                                          0);
                } else if (hdr->cmd == WAKEUP_IPC_NOTIFY_SUBCMD_ANGLE) {
                    wakeup_ipc_notify_subcmd_angle_t *angle = (wakeup_ipc_notify_subcmd_angle_t *)subcmd->data;
                    gcl_cb_event_dispatch(handle->event_callbacks, WAKEUP_CB_EVENT_ENGINE_ANGLE, angle->angle_data,
                                              angle->angle_cnt * sizeof(short));
                }else if(hdr->cmd == WAKEUP_IPC_NOTIFY_SUBCMD_MODE_SWITCH)
                {
                    wakeup_ipc_notify_subcmd_switch_mode_t *mode_switch = (wakeup_ipc_notify_subcmd_switch_mode_t *)subcmd->data;
                    acomp_wakeup_algo_mode_e mode;

                    if(mode_switch->mode == 0) {
                        mode = ACOMP_WAKEUP_ALGO_MODE_WAKEUP;
                    }
                    else {
                        mode = ACOMP_WAKEUP_ALGO_MODE_ESR;
                    }
                    gcl_cb_event_dispatch(handle->event_callbacks, WAKEUP_CB_EVENT_ENGINE_SWITCH_MODE, &mode,
                                              sizeof(acomp_wakeup_algo_mode_e));
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
                        WAKEUP_CB_EVENT_STREAM_UPDATE,
                        handle->stream->ch[chn],
                        sizeof(acomp_stream_channel_t));
                }
            }


        }
    }
}
#if 0
static int acomp_wakeup_default_params_set(void)
{
    LISA_LOGI(TAG, "acomp wakeup default params set enter");

    wakeup_ipc_control_subcmd_parameter_set_t params;
    memset(&params, 0, sizeof(wakeup_ipc_control_subcmd_parameter_set_t));
    params.mic_channel_num = CONFIG_ACOMP_WAKEUP_MIC_CHANNEL_NUM;
    params.mic_channel_index = CONFIG_ACOMP_WAKEUP_MIC_CHANNEL_INDEX;
    params.ref_type = CONFIG_ACOMP_WAKEUP_REF_TYPE;
    params.ref_channel_index = CONFIG_ACOMP_WAKEUP_REF_CHANNEL_INDEX;
    params.mic_l_gain_a_val = 0;
    params.mic_l_gain_d_val = 0;
    params.mic_r_gain_a_val = 0;
    params.mic_r_gain_d_val = 0;
    params.ref_gain_d_val = 0;
    params.ref_gain_a_val = 0;
    params.debug_enable = 0;

    int ret = acomp_wakeup_control_subcmd(WAKEUP_IPC_CONTROL_SUBCMD_PARAMETER_SET, &params, sizeof(acomp_wakeup_params_t));
    if (ret != ACOMP_ERR_OK) {  
        LISA_LOGE(TAG, "acomp wakeup default params set failed!");
        return ret;
    }

    LISA_LOGI(TAG, "acomp wakeup default params set exit");
    return ret;
}
#endif
int acomp_wakeup_init(void)
{
    int ret = 0;
    LISA_LOGI(TAG, "acomp wakeup init enter");

    if (wakeup_handle != NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    wakeup_handle = (acomp_wakeup_handle_t *)psram_malloc(sizeof(acomp_wakeup_handle_t));
    if (wakeup_handle == NULL) {
        return ACOMP_ERR_NO_MEM;
    }
    memset(wakeup_handle, 0, sizeof(acomp_wakeup_handle_t));
    wakeup_handle->event_callbacks = gcl_cb_list_create();
    wakeup_handle->dev_index = acomp_ipc_get_dev_index(ACOMP_WAKEUP_DEV_NAME);

    if (wakeup_handle->dev_index < 0) {
        psram_free(wakeup_handle);
        wakeup_handle = NULL;
        LISA_LOGE(TAG, "acomp wakeup dev index not found!");
        return ACOMP_ERR_NOT_FOUND;
    }
    LISA_LOGI(TAG, "acomp wakeup dev index %d,name:%s", wakeup_handle->dev_index, ACOMP_WAKEUP_DEV_NAME);

    ret = acomp_ipc_add_callback(wakeup_handle->dev_index, (ipc_event_cb_t)event_callback, wakeup_handle);
    if (ret != ACOMP_ERR_OK) {
        return ret;
    }

    ret = acomp_ipc_build_frame_send_sync(wakeup_handle->dev_index, ACOMP_CONTEXT_IPC_GLB_NEW | IPC_HEADER_REQ_REPALY, 0,
                                          0, NULL, 0);
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp wakeup init failed!");
        return ret;
    }

    wakeup_handle->stream = acomp_stream_create(wakeup_handle->dev_index);
    if(wakeup_handle->stream == NULL){

        return ACOMP_ERR_CREATE_STREAM_FAILED;
    }
    // ret = acomp_wakeup_default_params_set();

    LISA_LOGI(TAG, "acomp wakeup init exit");

    return 0;
}

int acomp_wakeup_deinit(void)
{
    /*TODO*/
    return ACOMP_ERR_NOT_SUPPORTED;
}

int acomp_wakeup_prepare(void)
{
    LISA_LOGI(TAG, "acomp wakeup prepare enter");

    acomp_ipc_prepare_t *prepare;
    uint32_t size;
    int ret;

    size = sizeof(acomp_ipc_prepare_t) + sizeof(acomp_res_item_t) * ACOMP_WAKEUP_RES_NUMBER;
    size = ALIGN_SIZE(size);

    prepare = psram_malloc_align(IPC_ALIGN_SIZE, size);
    if (prepare == NULL) {
        return ACOMP_ERR_NO_MEM;
    }

    prepare->number = ACOMP_WAKEUP_RES_NUMBER;
    prepare->item[0].index = WAKEUP_INDEX_CAE_ESR_MLP;
    prepare->item[0].addr = CONFIG_ACOMP_WAKEUP_RES_CAE_ESR_MLP_ADDRESS;
    prepare->item[0].offset = 0;
    prepare->item[0].size = CONFIG_ACOMP_WAKEUP_RES_CAE_ESR_MLP_LENGTH;

    prepare->item[1].index = WAKEUP_INDEX_AI_WRAP;
    prepare->item[1].addr = CONFIG_ACOMP_WAKEUP_RES_AI_WRAP_ADDRESS;
    prepare->item[1].offset = 0;
    prepare->item[1].size = CONFIG_ACOMP_WAKEUP_RES_AI_WRAP_LENGTH;

    ret = acomp_ipc_build_frame_send_sync(wakeup_handle->dev_index, ACOMP_CONTEXT_IPC_GLB_CONTROL | IPC_HEADER_REQ_REPALY,
                                          ACOMP_IPC_CMD_PREPARE, 0, prepare, size);
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp wakeup prepare failed!");
    }

    psram_free(prepare);

    LISA_LOGI(TAG, "acomp wakeup prepare exit");
    return ret;
}

int acomp_wakeup_prepare_with_config(acomp_ipc_prepare_t *prepare)
{
    LISA_LOGI(TAG, "acomp wakeup prepare enter");

    if (prepare == NULL) {
        return -1;
    }

    uint32_t size;
    int ret;

    size = sizeof(acomp_ipc_prepare_t) + sizeof(acomp_res_item_t) * ACOMP_WAKEUP_RES_NUMBER;
    size = ALIGN_SIZE(size);

    prepare->number = ACOMP_WAKEUP_RES_NUMBER;
    prepare->item[0].index = WAKEUP_INDEX_CAE_ESR_MLP;
    prepare->item[1].index = WAKEUP_INDEX_AI_WRAP;

    ret = acomp_ipc_build_frame_send_sync(wakeup_handle->dev_index, ACOMP_CONTEXT_IPC_GLB_CONTROL | IPC_HEADER_REQ_REPALY,
                                          ACOMP_IPC_CMD_PREPARE, 0, prepare, size);
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp wakeup prepare failed!");
    }

    LISA_LOGI(TAG, "acomp wakeup prepare exit");
    return ret;
}

int acomp_wakeup_cleanup(void)
{

    int ret;
    ret = acomp_ipc_build_frame_send_sync(wakeup_handle->dev_index, ACOMP_CONTEXT_IPC_GLB_CONTROL | IPC_HEADER_REQ_REPALY,
                                          ACOMP_IPC_CMD_CLEANUP, 0, NULL, 0);
    return ret;
}

int acomp_wakeup_start(void)
{
    LISA_LOGI(TAG, "acomp wakeup start enter");
    int ret;
    ret = acomp_ipc_build_frame_send_sync(wakeup_handle->dev_index, ACOMP_CONTEXT_IPC_GLB_CONTROL | IPC_HEADER_REQ_REPALY,
                                          ACOMP_IPC_CMD_START, 0, NULL, 0);
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp wakeup start failed!");
    }

    LISA_LOGI(TAG, "acomp wakeup start exit");
    return ret;
}

int acomp_wakeup_stop(void)
{
    LISA_LOGI(TAG, "acomp wakeup stop enter");
    int ret;
    ret = acomp_ipc_build_frame_send_sync(wakeup_handle->dev_index, ACOMP_CONTEXT_IPC_GLB_CONTROL | IPC_HEADER_REQ_REPALY,
                                          ACOMP_IPC_CMD_STOP, 0, NULL, 0);
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp wakeup stop failed!");
    }

    LISA_LOGI(TAG, "acomp wakeup stop exit");
    return ret;
}

static int acomp_wakeup_control_subcmd(wakeup_ipc_control_subcmd_e subcmd, void *data, uint32_t data_len)
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

    ret = acomp_ipc_build_frame_send_sync(wakeup_handle->dev_index, ACOMP_CONTEXT_IPC_GLB_CONTROL | IPC_HEADER_REQ_REPALY,
                                            ACOMP_IPC_CMD_CONTROL, 0, ipc_control, size);

    psram_free(ipc_control);

    return ret;
}

// int acomp_wakeup_params_set(acomp_wakeup_params_t *params)
// {
//     LISA_LOGI(TAG, "acomp wakeup params set enter");

//     LISA_LOGI(TAG, "acomp wakeup params set exit");
//     return 0;
// }

int acomp_wakeup_set_debug_mode(uint8_t enable)
{
    LISA_LOGI(TAG, "acomp wakeup set debug mode enter");

    wakeup_ipc_control_subcmd_debug_mode_set_t debug_mode_set;
    debug_mode_set.enable = enable;

    int ret = acomp_wakeup_control_subcmd(WAKEUP_IPC_CONTROL_SUDCMD_DEBUG_MODE_SET, &debug_mode_set, sizeof(wakeup_ipc_control_subcmd_debug_mode_set_t));
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp wakeup set debug mode failed!");
    }

    LISA_LOGI(TAG, "acomp wakeup set debug mode exit");
    return ret;
}

int acomp_wakeup_set_algo_mode(acomp_wakeup_algo_mode_e mode)
{
    LISA_LOGI(TAG, "acomp wakeup set algo mode enter, mode=%d", mode);

    if (mode != ACOMP_WAKEUP_ALGO_MODE_WAKEUP && mode != ACOMP_WAKEUP_ALGO_MODE_ESR) {
        LISA_LOGE(TAG, "invalid algo mode: %d", mode);
        return ACOMP_ERR_INVALID_ARG;
    }

    wakeup_ipc_control_subcmd_algo_mode_set_t algo_mode_set;
    algo_mode_set.mode = (uint8_t)mode;

    int ret = acomp_wakeup_control_subcmd(WAKEUP_IPC_CONTROL_SUBCMD_ALGO_MODE_SET, &algo_mode_set, sizeof(wakeup_ipc_control_subcmd_algo_mode_set_t));
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp wakeup set algo mode failed!");
    }

    LISA_LOGI(TAG, "acomp wakeup set algo mode exit");
    return ret;
}

int acomp_wakeup_set_threshold(acomp_wakeup_threshold_level_e level)
{
    LISA_LOGI(TAG, "acomp wakeup set threshold enter, level=%d", level);

    if (level < ACOMP_WAKEUP_THRESHOLD_LEVEL_1 || level > ACOMP_WAKEUP_THRESHOLD_LEVEL_6) {
        LISA_LOGE(TAG, "invalid threshold level: %d", level);
        return ACOMP_ERR_INVALID_ARG;
    }

    wakeup_ipc_control_subcmd_threshold_set_t threshold_set;
    threshold_set.level = (uint8_t)level;

    int ret = acomp_wakeup_control_subcmd(WAKEUP_IPC_CONTROL_SUBCMD_THRESHOLD_SET, &threshold_set, sizeof(wakeup_ipc_control_subcmd_threshold_set_t));
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp wakeup set threshold failed!");
    }

    LISA_LOGI(TAG, "acomp wakeup set threshold exit");
    return ret;
}

int acomp_wakeup_add_callback(uint32_t events, wakeup_event_cb_t cb, void *priv)
{

    int ret;
    if (wakeup_handle == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    ret = gcl_cb_list_add_callback(wakeup_handle->event_callbacks, events, cb, priv);
    return ret;
}

int acomp_wakeup_remove_callback(wakeup_event_cb_t cb)
{
    int ret;
    if (wakeup_handle == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    ret = gcl_cb_list_remove(wakeup_handle->event_callbacks, cb);
    return ret;
}

int acomp_wakeup_stream_ch_enable(int chn,acomp_stream_chn_create_desc_t *desc){

    int ret = 0;

    if ((wakeup_handle == NULL) || (wakeup_handle->stream == NULL)) {

        return ACOMP_ERR_INVALID_STATE;
    }

    if(chn >= ACOMP_STREAM_MAX_CHANNEL){
        return ACOMP_ERR_INVALID_ARG;
    }

    wakeup_handle->stream->ch[chn] = acomp_stream_ipc_channel_create(wakeup_handle->stream,chn,wakeup_handle->dev_index,desc);
    if(wakeup_handle->stream->ch[chn] == NULL){
        return ACOMP_ERR_CREATE_STREAM_FAILED;
    }
    LISA_LOGI(TAG,"acomp_wakeup_stream_ch_enable chn(%s) index(%d),desc(%p)",desc->cname,chn,desc);
    return ret;
}

int acomp_wakeup_stream_ch_disable(int chn){
    int ret;
    if (wakeup_handle == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    if(chn >= ACOMP_STREAM_MAX_CHANNEL){
        return ACOMP_ERR_INVALID_ARG;
    }

    ret = acomp_stream_ipc_channel_destroy(chn);
    LISA_LOGI(TAG,"acomp_wakeup_stream_ch_disable chn index(%d),ret(%d)",chn,ret);
    wakeup_handle->stream->ch[chn] = NULL;
    return ret;
}

void* acomp_wakeup_stream_rx_buffer_get(int chn, uint32_t* len, uint16_t* desc_idx){

    uint8_t *ptr;

    if (wakeup_handle == NULL) {
        return NULL;
    }

    if(chn >= ACOMP_STREAM_MAX_CHANNEL){
        return NULL;
    }

    if(wakeup_handle->stream->ch[chn] == NULL){
        return NULL;
    }

    ptr = wakeup_handle->stream->ops.rx_buffer_get(wakeup_handle->stream->ch[chn], len, desc_idx);
    return ptr;
}

int acomp_wakeup_stream_rx_buffer_release(int chn, uint16_t desc_idx, uint32_t len,void* buffer){
    int ret;

    if (wakeup_handle == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    if(chn >= ACOMP_STREAM_MAX_CHANNEL){
        return ACOMP_ERR_INVALID_ARG;
    }

    ret =  wakeup_handle->stream->ops.rx_buffer_release(wakeup_handle->stream->ch[chn],buffer, len, desc_idx);

    return ret;
}

void* acomp_wakeup_stream_tx_buffer_alloc(int chn, uint32_t* len, uint16_t* desc_idx){

    uint8_t* buffer;

    if (wakeup_handle == NULL) {
        return NULL;
    }

    if(chn >= ACOMP_STREAM_MAX_CHANNEL){
        return NULL;
    }

    if(wakeup_handle->stream->ch[chn] == NULL){
        return NULL;
    }

    buffer = wakeup_handle->stream->ops.tx_buffer_alloc(wakeup_handle->stream->ch[chn], len, desc_idx);

    return buffer;
}


int acomp_wakeup_stream_tx_buffer_submit(int chn, void* buffer, uint32_t len, uint16_t desc_idx){
    int ret;

    if (wakeup_handle == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    if(chn >= ACOMP_STREAM_MAX_CHANNEL){
        return ACOMP_ERR_INVALID_ARG;
    }

    if(wakeup_handle->stream->ch[chn] == NULL){
        return ACOMP_ERR_INVALID_STATE;
    }

    ret = wakeup_handle->stream->ops.tx_buffer_submit(wakeup_handle->stream->ch[chn], buffer, len, desc_idx);

    return ret;
}
