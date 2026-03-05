#include "stdio.h"
#include <string.h>
#include <stdbool.h>

#include "workqueue.h"
#include "app_fd_image_input.h"

#define TAG "app_fd_stream"

#include "lisa_log.h"

/* fd image in stream */
#define FD_IMAGE_FRAME_STREAM_CH_INDEX (0)
#define FD_IMAGE_FRAME_STREAM_CH_CNAME "stream.fd_image"
#define FD_IMAGE_FRAME_STREAM_BUF_SIZE (320 * 240 * 2 + sizeof(acomp_fd_input_frame_t))

static workqueue_t *fd_in_wq = NULL;

int app_fd_image_stream_init(void)
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

    image_frame_t *msg = (image_frame_t *)para;
    LISA_LOGD(TAG, "fd_in_wq_handler");

    buffer = acomp_fd_stream_tx_buffer_alloc(FD_IMAGE_FRAME_STREAM_CH_INDEX, &buf_size, &desc_idx);
    
    if(buffer && buf_size > 0){
        fd_frame = (acomp_fd_input_frame_t *)buffer;

        fd_frame->format = msg->image_format;
        fd_frame->index = msg->image_index;
        fd_frame->width = msg->image_width;
        fd_frame->height = msg->image_height;
        fd_frame->length = msg->image_length;
        memcpy(fd_frame->data, msg->image_data, msg->image_length);

        acomp_fd_stream_tx_buffer_submit(FD_IMAGE_FRAME_STREAM_CH_INDEX, buffer, buf_size, desc_idx);
    }

    psram_free(msg);
}

int app_fd_image_send(image_frame_t *data)
{
    int ret;

    image_frame_t *msg = psram_malloc(sizeof(image_frame_t));
    memcpy(msg, data, sizeof(image_frame_t));

    ret = workqueue_submit(fd_in_wq, fd_in_wq_handler, msg);
    if (ret == 0) {
        LISA_LOGE(TAG, "workqueue submit failed:%d", ret);
        psram_free(msg);
    }
    
    return ret;
}
