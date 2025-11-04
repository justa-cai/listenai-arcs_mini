#include <string.h>
#include "platform.h"
#include "lisaui_videoqueue.h"

#define VIDEO_BUFFER_ALIGN_BYTES (32)

lisaui_videoqueue_t *lisaui_videoqueue_create(uint32_t size, uint32_t count, uint32_t flags)
{
    lisaui_videoqueue_t *videoqueue = (lisaui_videoqueue_t *)lisaui_malloc(sizeof(lisaui_videoqueue_t));
    if (videoqueue == NULL) {
        return NULL;
    }
    memset(videoqueue, 0, sizeof(lisaui_videoqueue_t));
    videoqueue->frame_count = count;
    videoqueue->attributes = flags;
    videoqueue->available_queue = lisaui_queue_create(count, sizeof(lisaui_video_frame_desc_t));
    if (videoqueue->available_queue == NULL) {
        lisaui_free(videoqueue);
        return NULL;
    }

    if (LISAUI_VIDEOQUEUE_ATTRIBUTE_ACQUIRE_BUFFER & flags) {
        videoqueue->free_queue = lisaui_queue_create(count, sizeof(lisaui_video_frame_desc_t));
        if (videoqueue->free_queue == NULL) {
            lisaui_queue_delete(videoqueue->available_queue);
            lisaui_queue_delete(videoqueue->free_queue);
            lisaui_free(videoqueue);
            return NULL;
        }

        videoqueue->frames = (lisaui_video_frame_desc_t *)lisaui_malloc(sizeof(lisaui_video_frame_desc_t) * count);
        if (videoqueue->frames == NULL) {
            lisaui_queue_delete(videoqueue->available_queue);
            lisaui_queue_delete(videoqueue->free_queue);
            lisaui_free(videoqueue);
            return NULL;
        }
        for (int i = 0; i < count; i++) {
            videoqueue->frames[i].data = (uint8_t *)lisaui_align_malloc(VIDEO_BUFFER_ALIGN_BYTES, size);
            if (videoqueue->frames[i].data == NULL) {
                for (int j = 0; j < i; j++) {
                    lisaui_free(videoqueue->frames[j].data);
                }
                lisaui_free(videoqueue->frames);
                lisaui_queue_delete(videoqueue->available_queue);
                lisaui_queue_delete(videoqueue->free_queue);
                lisaui_free(videoqueue);
                return NULL;
            }
            lisaui_queue_send(videoqueue->free_queue, &videoqueue->frames[i], 0);
        }
    }

    return videoqueue;
}

int lisaui_videoqueue_buffer_acquire(lisaui_videoqueue_t *queue, lisaui_video_frame_desc_t **frame, uint32_t timeout_ms)
{
    int ret = 0;

    if (queue == NULL || queue->free_queue == NULL) 
    {
        return -EINVAL;
    }

    if(LISAUI_OS_SUCCESS != lisaui_queue_receive(queue->free_queue, frame, pdMS_TO_TICKS(timeout_ms)))
    {
        ret = -ETIMEDOUT;
    }

    return ret;
}

int lisaui_videoqueue_push(lisaui_videoqueue_t *queue, lisaui_video_frame_desc_t *frame, uint32_t timeout_ms)
{
    int ret = 0;

    if (queue == NULL || queue->available_queue == NULL) 
    {
        return -EINVAL;
    }

    if(LISAUI_OS_SUCCESS != lisaui_queue_send(queue->available_queue, frame, pdMS_TO_TICKS(timeout_ms)))
    {
        ret = -ETIMEDOUT;
    }

    return ret;
}

int lisaui_videoqueue_pop(lisaui_videoqueue_t *queue, lisaui_video_frame_desc_t *frame, uint32_t timeout_ms)
{
    int ret = 0;

    if (queue == NULL || queue->available_queue == NULL) 
    {
        return -EINVAL;
    }

    if (LISAUI_OS_SUCCESS != lisaui_queue_receive(queue->available_queue, frame, pdMS_TO_TICKS(timeout_ms))) 
    {
        ret = -ETIMEDOUT;
    }

    return ret;
}

int lisaui_videoqueue_release(lisaui_videoqueue_t *queue, lisaui_video_frame_desc_t *frame)
{
    int ret = 0;

    if (queue == NULL || queue->available_queue == NULL || frame == NULL || frame->data == NULL) 
    {
        return -EINVAL;
    }

    if (LISAUI_VIDEOQUEUE_ATTRIBUTE_ACQUIRE_BUFFER & queue->attributes) {
        lisaui_queue_send(queue->free_queue, frame, 0);
    }

    if (frame->release_hook != NULL) {
        frame->release_hook(frame);
    }

    return ret;
}
