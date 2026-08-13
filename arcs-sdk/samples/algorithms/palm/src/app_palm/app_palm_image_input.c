#include <string.h>

#include "FreeRTOS.h"
#include "workqueue.h"
#include "sysheap.h"

#include "app_palm_image_input.h"

#define TAG "app_palm_stream"
#include "lisa_log.h"

#define PALM_IMAGE_FRAME_STREAM_CH_INDEX 0
#define PALM_IMAGE_FRAME_STREAM_CH_CNAME "stream.palm_image"
#define PALM_IMAGE_FRAME_STREAM_BUF_SIZE (320U * 240U * 3U + sizeof(acomp_palm_input_frame_t))

static workqueue_t *g_palm_in_wq;

int app_palm_image_stream_init(void)
{
    int ret;
    acomp_stream_chn_create_desc_t desc = {
        .cname = PALM_IMAGE_FRAME_STREAM_CH_CNAME,
        .direction = ACOMP_STREAM_DIRECTION_M2R,
        .index = PALM_IMAGE_FRAME_STREAM_CH_INDEX,
        .buffer_size = PALM_IMAGE_FRAME_STREAM_BUF_SIZE,
        .num_descs = 4,
        .kick_policy = 1,
    };

    g_palm_in_wq = workqueue_create("palm_in_wq", 9, 10, 8192);
    if (g_palm_in_wq == NULL) {
        LISA_LOGE(TAG, "create palm input workqueue failed");
        return -1;
    }

    ret = acomp_palm_stream_ch_enable(PALM_IMAGE_FRAME_STREAM_CH_INDEX, &desc);
    if (ret != 0) {
        LISA_LOGE(TAG, "acomp_palm_stream_ch_enable failed: %d", ret);
        return ret;
    }

    return 0;
}

static void palm_in_wq_handler(void *para)
{
    uint8_t *buffer;
    uint32_t buf_size;
    uint16_t desc_idx;
    acomp_palm_input_frame_t *palm_frame;
    palm_image_frame_t *msg = (palm_image_frame_t *)para;

    if (msg == NULL) {
        return;
    }

    buffer = acomp_palm_stream_tx_buffer_alloc(PALM_IMAGE_FRAME_STREAM_CH_INDEX, &buf_size, &desc_idx);
    if (buffer == NULL || buf_size < (sizeof(*palm_frame) + msg->image_length)) {
        LISA_LOGE(TAG, "alloc palm stream buffer failed, size=%u", buf_size);
        psram_free(msg);
        return;
    }

    palm_frame = (acomp_palm_input_frame_t *)buffer;
    palm_frame->format = msg->image_format;
    palm_frame->index = msg->image_index;
    palm_frame->width = msg->image_width;
    palm_frame->height = msg->image_height;
    palm_frame->length = msg->image_length;
    memset(palm_frame->resv, 0, sizeof(palm_frame->resv));
    memcpy(palm_frame->data, msg->image_data, msg->image_length);

    (void)acomp_palm_stream_tx_buffer_submit(PALM_IMAGE_FRAME_STREAM_CH_INDEX,
                                             buffer,
                                             sizeof(*palm_frame) + msg->image_length,
                                             desc_idx);
    psram_free(msg);
}

int app_palm_image_send(palm_image_frame_t *data)
{
    int ret;
    palm_image_frame_t *msg;

    if (g_palm_in_wq == NULL || data == NULL || data->image_data == NULL || data->image_length == 0U) {
        return -1;
    }

    msg = (palm_image_frame_t *)psram_malloc(sizeof(*msg));
    if (msg == NULL) {
        return -1;
    }
    memcpy(msg, data, sizeof(*msg));

    ret = workqueue_submit(g_palm_in_wq, palm_in_wq_handler, msg);
    if (ret != pdPASS) {
        LISA_LOGE(TAG, "workqueue submit failed: %d", ret);
        psram_free(msg);
    }

    return ret;
}
