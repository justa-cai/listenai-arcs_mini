#ifndef __LISAUI_VIDEOQUEUE_H__
#define __LISAUI_VIDEOQUEUE_H__

#include <stdint.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "queue.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LISAUI_VIDEOQUEUE_ATTRIBUTE_ACQUIRE_BUFFER (1 << 0)

/* Forward declaration of the video frame descriptor structure */
struct lisaui_video_frame_desc;

typedef int (*lisaui_video_frame_release_hook_t)(struct lisaui_video_frame_desc *frame);

typedef struct lisaui_video_frame_desc {

    uint8_t *data;
    uint32_t width;
    uint32_t height;
    uint32_t size;
    lisaui_video_frame_release_hook_t release_hook;
    void *user_data;
} lisaui_video_frame_desc_t;

typedef struct {
    uint32_t attributes;
    QueueHandle_t available_queue;
    uint32_t frame_count;
    lisaui_video_frame_desc_t *frames;
    QueueHandle_t free_queue;

} lisaui_videoqueue_t;

/**
 * @brief Create a video queue
 *
 * @param size Frame size
 * @param count Maximum number of frames in queue
 * @return Pointer to created queue or NULL on failure
 */
lisaui_videoqueue_t *lisaui_videoqueue_create(uint32_t size, uint32_t count, uint32_t flags);

/**
 * @brief Acquire a frame buffer from the queue
 *
 * @param queue Queue handle
 * @param frame Buffer to store acquired frame
 * @param timeout_ms Timeout in milliseconds
 * @return 0 on success, negative value on failure
 * @note This function is only available when LISAUI_VIDEOQUEUE_ATTRIBUTE_ACQUIRE_BUFFER is set.
 */
int lisaui_videoqueue_buffer_acquire(lisaui_videoqueue_t *queue, lisaui_video_frame_desc_t **frame,
                                     uint32_t timeout_ms);

/**
 * @brief Push a frame to the queue
 *
 * @param queue Queue handle
 * @param frame Frame to push
 * @param timeout_ms Timeout in milliseconds
 * @return 0 on success, negative value on failure
 */
int lisaui_videoqueue_push(lisaui_videoqueue_t *queue, lisaui_video_frame_desc_t *frame, uint32_t timeout_ms);

/**
 * @brief Pop a frame from the queue
 *
 * @param queue Queue handle
 * @param frame Buffer to store popped frame
 * @param timeout_ms Timeout in milliseconds
 * @return 0 on success, negative value on failure
 */
int lisaui_videoqueue_pop(lisaui_videoqueue_t *queue, lisaui_video_frame_desc_t *frame, uint32_t timeout_ms);

/**
 * @brief Release a frame after use
 *
 * @param queue Queue handle
 * @param frame Frame to release
 * @return 0 on success, negative value on failure
 */
int lisaui_videoqueue_release(lisaui_videoqueue_t *queue, lisaui_video_frame_desc_t *frame);

/**
 * @brief Get number of free slots in queue
 *
 * @param queue Queue handle
 * @return Number of free slots or negative value on failure
 */
int lisaui_videoqueue_free_count(lisaui_videoqueue_t *queue);

/**
 * @brief Get number of available frames in queue
 *
 * @param queue Queue handle
 * @return Number of available frames or negative value on failure
 */
int lisaui_videoqueue_available_count(lisaui_videoqueue_t *queue);

#ifdef __cplusplus
}
#endif

#endif /* __LISAUI_VIDEOQUEUE_H__ */