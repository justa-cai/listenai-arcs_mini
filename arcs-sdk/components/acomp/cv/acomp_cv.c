#include <string.h>

#include "ipc/acomp_ipc.h"

#include "acomp_cv.h"
#include "acomp_err.h"
#include "gcl_cb_list/gcl_cb_list.h"
#include "private/cv_ipc.h"
#include "comm/stream/acomp_stream_ipc.h"

#define TAG "acomp_cv"
#include "lisa_log.h"

#define ACOMP_CV_DEV_NAME "acomp.cv"

typedef struct {
    uint32_t dev_index;
    gcl_cb_list_t event_callbacks;
    acomp_stream_t *stream;
} acomp_cv_handle_t;

static acomp_cv_handle_t *cv_handle = NULL;

static int cv_control_subcmd(cv_ipc_control_subcmd_e subcmd, void *data, uint32_t data_len);

void cv_event_callback(acomp_ipc_message_t *message, void *priv)
{
    acomp_cv_handle_t *handle = (acomp_cv_handle_t *)priv;
    if (handle == NULL || message == NULL) {
        return;
    }

    if (message->hdr.hdr.cmd == ACOMP_CONTEXT_IPC_GLB_NOTIFY) {

        if (message->acomp_cmd == ACOMP_IPC_CMD_NOTIFY_RESULT) {

            if (((void *)message->address != NULL) && (message->len > 0)) {

                acomp_ipc_notify_result_t *result = (acomp_ipc_notify_result_t *)message->address;
                gcl_cb_event_dispatch(handle->event_callbacks, CV_CB_EVENT_OCR_RESULT, result->data, result->len);
            }
        } else if (message->acomp_cmd == ACOMP_IPC_CMD_NOTIFY_SUBCMD) {

            if (((void *)message->address != NULL) && (message->len > 0)) {

                acomp_ipc_notify_subcmd_t *subcmd = (acomp_ipc_notify_subcmd_t *)message->address;
                cv_ipc_notify_subcmd_hdr_t *hdr = (cv_ipc_notify_subcmd_hdr_t *)subcmd->data;

                if (hdr->cmd == CV_IPC_NOTIFY_SUBCMD_OCR_RESULT) {
                    cv_ipc_notify_subcmd_ocr_result_t *ocr_result = (cv_ipc_notify_subcmd_ocr_result_t *)subcmd->data;
                    gcl_cb_event_dispatch(handle->event_callbacks, CV_CB_EVENT_OCR_RESULT, ocr_result->data,
                                              ocr_result->len);

                } else if (hdr->cmd == CV_IPC_NOTIFY_SUBCMD_STITCH_FRAME) {
                    cv_ipc_notify_subcmd_stitch_frame_t *stitch_frame = (cv_ipc_notify_subcmd_stitch_frame_t *)subcmd->data;
                    gcl_cb_event_dispatch(handle->event_callbacks, CV_CB_EVENT_STITCH_FRAME, stitch_frame->data,
                                              stitch_frame->len);

                } else if (hdr->cmd == CV_IPC_NOTIFY_SUBCMD_STATUS) {
                    cv_ipc_notify_subcmd_status_t *status = (cv_ipc_notify_subcmd_status_t *)subcmd->data;
                    gcl_cb_event_dispatch(handle->event_callbacks, CV_CB_EVENT_STATUS, &status->status,
                                          sizeof(status->status));
                } else if (hdr->cmd == CV_IPC_NOTIFY_SUBCMD_FRAME_DONE) {
                    cv_ipc_notify_subcmd_frame_done_t *frame_done = (cv_ipc_notify_subcmd_frame_done_t *)subcmd->data;
                    gcl_cb_event_dispatch(handle->event_callbacks, CV_CB_EVENT_FRAME_DONE, &frame_done->fb_addr,
                                          sizeof(frame_done->fb_addr));
                } else if (hdr->cmd == CV_IPC_NOTIFY_SUBCMD_IMG_SAVE) {
                    cv_ipc_notify_subcmd_img_save_t *img_save = (cv_ipc_notify_subcmd_img_save_t *)subcmd->data;
                    cv_img_save_info_t info = {
                        .img_addr = img_save->img_addr,
                        .width = img_save->width,
                        .height = img_save->height,
                        .img_type = img_save->img_type,
                    };
                    gcl_cb_event_dispatch(handle->event_callbacks, CV_CB_EVENT_IMG_SAVE, &info,
                                          sizeof(info));
                } else {
                    LISA_LOGW(TAG, "unknow notify hdr cmd:%d", hdr->cmd);
                }
            }
        }
        else if (message->acomp_cmd == ACOMP_IPC_CMD_NOTIFY_STREAM_UPDATE) {

            if (((void *)message->address != NULL) && (message->len > 0)) {
                acomp_ipc_stream_update_t *ipc_msg;
                uint32_t chn;
                ipc_msg = (acomp_ipc_stream_update_t *)message->address;
                chn = ipc_msg->index;
                if (chn < sizeof(handle->stream->ch) / sizeof(handle->stream->ch[0])) {
                    gcl_cb_event_dispatch(handle->event_callbacks,
                        CV_CB_EVENT_STREAM_UPDATE,
                        handle->stream->ch[chn],
                        sizeof(acomp_stream_channel_t));
                }
            }
        }
    }
}

int acomp_cv_init(void)
{
    int ret = 0;
    LISA_LOGI(TAG, "acomp cv init enter");

    if (cv_handle != NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    cv_handle = (acomp_cv_handle_t *)psram_malloc(sizeof(acomp_cv_handle_t));
    if (cv_handle == NULL) {
        return ACOMP_ERR_NO_MEM;
    }
    memset(cv_handle, 0, sizeof(acomp_cv_handle_t));
    cv_handle->event_callbacks = gcl_cb_list_create();
    if (cv_handle->event_callbacks == NULL) {
        LISA_LOGE(TAG, "acomp cv cb list create failed!");
        ret = ACOMP_ERR_NO_MEM;
        goto err_free_handle;
    }

    int dev_index = acomp_ipc_get_dev_index(ACOMP_CV_DEV_NAME);
    if (dev_index < 0) {
        LISA_LOGE(TAG, "acomp cv dev index not found!");
        ret = ACOMP_ERR_NOT_FOUND;
        goto err_destroy_cb;
    }
    cv_handle->dev_index = (uint32_t)dev_index;
    LISA_LOGI(TAG, "acomp cv dev index %d,name:%s", cv_handle->dev_index, ACOMP_CV_DEV_NAME);

    ret = acomp_ipc_add_callback(cv_handle->dev_index, (ipc_event_cb_t)cv_event_callback, cv_handle);
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp cv add callback failed!");
        goto err_destroy_cb;
    }

    ret = acomp_ipc_build_frame_send_sync(cv_handle->dev_index, ACOMP_CONTEXT_IPC_GLB_NEW | IPC_HEADER_REQ_REPALY, 0,
                                          0, NULL, 0);
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp cv init failed!");
        goto err_remove_cb;
    }

    cv_handle->stream = acomp_stream_create(cv_handle->dev_index);
    if (cv_handle->stream == NULL) {
        LISA_LOGE(TAG, "acomp cv stream create failed!");
        ret = ACOMP_ERR_CREATE_STREAM_FAILED;
        goto err_remove_cb;
    }

    LISA_LOGI(TAG, "acomp cv init exit");
    return 0;

err_remove_cb:
    acomp_ipc_remove_callback(cv_handle->dev_index, (ipc_event_cb_t)cv_event_callback);
err_destroy_cb:
    gcl_cb_list_delete(cv_handle->event_callbacks);
err_free_handle:
    psram_free(cv_handle);
    cv_handle = NULL;
    return ret;
}

int acomp_cv_prepare(acomp_ipc_prepare_t *prepare)
{
    LISA_LOGI(TAG, "acomp cv prepare enter");

    if (cv_handle == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    if (prepare == NULL) {
        return ACOMP_ERR_INVALID_ARG;
    }

    uint32_t size;
    int ret;

    size = sizeof(acomp_ipc_prepare_t) + sizeof(acomp_res_item_t) * prepare->number;
    size = ALIGN_SIZE(size);

    ret = acomp_ipc_build_frame_send_sync(cv_handle->dev_index, ACOMP_CONTEXT_IPC_GLB_CONTROL | IPC_HEADER_REQ_REPALY,
                                          ACOMP_IPC_CMD_PREPARE, 0, prepare, size);
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp cv prepare failed!");
    }

    LISA_LOGI(TAG, "acomp cv prepare exit");
    return ret;
}

int acomp_cv_cleanup(void)
{
    if (cv_handle == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    int ret;
    ret = acomp_ipc_build_frame_send_sync(cv_handle->dev_index, ACOMP_CONTEXT_IPC_GLB_CONTROL | IPC_HEADER_REQ_REPALY,
                                          ACOMP_IPC_CMD_CLEANUP, 0, NULL, 0);
    return ret;
}

int acomp_cv_start(void)
{
    if (cv_handle == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    LISA_LOGI(TAG, "acomp cv start enter");
    int ret;
    ret = acomp_ipc_build_frame_send_sync(cv_handle->dev_index, ACOMP_CONTEXT_IPC_GLB_CONTROL | IPC_HEADER_REQ_REPALY,
                                          ACOMP_IPC_CMD_START, 0, NULL, 0);
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp cv start failed!");
    }

    LISA_LOGI(TAG, "acomp cv start exit");
    return ret;
}

int acomp_cv_stop(void)
{
    if (cv_handle == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    LISA_LOGI(TAG, "acomp cv stop enter");
    int ret;
    ret = acomp_ipc_build_frame_send_sync(cv_handle->dev_index, ACOMP_CONTEXT_IPC_GLB_CONTROL | IPC_HEADER_REQ_REPALY,
                                          ACOMP_IPC_CMD_STOP, 0, NULL, 0);
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp cv stop failed!");
    }

    LISA_LOGI(TAG, "acomp cv stop exit");
    return ret;
}

static int cv_control_subcmd(cv_ipc_control_subcmd_e subcmd, void *data, uint32_t data_len)
{
    if (cv_handle == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

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

    ret = acomp_ipc_build_frame_send_sync(cv_handle->dev_index, ACOMP_CONTEXT_IPC_GLB_CONTROL | IPC_HEADER_REQ_REPALY,
                                            ACOMP_IPC_CMD_CONTROL, 0, ipc_control, size);

    psram_free(ipc_control);

    return ret;
}

int acomp_cv_set_scan_mode(uint8_t scan_mode)
{
    LISA_LOGI(TAG, "acomp cv set scan mode enter, scan_mode=%d", scan_mode);

    cv_ipc_control_subcmd_scan_mode_set_t scan_mode_set;
    scan_mode_set.scan_mode = scan_mode;

    int ret = cv_control_subcmd(CV_IPC_CONTROL_SUBCMD_SET_SCAN_MODE, &scan_mode_set, sizeof(cv_ipc_control_subcmd_scan_mode_set_t));
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp cv set scan mode failed!");
    }

    LISA_LOGI(TAG, "acomp cv set scan mode exit");
    return ret;
}

int acomp_cv_set_lr_mode(uint8_t lr_mode)
{
    LISA_LOGI(TAG, "acomp cv set lr mode enter, lr_mode=%d", lr_mode);

    cv_ipc_control_subcmd_lr_mode_set_t lr_mode_set;
    lr_mode_set.lr_mode = lr_mode;

    int ret = cv_control_subcmd(CV_IPC_CONTROL_SUBCMD_SET_LR_MODE, &lr_mode_set, sizeof(cv_ipc_control_subcmd_lr_mode_set_t));
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp cv set lr mode failed!");
    }

    LISA_LOGI(TAG, "acomp cv set lr mode exit");
    return ret;
}

int acomp_cv_set_screen_height(uint32_t screen_height)
{
    LISA_LOGI(TAG, "acomp cv set screen height enter, screen_height=%u", screen_height);

    cv_ipc_control_subcmd_screen_height_set_t screen_height_set;
    screen_height_set.screen_height = screen_height;

    int ret = cv_control_subcmd(CV_IPC_CONTROL_SUBCMD_SET_SCREEN_HEIGHT, &screen_height_set, sizeof(cv_ipc_control_subcmd_screen_height_set_t));
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp cv set screen height failed!");
    }

    LISA_LOGI(TAG, "acomp cv set screen height exit");
    return ret;
}

int acomp_cv_set_boot_type(uint8_t boot_type)
{
    LISA_LOGI(TAG, "acomp cv set boot type enter, boot_type=%d", boot_type);

    cv_ipc_control_subcmd_boot_type_set_t boot_type_set;
    boot_type_set.boot_type = boot_type;

    int ret = cv_control_subcmd(CV_IPC_CONTROL_SUBCMD_SET_BOOT_TYPE, &boot_type_set, sizeof(cv_ipc_control_subcmd_boot_type_set_t));
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "acomp cv set boot type failed!");
    }

    LISA_LOGI(TAG, "acomp cv set boot type exit");
    return ret;
}

int acomp_cv_add_callback(uint32_t events, cv_event_cb_t cb, void *priv)
{
    int ret;
    if (cv_handle == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    ret = gcl_cb_list_add_callback(cv_handle->event_callbacks, events, cb, priv);
    return ret;
}

int acomp_cv_remove_callback(cv_event_cb_t cb)
{
    int ret;
    if (cv_handle == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    ret = gcl_cb_list_remove_callback(cv_handle->event_callbacks, cb);
    return ret;
}

int acomp_cv_stream_ch_enable(int chn, acomp_stream_chn_create_desc_t *desc)
{
    int ret = 0;

    if ((cv_handle == NULL) || (cv_handle->stream == NULL)) {
        return ACOMP_ERR_INVALID_STATE;
    }

    if (chn >= ACOMP_STREAM_MAX_CHANNEL) {
        return ACOMP_ERR_INVALID_ARG;
    }

    cv_handle->stream->ch[chn] = acomp_stream_ipc_channel_create(cv_handle->stream, chn, cv_handle->dev_index, desc);
    if (cv_handle->stream->ch[chn] == NULL) {
        return ACOMP_ERR_CREATE_STREAM_FAILED;
    }
    LISA_LOGI(TAG, "acomp_cv_stream_ch_enable chn(%s) index(%d),desc(%p)", desc->cname, chn, desc);
    return ret;
}

int acomp_cv_stream_ch_disable(int chn)
{
    int ret;
    if (cv_handle == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    if (chn >= ACOMP_STREAM_MAX_CHANNEL) {
        return ACOMP_ERR_INVALID_ARG;
    }

    ret = acomp_stream_ipc_channel_destroy(chn);
    LISA_LOGI(TAG, "acomp_cv_stream_ch_disable chn index(%d),ret(%d)", chn, ret);
    cv_handle->stream->ch[chn] = NULL;
    return ret;
}

void* acomp_cv_stream_tx_buffer_alloc(int chn, uint32_t* len, uint16_t* desc_idx)
{
    uint8_t* buffer;

    if (cv_handle == NULL) {
        return NULL;
    }

    if (chn >= ACOMP_STREAM_MAX_CHANNEL) {
        return NULL;
    }

    if (cv_handle->stream->ch[chn] == NULL) {
        return NULL;
    }

    buffer = cv_handle->stream->ops.tx_buffer_alloc(cv_handle->stream->ch[chn], len, desc_idx);

    return buffer;
}

int acomp_cv_stream_tx_buffer_submit(int chn, void* buffer, uint32_t len, uint16_t desc_idx)
{
    int ret;

    if (cv_handle == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    if (chn >= ACOMP_STREAM_MAX_CHANNEL) {
        return ACOMP_ERR_INVALID_ARG;
    }

    if (cv_handle->stream->ch[chn] == NULL) {
        return ACOMP_ERR_INVALID_STATE;
    }

    ret = cv_handle->stream->ops.tx_buffer_submit(cv_handle->stream->ch[chn], buffer, len, desc_idx);

    return ret;
}
