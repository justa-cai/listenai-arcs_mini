
#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"

#include "lisaui_user_data.h"
#include "camera_xfer.h"
#include "camera_core.h"

#define LOG_TAG "video_camera"

#include "lisa_log.h"

#define VIDEO_RECEIVE_TIMEOUT  (1000)
#define VIDEO_TASK_PRIORITY  (6)
#define VIDEO_TASK_TACK_SIZE (1024)

/* Event group definitions */
#define VIDEO_TASK_RUN_BIT    (1 << 0)    /* Video task run bit */
static EventGroupHandle_t video_task_event_group = NULL;
static TaskHandle_t video_task_handle = NULL;
static const camera_config_t camera_cfg = {
    .xclk_freq_hz = 24000000,
    .pixel_format = PIXFORMAT_RGB565,
    .frame_size = FRAMESIZE_QVGA,
    .buf_count = 3,
    .colorbar = 0,
    .is_h_mirror = false,
    .is_v_flip = true,
};

int video_frame_release_handler(struct lisaui_video_frame_desc *frame)
{
    struct cam_ipeg_mem *cam_mem = (struct cam_ipeg_mem *)frame->user_data;
    camera_qbuf(cam_mem);
    return 0;
}


static void video_task(void *pvParameters)
{
    lisaui_videoqueue_t *queue = NULL;
    lisaui_video_frame_desc_t frame_desc;
    queue = lisaui_userdata_get_camera_videoqueue();
    
    while (1) {
        /* Wait for RUN event bit, block if the bit is not set */
        EventBits_t bits = xEventGroupWaitBits(
            video_task_event_group,   
            VIDEO_TASK_RUN_BIT,       
            pdFALSE,                  
            pdTRUE,                   
            portMAX_DELAY);           
            
        /* Check if we have permission to run */
        if ((bits & VIDEO_TASK_RUN_BIT) == 0) {
            LISA_LOGI(LOG_TAG, "Video task blocked");
            continue;  /* If the run bit is not set, continue waiting */
        }
        
        /* Normal video processing flow */
        struct cam_ipeg_mem *cam_mem = NULL;

        cam_mem = camera_dqbuf(cam_mem, VIDEO_RECEIVE_TIMEOUT);
        if (cam_mem != NULL) {
            frame_desc.data = cam_mem->buf.addr;
            frame_desc.width = LISAUI_USERDATA_DVP_CAMERA_VIDEO_WIDTH;
            frame_desc.height = LISAUI_USERDATA_DVP_CAMERA_VIDEO_HEIGHT;
            frame_desc.size = cam_mem->buf.size;
            frame_desc.release_hook = video_frame_release_handler;
            frame_desc.user_data = cam_mem;

            int ret = lisaui_videoqueue_push(queue, &frame_desc, 0);
            if (ret != 0) {
                LISA_LOGW(LOG_TAG, "Drop camera frame.size:%d", cam_mem->buf.size);
                camera_qbuf(cam_mem);
            }
            else{
                // LISA_LOGI(LOG_TAG, "Push camera w:%d h:%d size:%d", 
                //     frame_desc.width, frame_desc.height, frame_desc.size);
            }
        }
        else{
            LISA_LOGE(LOG_TAG, "camera_dqbuf timeout");
        }
    }
}

int video_camera_init(void)
{
    int ret = 0;
    
    /* Create event group */
    video_task_event_group = xEventGroupCreate();
    if (video_task_event_group == NULL) {
        LISA_LOGE(LOG_TAG, "Failed to create video task event group");
        return -1;
    }
    
    /* Initially clear the run bit, the task will be blocked */
    xEventGroupClearBits(video_task_event_group, VIDEO_TASK_RUN_BIT);
    
    camera_init(&camera_cfg);
    ret = xTaskCreate(video_task, "video_task", VIDEO_TASK_TACK_SIZE, NULL, VIDEO_TASK_PRIORITY, &video_task_handle);
    return ret;
}

int video_camera_start(void)
{
    camera_start();
    
    /* Set event bit to allow video_task to run */
    if (video_task_event_group != NULL) {
        xEventGroupSetBits(video_task_event_group, VIDEO_TASK_RUN_BIT);
        LISA_LOGI(LOG_TAG, "Video task unblocked");
    }
    LISA_LOGI(LOG_TAG, "Video started");
    
    return 0;
}

int video_camera_stop(void)
{
    lisaui_videoqueue_t *queue = NULL;
    lisaui_video_frame_desc_t frame_desc;
    /* Clear event bit to block video_task */
    camera_stop();
    queue = lisaui_userdata_get_camera_videoqueue();
    do
    {
        if (0 == lisaui_videoqueue_pop(queue, &frame_desc, 0)) 
        {
            lisaui_videoqueue_release(queue, &frame_desc);
            continue;
        }
        break;
    } while (1);

    if (video_task_event_group != NULL) {
        xEventGroupClearBits(video_task_event_group, VIDEO_TASK_RUN_BIT);
        LISA_LOGI(LOG_TAG, "Video task blocked");
    }
    LISA_LOGI(LOG_TAG, "Video stopped");    
    
    return 0;
}
