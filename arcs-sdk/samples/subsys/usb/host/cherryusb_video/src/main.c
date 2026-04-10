/*
 * Copyright (c) 2024, ListenAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>

#include "ClockManager.h"
#include "arcs_ap.h"

#include "FreeRTOS.h"
#include "task.h"

#include "usbh_core.h"
#include "usbh_video.h"

#include "sys_init.h"
#include "sysheap.h"
#include "video_display.h"
#include "usb_config.h"

/*
 * 首选视频格式：优先尝试此格式；若摄像头不支持，自动退回其他可用格式
 */
#define VIDEO_FORMAT_PREF    USBH_VIDEO_FORMAT_MJPEG

/* 最大同时支持的设备数，与 usb_config.h 保持一致 */
#define MAX_VIDEO_DEVICES CONFIG_USBHOST_MAX_VIDEO_CLASS

/* ------------------------------------------------------------------ */
/*  每设备上下文                                                        */
/* ------------------------------------------------------------------ */
struct video_dev_ctx {
    struct usbh_video *video_class;     /* CherryUSB 设备句柄，NULL 表示槽位空闲 */
    uint8_t           *frame_buf;       /* PSRAM 帧组装缓冲区 */
    uint8_t           *chunk_buf;       /* PSRAM DMA chunk 缓冲区 */
    volatile bool      running;         /* 采集任务正在运行 */
    volatile bool      disconnected;    /* 已收到断开信号 */
    TaskHandle_t       task_handle;     /* 采集任务句柄，用于 stop 时快速唤醒 */
    uint32_t           frame_count;
    uint32_t           total_bytes;
    uint16_t           width;           /* 实际协商的视频宽度（像素） */
    uint16_t           height;          /* 实际协商的视频高度（像素） */
};

static struct video_dev_ctx g_devs[MAX_VIDEO_DEVICES];

/* ------------------------------------------------------------------ */
/*  遍历设备格式表，找到首选格式的第一帧；不支持则退回任意可用格式     */
/* ------------------------------------------------------------------ */
static bool find_best_format(struct usbh_video *vc, uint8_t pref_fmt,
                              uint8_t *out_fmt, uint16_t *out_w, uint16_t *out_h)
{
    /* 优先找首选格式，取其第一帧 */
    for (uint8_t i = 0; i < vc->num_of_formats; i++) {
        if (vc->format[i].format_type == pref_fmt && vc->format[i].num_of_frames > 0) {
            *out_fmt = pref_fmt;
            *out_w   = vc->format[i].frame[0].wWidth;
            *out_h   = vc->format[i].frame[0].wHeight;
            return true;
        }
    }
    /* 退回：取任意格式的第一帧 */
    for (uint8_t i = 0; i < vc->num_of_formats; i++) {
        if (vc->format[i].num_of_frames > 0) {
            *out_fmt = vc->format[i].format_type;
            *out_w   = vc->format[i].frame[0].wWidth;
            *out_h   = vc->format[i].frame[0].wHeight;
            return true;
        }
    }
    return false;
}

/* ------------------------------------------------------------------ */
/*  帧回调：每收到一帧完整图像时被调用（从 USB 任务上下文触发）        */
/* ------------------------------------------------------------------ */
static void on_video_frame(struct usbh_video *video_class,
                           uint8_t *frame_buf,
                           uint32_t frame_size,
                           void *arg)
{
    struct video_dev_ctx *ctx = (struct video_dev_ctx *)arg;

    ctx->frame_count++;
    ctx->total_bytes += frame_size;

    if (ctx->frame_count % 30 == 0) {
        printf("[VIDEO%u] frame #%lu  size=%lu B  total=%lu KB\r\n",
               (unsigned)video_class->minor,
               (unsigned long)ctx->frame_count,
               (unsigned long)frame_size,
               (unsigned long)(ctx->total_bytes / 1024));
    }

    if (ctx->frame_count == 1 && frame_size >= 4) {
        printf("[VIDEO%u] first frame header: %02X %02X %02X %02X\r\n",
               (unsigned)video_class->minor,
               frame_buf[0], frame_buf[1], frame_buf[2], frame_buf[3]);
    }

    /* 送显示：使用实际协商的分辨率（CAM0→左半，CAM1→右半） */
    video_display_update(video_class->minor,
                         frame_buf, frame_size,
                         ctx->width, ctx->height,
                         video_class->current_format);
}

/* ------------------------------------------------------------------ */
/*  视频采集任务（每个设备独立一个任务）                                */
/* ------------------------------------------------------------------ */
static void video_stream_task(void *arg)
{
    struct video_dev_ctx *ctx = (struct video_dev_ctx *)arg;
    struct usbh_video    *video_class = ctx->video_class;
    int ret;
    uint8_t dev_idx = video_class->minor;

    /* 保存任务句柄，供 usbh_video_stop 快速唤醒 */
    ctx->task_handle = xTaskGetCurrentTaskHandle();

    /* 从设备描述符中动态选择最佳格式和分辨率，不依赖硬编码宽高 */
    uint8_t  actual_fmt;
    uint16_t actual_w, actual_h;
    if (!find_best_format(video_class, VIDEO_FORMAT_PREF, &actual_fmt, &actual_w, &actual_h)) {
        printf("[VIDEO%u] No valid format/frame in device descriptor\r\n", (unsigned)dev_idx);
        goto out_no_stream;
    }

    printf("[VIDEO%u] Opening device (format=%s %ux%u)...\r\n",
           (unsigned)dev_idx,
           (actual_fmt == USBH_VIDEO_FORMAT_MJPEG) ? "MJPEG" : "UNCOMPRESSED",
           (unsigned)actual_w, (unsigned)actual_h);

    ret = usbh_video_open(video_class, actual_fmt, actual_w, actual_h, 0);
    if (ret < 0) {
        printf("[VIDEO%u] Open failed: %d\r\n", (unsigned)dev_idx, ret);
        goto out_no_stream;
    }

    /* 保存协商到的实际分辨率，供帧回调传递给显示模块 */
    ctx->width  = actual_w;
    ctx->height = actual_h;

    if (!video_class->is_bulk) {
        printf("[VIDEO%u] ISO streaming not yet supported on MUSB, aborting\r\n",
               (unsigned)dev_idx);
        usbh_video_close(video_class);
        goto out_no_stream;
    }

    /* 根据协商后的实际分辨率和格式动态分配帧缓冲区：
     * UNCOMPRESSED(YUY2): width * height * 2 B
     * MJPEG: 按 UNCOMPRESSED 同等大小预留（MJPEG 压缩后一般更小，此为上限）
     */
    uint32_t frame_bufsize = (uint32_t)actual_w * actual_h * 2;
    ctx->frame_buf = psram_malloc_align(CONFIG_USB_ALIGN_SIZE, frame_bufsize);
    if (!ctx->frame_buf) {
        printf("[VIDEO%u] Failed to alloc frame_buf (%lu B)\r\n",
               (unsigned)dev_idx, (unsigned long)frame_bufsize);
        usbh_video_close(video_class);
        goto out_no_stream;
    }
    printf("[VIDEO%u] frame_buf allocated: %lu B (%ux%u)\r\n",
           (unsigned)dev_idx, (unsigned long)frame_bufsize,
           (unsigned)actual_w, (unsigned)actual_h);

    /* 从 PSRAM 分配 chunk 缓冲区 */
    uint32_t chunk_size = video_class->probe.dwMaxPayloadTransferSize;
    if (chunk_size == 0) {
        chunk_size = CONFIG_USBH_VIDEO_BULK_CHUNK_SIZE;
    }
    ctx->chunk_buf = psram_malloc_align(CONFIG_USB_ALIGN_SIZE, chunk_size);
    if (!ctx->chunk_buf) {
        printf("[VIDEO%u] Failed to allocate chunk_buf (%lu B)\r\n",
               (unsigned)dev_idx, (unsigned long)chunk_size);
        usbh_video_close(video_class);
        goto out_no_stream;
    }
    printf("[VIDEO%u] chunk_buf allocated: %lu B\r\n",
           (unsigned)dev_idx, (unsigned long)chunk_size);

    printf("[VIDEO%u] Starting bulk streaming...\r\n", (unsigned)dev_idx);

    ctx->frame_count = 0;
    ctx->total_bytes = 0;

    ret = usbh_video_start_streaming(video_class,
                                     ctx->frame_buf,
                                     frame_bufsize,
                                     ctx->chunk_buf,
                                     chunk_size,
                                     on_video_frame,
                                     ctx);
    if (ret < 0) {
        printf("[VIDEO%u] start_streaming failed: %d\r\n", (unsigned)dev_idx, ret);
        psram_free(ctx->chunk_buf);
        ctx->chunk_buf = NULL;
        usbh_video_close(video_class);
        goto out_no_stream;
    }

    printf("[VIDEO%u] Streaming started\r\n", (unsigned)dev_idx);

    /*
     * 等待设备断开通知。
     * usbh_video_stop() 会先调用 usbh_video_stop_streaming() 杀掉 URB，
     * 再调用 xTaskNotifyGive() 唤醒本任务，然后才返回给 usbh_video_class_free()。
     * 因此本任务被唤醒时 struct usbh_video 仍然有效，且 streaming 已停止。
     *
     * ulTaskNotifyTake 带超时用于定期打印统计信息；
     * 收到通知后若 disconnected 为真则立即退出循环。
     *
     * 若 streaming 因连续 URB 错误自动停止（设备从 Hub 拔出但 Hub 断开事件
     * 尚未到达），则静默等待 usbh_video_stop() 发出通知后再清理，
     * 避免打印混乱的统计信息。
     */
    while (!ctx->disconnected) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
        if (ctx->disconnected) {
            break;
        }
        if (!video_class->streaming) {
            /* streaming 因连续 URB 错误停止，等待 Hub 驱动发出断开通知 */
            printf("[VIDEO%u] Streaming stopped (device error), waiting for disconnect...\r\n",
                   (unsigned)dev_idx);
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            break;
        }
        printf("[VIDEO%u] Running: %lu frames, %lu KB total\r\n",
               (unsigned)dev_idx,
               (unsigned long)ctx->frame_count,
               (unsigned long)(ctx->total_bytes / 1024));
    }

    printf("[VIDEO%u] Device disconnected, cleaning up...\r\n", (unsigned)dev_idx);

    /*
     * streaming 已由 usbh_video_stop() 停止（URB 已 kill），
     * 此处仅释放 chunk_buf；usbh_video_close() 因 hport 可能已被清零而跳过。
     */
    psram_free(ctx->chunk_buf);
    ctx->chunk_buf = NULL;
    goto out;

out_no_stream:
    /* 没有进入 streaming 循环，chunk_buf 不需要额外处理 */
    ;

out:
    psram_free(ctx->frame_buf);
    ctx->frame_buf    = NULL;
    ctx->task_handle  = NULL;
    ctx->video_class  = NULL;
    ctx->running      = false;
    vTaskDelete(NULL);
}

/* ------------------------------------------------------------------ */
/*  弱符号覆盖：摄像头插入时由 CherryUSB 枚举框架调用                  */
/* ------------------------------------------------------------------ */
void usbh_video_run(struct usbh_video *video_class)
{
    uint8_t idx = video_class->minor;

    if (idx >= MAX_VIDEO_DEVICES) {
        printf("[VIDEO] minor=%u exceeds MAX_VIDEO_DEVICES=%d, ignored\r\n",
               (unsigned)idx, MAX_VIDEO_DEVICES);
        return;
    }

    struct video_dev_ctx *ctx = &g_devs[idx];

    if (ctx->running) {
        printf("[VIDEO%u] Already running, ignoring hot-plug\r\n", (unsigned)idx);
        return;
    }

    printf("[VIDEO%u] Camera connected: %s\r\n",
           (unsigned)idx,
           video_class->hport->config.intf[video_class->ctrl_intf].devname);
    usbh_video_list_info(video_class);

    ctx->video_class  = video_class;
    ctx->frame_buf    = NULL;
    ctx->chunk_buf    = NULL;
    ctx->task_handle  = NULL;
    ctx->running      = true;
    ctx->disconnected = false;
    ctx->frame_count  = 0;
    ctx->total_bytes  = 0;

    char task_name[16];
    snprintf(task_name, sizeof(task_name), "video%u", (unsigned)idx);

    xTaskCreate(video_stream_task, task_name, 4096, ctx,
                CONFIG_USBHOST_PSC_PRIO + 1, NULL);
}

/* ------------------------------------------------------------------ */
/*  弱符号覆盖：摄像头拔出时由 CherryUSB 枚举框架调用                  */
/*                                                                      */
/*  调用链：usbh_video_ctrl_disconnect()                                */
/*            → usbh_video_stop()       ← 本函数                       */
/*            → usbh_video_class_free() ← memset(video_class, 0)       */
/*                                                                      */
/*  必须在本函数返回前完成 URB kill，否则 usbh_video_class_free 清零     */
/*  bulkin_urb（含回调指针）后 MUSB 硬件触发中断会访问 NULL 回调 → crash */
/* ------------------------------------------------------------------ */
void usbh_video_stop(struct usbh_video *video_class)
{
    uint8_t idx = video_class->minor;

    if (idx >= MAX_VIDEO_DEVICES) {
        return;
    }

    printf("[VIDEO%u] Camera disconnected, stopping streaming\r\n", (unsigned)idx);

    /*
     * 关键步骤：立即终止 URB。
     * 必须在本函数返回前完成，确保 usbh_video_class_free() 清零 struct 时
     * MUSB 硬件已不再持有对 bulkin_urb 的引用。
     */
    usbh_video_stop_streaming(video_class);

    /*
     * 通知采集任务退出循环并释放资源。
     * 任务被唤醒时 streaming 已停止，可安全释放 chunk_buf。
     */
    struct video_dev_ctx *ctx = &g_devs[idx];
    ctx->disconnected = true;
    if (ctx->task_handle) {
        xTaskNotifyGive(ctx->task_handle);
    }

    /*
     * 短暂等待任务完成 PSRAM 资源释放（ctx->running → false）。
     * 无需等待太久，因为任务被立即唤醒且只做 psram_free 操作。
     */
    vTaskDelay(pdMS_TO_TICKS(50));
}

/* ------------------------------------------------------------------ */
/*  USB 事件回调（可选）                                                */
/* ------------------------------------------------------------------ */
static void usbh_event_handler(uint8_t busid, uint8_t hub_index, uint8_t hub_port,
                               uint8_t intf, uint8_t event)
{
    (void)busid;
    (void)hub_index;
    (void)hub_port;
    (void)intf;
    (void)event;
}

/* ------------------------------------------------------------------ */
/*  main                                                                */
/* ------------------------------------------------------------------ */
int main(void)
{
    printf("\r\n");
    printf("========================================\r\n");
    printf("  CherryUSB Host Video Example\r\n");
    printf("  Preferred format: %s (auto-negotiated from device)\r\n",
           (VIDEO_FORMAT_PREF == USBH_VIDEO_FORMAT_MJPEG) ? "MJPEG" : "UNCOMPRESSED");
    printf("  Max devices: %d\r\n", MAX_VIDEO_DEVICES);
    printf("========================================\r\n\r\n");

    printf("[INFO] Initializing display...\r\n");
    video_display_init();

    printf("[INFO] Initializing USB Host...\r\n");
    usbh_initialize(0, 0x41000000UL, usbh_event_handler);
    printf("[INFO] USB Host initialized, please connect USB cameras\r\n\r\n");

    int count = 0;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        count++;

        int active = 0;
        for (int i = 0; i < MAX_VIDEO_DEVICES; i++) {
            if (g_devs[i].running) {
                active++;
            }
        }
        if (active == 0) {
            printf("[%d] Waiting for USB camera...\r\n", count);
        }
    }

    return 0;
}

/* ------------------------------------------------------------------ */
/*  USB PHY 硬件初始化（Host 模式）                                     */
/* ------------------------------------------------------------------ */
static int usb_host_init(void)
{
    IP_CMN_SYS->REG_USB_CTRL1.bit.USBPHY_OUTCLKSEL = 0x1;
    IP_SYSCTRL->REG_PERI_CLK_CFG6.bit.ENA_USB_CLK = 0x1;

    IP_CMN_SYS->REG_USB_CTRL1.bit.USBC_CFG_IDDIG = 0x0;    /* Host 模式 (ID=0) */
    IP_CMN_SYS->REG_USB_CTRL1.bit.UTMI_DATABUS16_8 = 0x1;  /* 16 位模式 */

    return 0;
}

SYS_INIT(usb_host_init, SYS_INIT_LEVEL_PRE_DEVICES_INIT, 0);
