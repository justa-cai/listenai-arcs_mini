/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"

#include "workqueue.h"

#include "ClockManager.h"
#include "tusb.h"
#include "tusb.h"
#include "usb_descriptors.h"

#include "uvc_stream.h"

#define TAG "uvc_stream"
#include <lisa_log.h>

static workqueue_t *uvc_wq = NULL;

/* Static variables */
static bool is_streaming = false;
static volatile bool frame_sent = true;
static uint32_t frame_counter = 0;

/* Frame interval in 100ns units */
static uint32_t interval_ms = 0;

void tud_mount_cb(void)
{
    LOGI("USB mounted");
}

void tud_umount_cb(void)
{
    LOGI("USB unmounted");
    is_streaming = false;
}

void tud_suspend_cb(bool remote_wakeup_en)
{
    (void)remote_wakeup_en;
    is_streaming = false;
    LOGI("USB suspended");
}

void tud_resume_cb(void)
{
    LOGI("USB resumed");
}

/**
 * @brief Video control callback - called when host negotiates video parameters
 */
int tud_video_commit_cb(uint_fast8_t ctl_idx, uint_fast8_t stm_idx,
                        video_probe_and_commit_control_t const *param)
{
    (void)ctl_idx;
    (void)stm_idx;

    /* Update frame interval */
    interval_ms = param->dwFrameInterval / 10000;
    
    /* Start streaming when host requests it */
    is_streaming = true;
    
    LOGI("=== Video Commit Parameters ===");
    LOGI("  bmHint: 0x%02X", param->bmHint);
    LOGI("  bFormatIndex: %u", param->bFormatIndex);
    LOGI("  bFrameIndex: %u", param->bFrameIndex);
    LOGI("  dwFrameInterval: %u (100ns units) = %u fps", 
         param->dwFrameInterval, 10000000 / param->dwFrameInterval);
    LOGI("  wKeyFrameRate: %u", param->wKeyFrameRate);
    LOGI("  wPFrameRate: %u", param->wPFrameRate);
    LOGI("  wCompQuality: %u", param->wCompQuality);
    LOGI("  wCompWindowSize: %u", param->wCompWindowSize);
    LOGI("  wDelay: %u ms", param->wDelay);
    LOGI("  dwMaxVideoFrameSize: %u bytes", param->dwMaxVideoFrameSize);
    LOGI("  dwMaxPayloadTransferSize: %u bytes", param->dwMaxPayloadTransferSize);
    LOGI("  dwClockFrequency: %u Hz", param->dwClockFrequency);
    LOGI("  bmFramingInfo: 0x%02X", param->bmFramingInfo);
    LOGI("  bPreferedVersion: %u", param->bPreferedVersion);
    LOGI("  bMinVersion: %u", param->bMinVersion);
    LOGI("  bMaxVersion: %u", param->bMaxVersion);
    LOGI("  bUsage: %u", param->bUsage);
    LOGI("  bBitDepthLuma: %u", param->bBitDepthLuma);
    LOGI("  bmSettings: 0x%02X", param->bmSettings);
    LOGI("  bMaxNumberOfRefFramesPlus1: %u", param->bMaxNumberOfRefFramesPlus1);
    LOGI("  bmRateControlModes: 0x%04X", param->bmRateControlModes);
    LOGI("  bmLayoutPerStream: 0x%016llX", param->bmLayoutPerStream);
    LOGI("=== Streaming STARTED (interval=%u ms) ===", interval_ms);
    
    return 0;  /* Return 0 for success */
}

/**
 * @brief Video frame transfer complete callback
 */
void tud_video_frame_xfer_complete_cb(uint_fast8_t ctl_idx, uint_fast8_t stm_idx)
{
    (void)ctl_idx;
    (void)stm_idx;
    
    frame_sent = true;
    frame_counter++;
    
    static uint32_t last_time = 0;
    static uint32_t fps_counter = 0;
    static uint32_t fps_start_time = 0;
    
    uint32_t now = xTaskGetTickCount();
    uint32_t interval = now - last_time;
    last_time = now;
    
    // 每秒统计一次平均帧率
    fps_counter++;
    if (fps_start_time == 0) {
        fps_start_time = now;
    }
    
    uint32_t elapsed = now - fps_start_time;
    if (elapsed >= 1000) {
        float avg_fps = (float)fps_counter * 1000.0f / elapsed;
        LOGI("USB Transfer: interval=%u ms, instant_fps=%.1f, avg_fps=%.1f", 
             interval, 1000.0f / interval, avg_fps);
        fps_counter = 0;
        fps_start_time = now;
    }

    /* Notify the workqueue task that USB transfer is complete */
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(uvc_wq->taskHandle, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

static void user_usbd_init(void)
{
    //enable usb clock
    __HAL_CRM_USB_CLK_ENABLE();
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBC_CFG_IDDIG = 0x1; //Config "B" device
    IP_CMN_SYS->REG_USB_CTRL1.bit.UTMI_DATABUS16_8 = 0x1; //16bit mode

    tud_disconnect(); // soft-disconnect from host
    tusb_init();
    tud_connect(); // soft-connect to host

    LOGI("TinyUSB UVC class ready\n");
}

void usb_device_task(void *param) 
{
    (void) param;

    tusb_rhport_init_t dev_init = {
        .role = TUSB_ROLE_DEVICE,
        .speed = TUSB_SPEED_AUTO
    };
    tusb_init(BOARD_TUD_RHPORT, &dev_init);

    while (1) {
        tud_task();
    }
}

static bool uvc_stream_is_ready(void)
{
    bool streaming_state = tud_video_n_streaming(0, 0);
    if (!streaming_state && is_streaming) {
        LISA_LOGW(TAG, "UVC not ready: is_streaming=%d, tud_video_n_streaming=%d", 
                  is_streaming, streaming_state);
    }
    return is_streaming && streaming_state;
}

void uvc_wq_handler(void *para) 
{
    video_info_t *msg = (video_info_t*)para;

    if (!uvc_stream_is_ready()) {
        LISA_LOGD(TAG, "uvc_stream not ready");
        msg->func(msg->func_param);
        psram_free(msg);
        return;
    }

    LISA_LOGD(TAG, "uvc_wq buf:0x%p, len: %d, w:%d, h:%d, f:%d", 
                    msg->data, 
                    msg->len,
                    msg->width,
                    msg->height,
                    msg->format);
    
    /* Start USB transfer */
    bool result = tud_video_n_frame_xfer(0, 0, (void*) msg->data, msg->len);
    
    if (!result) {
        /* Transfer failed, clean up immediately */
        LISA_LOGE(TAG, "USB transfer failed!");
        is_streaming = false;
        msg->func(msg->func_param);
        psram_free(msg);
        return;
    }
    
    /* Wait for USB transfer completion notification (with timeout) */
    uint32_t notification = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));
    if (notification == 0) {
        /* Timeout - this should not happen */
        LISA_LOGE(TAG, "USB transfer timeout!");
        is_streaming = false;
    } else {
        LISA_LOGD(TAG, "USB transfer completed successfully");
    }

    msg->func(msg->func_param);
    psram_free(msg);
}

void uvc_stream_send_data(video_info_t *info)
{
    int ret;

    video_info_t *msg = psram_malloc(sizeof(video_info_t));
    memcpy(msg, info, sizeof(video_info_t));

    ret = workqueue_submit(uvc_wq, uvc_wq_handler, msg);
    if (ret == 0) {
        LISA_LOGE(TAG, "workqueue submit failed:%d", ret);
    }
}

int uvc_stream_init(void)
{
    /* Initialize TinyUSB */
    user_usbd_init();

    xTaskCreate(usb_device_task, "usbd", 4096, NULL, configMAX_PRIORITIES - 1, NULL);

    uvc_wq = workqueue_create("uvc_wq", 9, 10, 8096);
    if (uvc_wq == NULL) {
        LISA_LOGE(TAG, "Failed to create uvc_wq");
    }
    return 0;
}
