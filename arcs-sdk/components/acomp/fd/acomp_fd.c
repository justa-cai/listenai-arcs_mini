#include <string.h>

#include "crc32.h"

#include "ipc/acomp_ipc.h"
#include "gcl_cb_list/gcl_cb_list.h"
#include "comm/stream/acomp_stream_ipc.h"

#include "acomp_err.h"
#include "private/fd_ipc.h"
#include "acomp_fd.h"

#define TAG "acomp_fd"
#include "lisa_log.h"

#define ACOMP_FD_DEV_NAME "acomp.fd"
#define ALIGN_SIZE(len) ((len + IPC_ALIGN_SIZE - 1) / IPC_ALIGN_SIZE * IPC_ALIGN_SIZE) 

typedef struct {
    uint32_t dev_index;
    gcl_cb_list_t event_callbacks;
    // acomp_fd_result_info_t fd_info;
    acomp_stream_t *stream;
} acomp_fd_handle_t;

acomp_fd_handle_t *fd_handle = NULL;

void event_callback(acomp_ipc_message_t *message, void *priv)
{

    acomp_fd_handle_t *handle = (acomp_fd_handle_t *)priv;
    if (handle == NULL) {
        return;
    }

    if (message->hdr.hdr.cmd == ACOMP_CONTEXT_IPC_GLB_NOTIFY) {

        if (message->acomp_cmd == ACOMP_IPC_CMD_NOTIFY_RESULT) {

            if (((void *)message->address != NULL) && (message->len > 0)) {

                acomp_ipc_notify_result_t *result = (acomp_ipc_notify_result_t *)message->address;

                fd_ipc_notify_subcmd_fd_result_hdr_t *fd_result = (fd_ipc_notify_subcmd_fd_result_hdr_t *)result->data;
                if (fd_result->results_cnt >= 0) {
                    // fd_handle->fd_info.results_cnt = fd_result->results_cnt;
                    // fd_handle->fd_info.max_area_results_index = fd_result->max_area_results_index;
                    // fd_handle->fd_info.results = (acomp_fd_result_t *)fd_result->results;
                    gcl_cb_event_dispatch(handle->event_callbacks, FD_CB_EVENT_ENGINE_RLT, fd_result, result->len);
                }
            }
        }
    }
}

int acomp_fd_init(void)
{
    LISA_LOGI(TAG, "acomp fd init enter");
    int ret = 0;

    if (fd_handle != NULL) {
        LISA_LOGI(TAG, "acomp fd_handle already exit");
        return ACOMP_ERR_INVALID_STATE;
    }

    fd_handle = (acomp_fd_handle_t *)psram_malloc(sizeof(acomp_fd_handle_t));
    if (fd_handle == NULL) {
        LISA_LOGE(TAG, "acomp fd init failed! no mem");
        return ACOMP_ERR_NO_MEM;
    }
    memset(fd_handle, 0, sizeof(acomp_fd_handle_t));
    fd_handle->event_callbacks = gcl_cb_list_create();
    fd_handle->dev_index = acomp_ipc_get_dev_index(ACOMP_FD_DEV_NAME);
    LISA_LOGI(TAG, "acomp fd dev index %d,name:%s", fd_handle->dev_index, ACOMP_FD_DEV_NAME);
    if (fd_handle->dev_index < 0) {
        psram_free(fd_handle);
        fd_handle = NULL;
        LISA_LOGE(TAG, "acomp fd dev index not found!");
        return ACOMP_ERR_NOT_FOUND;
    }
    LISA_LOGI(TAG, "acomp fd dev index %d,name:%s", fd_handle->dev_index, ACOMP_FD_DEV_NAME);

    ret = acomp_ipc_add_callback(fd_handle->dev_index, (ipc_event_cb_t)event_callback, fd_handle);
    if (ret != ACOMP_ERR_OK) {
        return ret;
    }

    ret = acomp_ipc_build_frame_send_sync(fd_handle->dev_index, ACOMP_CONTEXT_IPC_GLB_NEW | IPC_HEADER_REQ_REPALY, 0,
                                          0, NULL, 0);
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp fd init failed! ret:%d", ret);
        return ret;
    }

    fd_handle->stream = acomp_stream_create(fd_handle->dev_index);
    if(fd_handle->stream == NULL){

        return ACOMP_ERR_CREATE_STREAM_FAILED;
    }

    LISA_LOGI(TAG, "acomp fd init exit");

    return 0;
}

int acomp_fd_deinit(void)
{
    /*TODO*/
    return ACOMP_ERR_NOT_SUPPORTED;
}

int acomp_fd_prepare(void)
{
    LISA_LOGI(TAG, "acomp fd prepare enter");

    acomp_ipc_prepare_t *prepare;
    uint32_t size;
    int ret;

    size = sizeof(acomp_ipc_prepare_t) + sizeof(acomp_res_item_t) * ACOMP_FD_RES_NUMBER;
    size = ALIGN_SIZE(size);

    prepare = psram_malloc_align(IPC_ALIGN_SIZE, size);
    if (prepare == NULL) {
        LISA_LOGE(TAG, "acomp fd prepare failed! no mem");
        return ACOMP_ERR_NO_MEM;
    }
    memset(prepare, 0, size);
    prepare->number = ACOMP_FD_RES_NUMBER;
    prepare->item[0].index = RES_FACE_DETECT;
    prepare->item[0].attr.hdr.storage = 0;
    prepare->item[0].addr = CONFIG_ACOMP_FD_RES_FACE_DETECT_ADDRESS;
    prepare->item[0].offset = 0;
    prepare->item[0].size = CONFIG_ACOMP_FD_RES_FACE_DETECT_LENGTH;

    prepare->item[1].index = RES_FACE_ALIGN;
    prepare->item[1].attr.hdr.storage = 0;
    prepare->item[1].addr = CONFIG_ACOMP_FD_RES_FACE_ALIGN_ADDRESS;
    prepare->item[1].offset = 0;
    prepare->item[1].size = CONFIG_ACOMP_FD_RES_FACE_ALIGN_LENGTH;

    prepare->item[2].index = RES_FACE_LIVE;
    prepare->item[2].attr.hdr.storage = 0;
    prepare->item[2].addr = CONFIG_ACOMP_FD_RES_FACE_LIVE_ADDRESS;
    prepare->item[2].offset = 0;
    prepare->item[2].size = CONFIG_ACOMP_FD_RES_FACE_LIVE_LENGTH;

    prepare->item[3].index = RES_FACE_VERIFY;
    prepare->item[3].attr.hdr.storage = 0;
    prepare->item[3].addr = CONFIG_ACOMP_FD_FACE_VERIFY_ADDRESS;
    prepare->item[3].offset = 0;
    prepare->item[3].size = CONFIG_ACOMP_FD_FACE_VERIFY_LENGTH;

    LISA_LOGI(TAG, "acomp fd prepare:%p, size:%u", prepare, size);
    ret = acomp_ipc_build_frame_send_sync(fd_handle->dev_index, ACOMP_CONTEXT_IPC_GLB_CONTROL | IPC_HEADER_REQ_REPALY,
                                          ACOMP_IPC_CMD_PREPARE, 0, prepare, size);
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp fd prepare failed! ret:%d", ret);
        return ret;
    }

    psram_free(prepare);

    LISA_LOGI(TAG, "acomp fd prepare exit");
    return ret;
}

int acomp_fd_cleanup(void)
{

    int ret;
    ret = acomp_ipc_build_frame_send_sync(fd_handle->dev_index, ACOMP_CONTEXT_IPC_GLB_CONTROL | IPC_HEADER_REQ_REPALY,
                                          ACOMP_IPC_CMD_CLEANUP, 0, NULL, 0);
    return ret;
}

int acomp_fd_start(void)
{
    LISA_LOGI(TAG, "acomp fd start enter");
    int ret;
    ret = acomp_ipc_build_frame_send_sync(fd_handle->dev_index, ACOMP_CONTEXT_IPC_GLB_CONTROL | IPC_HEADER_REQ_REPALY,
                                          ACOMP_IPC_CMD_START, 0, NULL, 0);
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp fd start failed! ret:%d", ret);
        return ret;
    }
    
    LISA_LOGI(TAG, "acomp fd start exit");
    return ret;
}

int acomp_fd_stop(void)
{

    int ret;
    ret = acomp_ipc_build_frame_send_sync(fd_handle->dev_index, ACOMP_CONTEXT_IPC_GLB_CONTROL | IPC_HEADER_REQ_REPALY,
                                          ACOMP_IPC_CMD_STOP, 0, NULL, 0);
    return ret;
}

static int acomp_fd_control_subcmd(fd_ipc_control_subcmd_e subcmd, void *data, uint32_t data_len)
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

    ret = acomp_ipc_build_frame_send_sync(fd_handle->dev_index, ACOMP_CONTEXT_IPC_GLB_CONTROL | IPC_HEADER_REQ_REPALY,
                                            ACOMP_IPC_CMD_CONTROL, 0, ipc_control, size);

    psram_free(ipc_control);

    return ret;
}

int acomp_fd_params_set(const acomp_fd_param_t *params, uint32_t params_cnt)
{
    LISA_LOGI(TAG, "acomp_fd_params_set enter");

    int len = sizeof(fd_ipc_control_subcmd_parameter_set_t) + sizeof(acomp_fd_param_t) * params_cnt;
    fd_ipc_control_subcmd_parameter_set_t *params_set = (fd_ipc_control_subcmd_parameter_set_t *)psram_malloc(len);
    if (params_set == NULL) {
        return ACOMP_ERR_NO_MEM;
    }

    params_set->params_cnt = params_cnt;
    memcpy((uint8_t*)params_set->data, (uint8_t*)params, sizeof(acomp_fd_param_t) * params_cnt);

    int ret = acomp_fd_control_subcmd(FD_ICP_CONTROL_SUBCMD_PARAMETER_SET, params_set, len);
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp_fd_params_set failed!");
    }

    psram_free(params_set);

    LISA_LOGI(TAG, "acomp_fd_params_set exit");
    return ret;
}

int acomp_fd_align_threshold_set(const acomp_fd_head_pose_t *threshold)
{
    LISA_LOGI(TAG, "acomp_fd_align_threshold_set enter");

    fd_ipc_control_subcmd_align_threshold_set_t align_threshold_set;
    align_threshold_set.head_pose.yaw = threshold->yaw;
    align_threshold_set.head_pose.pitch = threshold->pitch;
    align_threshold_set.head_pose.roll = threshold->roll;

    int ret = acomp_fd_control_subcmd(FD_ICP_CONTROL_SUBCMD_ALIGN_THRESHOLD_SET, &align_threshold_set, sizeof(fd_ipc_control_subcmd_align_threshold_set_t));
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp_fd_align_threshold_set failed!");
    }

    LISA_LOGI(TAG, "acomp_fd_align_threshold_set exit");
    return ret;
}

int acomp_fd_live_detect_mode_set(const acomp_fd_live_detect_mode_t *mode)
{
    LISA_LOGI(TAG, "acomp_fd_live_detect_mode_set enter");

    fd_ipc_control_subcmd_live_detect_set_t live_detect_set;
    live_detect_set.enable = mode->enable;
    live_detect_set.score_threshold[0] = mode->score_threshold[0];
    live_detect_set.score_threshold[1] = mode->score_threshold[1];

    int ret = acomp_fd_control_subcmd(FD_ICP_CONTROL_SUBCMD_LIVE_DETECT_SET, &live_detect_set, sizeof(fd_ipc_control_subcmd_live_detect_set_t));
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp_fd_live_detect_mode_set failed!");
    }

    LISA_LOGI(TAG, "acomp_fd_live_detect_mode_set exit");
    return ret;
}

int acomp_fd_features_load(const acomp_fd_feature_result_t *features, uint32_t count)
{
    LISA_LOGI(TAG, "acomp_fd_features_load enter");

    int len = sizeof(fd_ipc_control_subcmd_features_load_t) + sizeof(acomp_fd_feature_result_t) * count;
    fd_ipc_control_subcmd_features_load_t *features_load = (fd_ipc_control_subcmd_features_load_t *)psram_malloc(len);
    if (features_load == NULL) {
        LISA_LOGE(TAG, "acomp_fd_features_load malloc failed!");
        return ACOMP_ERR_NO_MEM;
    }

    features_load->feature_cnt = count;
    acomp_fd_feature_result_t *features_data = (acomp_fd_feature_result_t *)features_load->data;
    memcpy((uint8_t *)features_data, (uint8_t *)features, sizeof(acomp_fd_feature_result_t) * count);

    int ret = acomp_fd_control_subcmd(FD_ICP_CONTROL_SUBCMD_FEATURES_LOAD, features_load, len);
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp_fd_features_load failed!");
    }

    psram_free(features_load);

    LISA_LOGI(TAG, "acomp_fd_features_load exit");
    return ret;
}

int acomp_fd_add_callback(uint32_t events, fd_event_cb_t cb, void *priv)
{
    LISA_LOGI(TAG, "acomp fd add callback enter");
    int ret;
    if (fd_handle == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    ret = gcl_cb_list_add_callback(fd_handle->event_callbacks, events, cb, priv);
    LISA_LOGI(TAG, "acomp fd add callback exit");
    return ret;
}

int acomp_fd_remove_callback(fd_event_cb_t cb)
{
    LISA_LOGI(TAG, "acomp fd remove callback enter");
    int ret;
    if (fd_handle == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    ret = gcl_cb_list_remove(fd_handle->event_callbacks, cb);
    LISA_LOGI(TAG, "acomp fd remove callback exit");
    return ret;
}

int acomp_fd_stream_ch_enable(int chn,acomp_stream_chn_create_desc_t *desc){

    int ret = 0;

    if ((fd_handle == NULL) || (fd_handle->stream == NULL)) {

        return ACOMP_ERR_INVALID_STATE;
    }

    if(chn >= ACOMP_STREAM_MAX_CHANNEL){
        return ACOMP_ERR_INVALID_ARG;
    }

    fd_handle->stream->ch[chn] = acomp_stream_ipc_channel_create(fd_handle->stream,chn,fd_handle->dev_index,desc);
    if(fd_handle->stream->ch[chn] == NULL){
        LISA_LOGE(TAG, "fd_handle->stream->ch[%d] == NULL, failed", chn);
        return ACOMP_ERR_CREATE_STREAM_FAILED;
    }
    LISA_LOGI(TAG,"acomp_fd_stream_ch_enable chn(%s) index(%d),desc(%p)",desc->cname,chn,desc);
    return ret;
}

int acomp_fd_stream_ch_disable(int chn){
    int ret;
    if (fd_handle == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    if(chn >= ACOMP_STREAM_MAX_CHANNEL){
        return ACOMP_ERR_INVALID_ARG;
    }

    ret = acomp_stream_ipc_channel_destroy(chn);
    LISA_LOGI(TAG,"acomp_fd_stream_ch_disable chn index(%d),ret(%d)",chn,ret);
    fd_handle->stream->ch[chn] = NULL;
    return ret;
}

void* acomp_fd_stream_rx_buffer_get(int chn, uint32_t* len, uint16_t* desc_idx){

    uint8_t *ptr;

    if (fd_handle == NULL) {
        return NULL;
    }

    if(chn >= ACOMP_STREAM_MAX_CHANNEL){
        return NULL;
    }

    if(fd_handle->stream->ch[chn] == NULL){
        return NULL;
    }

    ptr = fd_handle->stream->ops.rx_buffer_get(fd_handle->stream->ch[chn], len, desc_idx);
    return ptr;
}

int acomp_fd_stream_rx_buffer_release(int chn, uint16_t desc_idx, uint32_t len,void* buffer){
    int ret;

    if (fd_handle == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    if(chn >= ACOMP_STREAM_MAX_CHANNEL){
        return ACOMP_ERR_INVALID_ARG;
    }

    ret =  fd_handle->stream->ops.rx_buffer_release(fd_handle->stream->ch[chn],buffer, len, desc_idx);

    return ret;
}

void* acomp_fd_stream_tx_buffer_alloc(int chn, uint32_t* len, uint16_t* desc_idx){

    uint8_t* buffer;

    if (fd_handle == NULL) {
        return NULL;
    }

    if(chn >= ACOMP_STREAM_MAX_CHANNEL){
        return NULL;
    }

    if(fd_handle->stream->ch[chn] == NULL){
        return NULL;
    }

    buffer = fd_handle->stream->ops.tx_buffer_alloc(fd_handle->stream->ch[chn], len, desc_idx);

    return buffer;
}


int acomp_fd_stream_tx_buffer_submit(int chn, void* buffer, uint32_t len, uint16_t desc_idx){
    int ret;

    if (fd_handle == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    if(chn >= ACOMP_STREAM_MAX_CHANNEL){
        return ACOMP_ERR_INVALID_ARG;
    }

    if(fd_handle->stream->ch[chn] == NULL){
        return ACOMP_ERR_INVALID_STATE;
    }

    ret = fd_handle->stream->ops.tx_buffer_submit(fd_handle->stream->ch[chn], buffer, len, desc_idx);

    return ret;
}
