#include "cache.h"
#include "ic_message.h"
#include <stdint.h>
#include <string.h>
#include "sysheap.h"
#include "log_print.h"
#include "lsfs_interface.h"
#include "img_stream.h"

static img_stream_t *_img_stream = NULL;

static int32_t _img_stream_cb(ic_message_handle_info_t *handle, ic_message_msg_info_t *msg)
{
    img_stream_t *img_stream = (void *)handle->user_datas;
    struct img_stream_msg *img_msg = (void *)msg->msg;
    uint32_t img_size = img_msg->wdith * img_msg->height;
    int ret = 0;

    if (img_msg->addr && img_size)
        HAL_InvalidateDCache_by_Addr((uint32_t *)img_msg->addr, img_size);

    for (int i = 0; i < sizeof(img_stream->observers)/sizeof(img_stream->observers[0]); i++) {
        if (img_stream->observers[i] != NULL) {
            img_stream->observers[i](img_msg);
        }
    }

end:
    ic_message_msg_send_by_id(IC_MESSAGE_ID_STREAM, IC_MESSAGE_MSG_TYPE_CMD, &ret, sizeof(ret));

    return 0;
}

img_stream_t *img_stream_init(void)
{
    img_stream_t *img_stream = exram_malloc(4, sizeof(img_stream_t));
    if (!img_stream) {
        CLOGE("malloc img_stream failed");
        return NULL;
    }

    memset(img_stream, 0x00, sizeof(img_stream_t));

    uint32_t send_data = 0;
    ic_message_register_by_id(IC_MESSAGE_ID_STREAM, _img_stream_cb, img_stream);
    do {
        int ret = ic_message_msg_send_by_id(IC_MESSAGE_ID_STREAM, IC_MESSAGE_MSG_TYPE_CMD, &send_data, sizeof(send_data));
        if (ret == 0) break;
    } while (1);

    _img_stream = img_stream;

    return img_stream;
}

void img_stream_register_on_recv(on_data_received cbs_recv)
{
    if (!_img_stream)
        return;

    for (int i = 0; i < sizeof(_img_stream->observers) / sizeof(_img_stream->observers[0]); i++) {
        if (_img_stream->observers[i] == NULL) {
            _img_stream->observers[i] = cbs_recv;
            break;
        }
    }
}

void img_stream_unregister_on_recv(on_data_received cbs_recv)
{
    if (!_img_stream)
        return;

    for (int i = 0; i < sizeof(_img_stream->observers) / sizeof(_img_stream->observers[0]); i++) {
        if (_img_stream->observers[i] == cbs_recv) {
            _img_stream->observers[i] = NULL;
            break;
        }
    }
}

void img_stream_unregister_all(void)
{
    for (int i = 0; i < sizeof(_img_stream->observers) / sizeof(_img_stream->observers[0]); i++) {
        if (_img_stream->observers[i] != NULL) {
            _img_stream->observers[i] = NULL;
        }
    }
}