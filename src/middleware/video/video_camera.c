
#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Driver_DVP.h"
#include "Driver_GPDMA.h"
#include "Driver_SPI.h"
#include "Driver_I2C.h"
#include "Driver_UART.h"
#include "IOMuxManager.h"
#include "ClockManager.h"
#include "lisa_log.h"
#include "camera_xfer.h"
#include "camera_core.h"

#define TAG "video_camera"


#define VIDEO_RECEIVE_TIMEOUT  (1000)  // 视频接收超时时间（毫秒）
#define VIDEO_TASK_PRIORITY  (6)        // 视频任务优先级
#define VIDEO_TASK_TACK_SIZE (4*1024)     // 视频任务栈大小

/* 事件组定义 */
#define VIDEO_TASK_RUN_BIT    (1 << 0)  /* 视频任务运行位 */
static EventGroupHandle_t video_task_event_group = NULL;  // 视频任务事件组
static TaskHandle_t video_task_handle = NULL;             // 视频任务句柄


// 视频帧释放处理函数（暂时禁用，需要视频队列支持）
// int video_frame_release_handler(struct lisaui_video_frame_desc *frame)
// {
//     struct cam_ipeg_mem *cam_mem = (struct cam_ipeg_mem *)frame->user_data;
//     camera_qbuf(cam_mem);
//     return 0;
// }


// 视频任务主函数（暂时禁用，需要视频队列支持）
static void video_task(void *pvParameters __attribute__((unused)))
{
    // 视频队列功能暂时禁用，等待lisaui_videoqueue相关类型定义
    LISA_LOGI(TAG, "Video task started (video queue disabled)");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));  // 空循环，避免任务退出
    }
    
    /* 原始代码保留供参考
    lisaui_videoqueue_t *queue = NULL;
    lisaui_video_frame_desc_t frame_desc;
    queue = lisaui_userdata_get_camera_videoqueue();
    
    while (1) {
        EventBits_t bits = xEventGroupWaitBits(
            video_task_event_group,   
            VIDEO_TASK_RUN_BIT,       
            pdFALSE,                  
            pdTRUE,                   
            portMAX_DELAY);           
            
        if ((bits & VIDEO_TASK_RUN_BIT) == 0) {
            LISA_LOGI(LOG_TAG, "Video task blocked");
            continue;
        }
        
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
        }
        else{
            LISA_LOGE(LOG_TAG, "camera_dqbuf timeout");
        }
    }
    */
}

// 初始化摄像头视频系统
int video_camera_init(void)
{
    int ret = 0;
    
    /* 创建事件组 */
    // video_task_event_group = xEventGroupCreate();
    // if (video_task_event_group == NULL) {
    //     LISA_LOGE(LOG_TAG, "Failed to create video task event group");
    //     return -1;
    // }
    
    // /* 初始时清除运行位，任务将被阻塞 */
    // xEventGroupClearBits(video_task_event_group, VIDEO_TASK_RUN_BIT);

    // 摄像头配置（运行时初始化）
    camera_config_t camera_cfg = {
    .xclk_freq_hz = 12000000,
    .pixel_format = PIXFORMAT_RGB565,
    .frame_size   = FRAMESIZE_QQVGA,
    .buf_count    = 3,
    .colorbar     = 0,
    .i2c_config = {
        .i2c_dev = I2C0(),
        .pins = {
            .sda = {
                .pad = CSK_IOMUX_PAD_B,
                .pin = 6,
                .func = CSK_IOMUX_FUNC_ALTER8
            },
            .scl = {
                .pad = CSK_IOMUX_PAD_B,
                .pin = 7,
                .func = CSK_IOMUX_FUNC_ALTER8
            }
        }
    },
    .xfer_config = {
        .dvp_config = {
            .dvp_dev = DVP0(),
            .input_format = DVP_INPUT_FORM_YUV422_CRY0CBY1,
            .pck_polarity = DVP_POL_FALLING,
            .vs_polarity  = DVP_POL_FALLING,
            .hs_polarity  = DVP_POL_RISING,
            .data_align   = DVP_DATA_ALIGN_LEFT,
            .dma_channel  = gp_dma_ch3,
            .pins = {
                .hsync = {
                    .pad = CSK_IOMUX_PAD_A,
                    .pin = 10,
                    .func = CSK_IOMUX_FUNC_ALTER16
                },
                .vsync = {
                    .pad = CSK_IOMUX_PAD_A,
                    .pin = 11,
                    .func = CSK_IOMUX_FUNC_ALTER16
                },
                .pclk = {
                    .pad = CSK_IOMUX_PAD_A,
                    .pin = 12,
                    .func = CSK_IOMUX_FUNC_ALTER16
                },
                .data = {
                    {
                        .pad = CSK_IOMUX_PAD_A,
                        .pin = 13,
                        .func = CSK_IOMUX_FUNC_ALTER16
                    },
                    {
                        .pad = CSK_IOMUX_PAD_A,
                        .pin = 14,
                        .func = CSK_IOMUX_FUNC_ALTER16
                    },
                    {
                        .pad = CSK_IOMUX_PAD_A,
                        .pin = 15,
                        .func = CSK_IOMUX_FUNC_ALTER16
                    },
                    {
                        .pad = CSK_IOMUX_PAD_A,
                        .pin = 16,
                        .func = CSK_IOMUX_FUNC_ALTER16
                    },
                    {
                        .pad = CSK_IOMUX_PAD_A,
                        .pin = 17,
                        .func = CSK_IOMUX_FUNC_ALTER16
                    },
                    {
                        .pad = CSK_IOMUX_PAD_A,
                        .pin = 18,
                        .func = CSK_IOMUX_FUNC_ALTER16
                    },
                    {
                        .pad = CSK_IOMUX_PAD_A,
                        .pin = 19,
                        .func = CSK_IOMUX_FUNC_ALTER16
                    },
                    {
                        .pad = CSK_IOMUX_PAD_A,
                        .pin = 20,
                        .func = CSK_IOMUX_FUNC_ALTER16
                    }
                }
            }
        },
    },
    .hw_config = {
        .pin_config = {
            .pwdn = {
                .pad = CSK_IOMUX_PAD_B,
                .pin = 7,
                .func = CSK_IOMUX_FUNC_DEFAULT
            },
            .xclk_out = {
                .pad = CSK_IOMUX_PAD_A,
                .pin = 26,
                .func = CSK_IOMUX_FUNC_ALTER16
            }
        },
        .pwdn_delay_us = 10,
        .xclk_delay_us = 10,
    }
};
    camera_init(&camera_cfg);
    return ret;
}

// 启动视频捕获
int video_camera_start(void)
{
    camera_start();
    
    /* 设置事件位允许video_task运行 */
    // if (video_task_event_group != NULL) {
    //     xEventGroupSetBits(video_task_event_group, VIDEO_TASK_RUN_BIT);
    //     LISA_LOGI(LOG_TAG, "Video task unblocked");
    // }
    LISA_LOGI(LOG_TAG, "Video started");
    
    return 0;
}

// 停止视频捕获
int video_camera_stop(void)
{
    /* 清除事件位以阻塞video_task */
    camera_stop();
    
    // 视频队列功能暂时禁用
    /* 原始代码保留供参考
    lisaui_videoqueue_t *queue = NULL;
    lisaui_video_frame_desc_t frame_desc;
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
    */

    // if (video_task_event_group != NULL) {
    //     xEventGroupClearBits(video_task_event_group, VIDEO_TASK_RUN_BIT);
    //     LISA_LOGI(TAG, "Video task blocked");
    // }
    // LISA_LOGI(TAG, "Video stopped");    
    
    return 0;
}

int video_camera_capture_photo(uint8_t *out_data, size_t *out_size, uint32_t timeout_ms)
{
    if (!out_data || !out_size) {
        LISA_LOGE(LOG_TAG, "Invalid parameters");
        return -1;
    }

    LISA_LOGI(LOG_TAG, "Capturing photo with timeout %u ms", timeout_ms);

    // 启动摄像头
    camera_start();
    
    // 丢弃前3帧，等待摄像头稳定（曝光、白平衡调整）
    LISA_LOGI(LOG_TAG, "Waiting for camera to stabilize...");
    for (int i = 0; i < 3; i++) {
        struct cam_ipeg_mem *temp_mem = camera_dqbuf(NULL, timeout_ms);
        if (temp_mem != NULL) {
            camera_qbuf(temp_mem);  // 立即释放
            LISA_LOGI(LOG_TAG, "Dropped frame %d/3", i + 1);
        } else {
            LISA_LOGW(LOG_TAG, "Failed to get frame %d during stabilization", i + 1);
        }
    }
    
    // 获取稳定后的一帧数据
    LISA_LOGI(LOG_TAG, "Capturing stable frame...");
    struct cam_ipeg_mem *cam_mem = NULL;
    cam_mem = camera_dqbuf(cam_mem, timeout_ms);
    
    if (cam_mem == NULL) {
        LISA_LOGE(LOG_TAG, "Failed to capture photo: timeout");
        camera_stop();
        return -1;
    }

    // 返回缓冲区信息（直接返回摄像头缓冲区指针）
    *out_size = cam_mem->buf.size;
    memcpy(out_data, cam_mem->buf.addr, *out_size);

    LISA_LOGI(LOG_TAG, "Photo captured: size=%zu bytes", *out_size);
    
    // 立即释放缓冲区并停止摄像头
    camera_qbuf(cam_mem);
    camera_stop();
    
    LISA_LOGI(LOG_TAG, "Camera stopped");
    
    return 0;
}
