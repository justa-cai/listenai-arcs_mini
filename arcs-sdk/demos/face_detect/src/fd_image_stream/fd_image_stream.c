#include "stdio.h"
#include <string.h>
#include <stdbool.h>

#include "workqueue.h"
#include "acomp_fd.h"

#include "fd_image_stream.h"

#define TAG "fd_image_stream"

#include "lisa_log.h"

/* fd image in stream */
#define FD_IMAGE_FRAME_STREAM_CH_INDEX (0)
#define FD_IMAGE_FRAME_STREAM_CH_CNAME "stream.fd_image"
#define FD_IMAGE_FRAME_STREAM_BUF_SIZE (CONFIG_IMAGE_WIDTH * CONFIG_IMAGE_HEIGHT * 2 + sizeof(acomp_fd_input_frame_t))

static workqueue_t *fd_in_wq = NULL;

int fd_image_stream_init(void)
{
    acomp_stream_chn_create_desc_t desc = {
        .cname = FD_IMAGE_FRAME_STREAM_CH_CNAME,
        .direction = ACOMP_STREAM_DIRECTION_M2R,
        .index = FD_IMAGE_FRAME_STREAM_CH_INDEX,
        .buffer_size = FD_IMAGE_FRAME_STREAM_BUF_SIZE,
        .num_descs = 4,
        .kick_policy = 1,
    };

    fd_in_wq = workqueue_create("fd_in_wq", 9, 10, 8096);
    if (fd_in_wq == NULL) {
        LISA_LOGE(TAG, "Failed to create fd_in_wq");
    }

    acomp_fd_stream_ch_enable(FD_IMAGE_FRAME_STREAM_CH_INDEX,&desc);

    return 0;
}

static void fd_in_wq_handler(void *para)
{
    uint8_t *buffer;
    uint32_t buf_size;
    uint16_t desc_idx;
    acomp_fd_input_frame_t *fd_frame;
    static int frame_index = 0;
    uint8_t *data = NULL;

    video_frame_t *msg = (video_frame_t *)para;
    LISA_LOGD(LOG_TAG, "fd_in_wq_handler");

    buffer = acomp_fd_stream_tx_buffer_alloc(FD_IMAGE_FRAME_STREAM_CH_INDEX, &buf_size, &desc_idx);
    
    if(buffer && buf_size > 0){
        fd_frame = (acomp_fd_input_frame_t *)buffer;

        if (msg->format == LISA_CAMERA_PIXFMT_YUV422) {
            fd_frame->format = PIX_FMT_YUV422_YUYV_PACKED;
        } else if (msg->format == LISA_CAMERA_PIXFMT_RGB565) {
            fd_frame->format = PIX_FMT_RGB565;
        } else if (msg->format == LISA_CAMERA_PIXFMT_GRAY) {
            fd_frame->format = PIX_FMT_GRAY;
        }

        fd_frame->index = frame_index++;
        fd_frame->width = msg->width;
        fd_frame->height = msg->height;
        fd_frame->length = msg->length;
        memcpy(fd_frame->data, msg->buffer, msg->length);

        acomp_fd_stream_tx_buffer_submit(FD_IMAGE_FRAME_STREAM_CH_INDEX, buffer, buf_size, desc_idx);
    }

    msg->func(msg->func_param);
    psram_free(msg);
}

int fd_image_stream_send(video_frame_t *data)
{
    int ret;

    video_frame_t *msg = psram_malloc(sizeof(video_frame_t));
    memcpy(msg, data, sizeof(video_frame_t));

    ret = workqueue_submit(fd_in_wq, fd_in_wq_handler, msg);
    if (ret == 0) {
        LISA_LOGE(TAG, "workqueue submit failed:%d", ret);
        msg->func(msg->func_param);
        psram_free(msg);
    }
}
