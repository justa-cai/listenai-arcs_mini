/**
 * @file uvc_stream.c
 * @brief USB UVC 视频流输出封装。
 *
 * @details
 * 本文件基于 CherryUSB UVC 设备栈，将摄像头 YUY2 帧拷贝到 UVC 帧缓存，
 * 按配置帧率通过 USB isochronous 端点发送到主机；可选支持把人脸检测框
 * 绘制到 UVC 预览画面上。
 */

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "sysheap.h"
#include "usbd_core.h"
#include "usbd_video.h"
#include "venusa_ap.h"

#include "uvc_stream.h"

#define TAG "uvc_stream"
#include "lisa_log.h"

#define UVC_BUS_ID         0      /* UVC 使用的 USB 总线 ID。 */
#define UVC_VIDEO_IN_EP    0x81   /* UVC 视频数据输入端点地址。 */
#define UVC_VIDEO_INT_EP   0x83   /* UVC 中断端点地址。 */
#define UVC_USBD_VID       0xffff /* USB 设备 VID/PID。 */
#define UVC_USBD_PID       0x0001
#define UVC_USBD_MAX_POWER 100  /* USB 配置描述符中的最大功耗，单位 mA。 */
#define UVC_LANGID_STRING  1033 /* 英文字符串描述符语言 ID。 */

#define UVC_BYTES_PER_PIXEL 2U   /* YUY2 每像素字节数。 */
#define UVC_TASK_STACK_SIZE 4096 /* UVC 发送任务栈大小。 */
#define UVC_OPEN_SETTLE_MS  120U /* 主机打开 UVC 后等待链路稳定的时间。 */
/* UVC 提交帧的最小时间间隔。 */
#define UVC_FRAME_PERIOD_MS ((1000U + CONFIG_FACE_DETECT_UVC_FPS - 1U) / CONFIG_FACE_DETECT_UVC_FPS)

#ifdef CONFIG_USB_HS
#define UVC_MAX_PAYLOAD_SIZE 1024U /* 高速 USB 单包最大负载。 */
#else
#define UVC_MAX_PAYLOAD_SIZE 1020U /* 全速 USB 单包最大负载。 */
#endif

/* UVC isochronous 端点包大小字段。 */
#define UVC_VIDEO_PACKET_SIZE ((unsigned int)((UVC_MAX_PAYLOAD_SIZE) | (0x00U << 11)))
#define UVC_WIDTH             ((uint32_t)CONFIG_IMAGE_WIDTH)  /* UVC 输出图像宽度。 */
#define UVC_HEIGHT            ((uint32_t)CONFIG_IMAGE_HEIGHT) /* UVC 输出图像高度。 */
/* 单帧 UVC 图像字节数。 */
#define UVC_FRAME_SIZE        (UVC_WIDTH * UVC_HEIGHT * UVC_BYTES_PER_PIXEL)
/* UVC 帧间隔描述符值，单位 100ns。 */
#define UVC_FRAME_INTERVAL    ((unsigned long)(10000000UL / CONFIG_FACE_DETECT_UVC_FPS))
/* UVC 描述符中声明的码率。 */
#define UVC_BIT_RATE          ((unsigned long)(UVC_WIDTH * UVC_HEIGHT * 16UL * CONFIG_FACE_DETECT_UVC_FPS))
/* Video Streaming 类特定描述符总长度。 */
#define UVC_VS_HEADER_SIZE                                                                                             \
    ((unsigned int)(VIDEO_SIZEOF_VS_INPUT_HEADER_DESC(1, 1) + VIDEO_SIZEOF_VS_FORMAT_UNCOMPRESSED_DESC +               \
                    VIDEO_SIZEOF_VS_FRAME_UNCOMPRESSED_DESC(1)))
/* USB 配置描述符总长度。 */
#define UVC_USB_DESC_SIZE ((unsigned long)(9 + VIDEO_VC_NOEP_DESCRIPTOR_LEN + 9 + UVC_VS_HEADER_SIZE + 6 + 9 + 7))

/* USB 设备描述符。 */
static const uint8_t uvc_device_descriptor[] = {
    USB_DEVICE_DESCRIPTOR_INIT(USB_2_0, 0xef, 0x02, 0x01, UVC_USBD_VID, UVC_USBD_PID, 0x0001, 0x01)};

/* USB 配置描述符，包含 UVC 控制接口、流接口和视频端点。 */
static const uint8_t uvc_config_descriptor[] = {
    USB_CONFIG_DESCRIPTOR_INIT(UVC_USB_DESC_SIZE, 0x02, 0x01, USB_CONFIG_BUS_POWERED, UVC_USBD_MAX_POWER),
    VIDEO_VC_NOEP_DESCRIPTOR_INIT(0x00, UVC_VIDEO_INT_EP, 0x0100, VIDEO_VC_TERMINAL_LEN, 48000000, 0x02),
    VIDEO_VS_DESCRIPTOR_INIT(0x01, 0x00, 0x00),
    VIDEO_VS_INPUT_HEADER_DESCRIPTOR_INIT(0x01, UVC_VS_HEADER_SIZE, UVC_VIDEO_IN_EP, 0x00),
    VIDEO_VS_FORMAT_UNCOMPRESSED_DESCRIPTOR_INIT(0x01, 0x01, VIDEO_GUID_YUY2),
    VIDEO_VS_FRAME_UNCOMPRESSED_DESCRIPTOR_INIT(0x01, UVC_WIDTH, UVC_HEIGHT, UVC_BIT_RATE, UVC_BIT_RATE, UVC_FRAME_SIZE,
                                                DBVAL(UVC_FRAME_INTERVAL), 0x01, DBVAL(UVC_FRAME_INTERVAL)),
    VIDEO_VS_COLOR_MATCHING_DESCRIPTOR_INIT(),
    VIDEO_VS_DESCRIPTOR_INIT(0x01, 0x01, 0x01),
    USB_ENDPOINT_DESCRIPTOR_INIT(UVC_VIDEO_IN_EP, 0x05, UVC_VIDEO_PACKET_SIZE, 0x01),
};

/* USB Device Qualifier 描述符。 */
static const uint8_t uvc_device_quality_descriptor[] = {
    0x0a, USB_DESCRIPTOR_TYPE_DEVICE_QUALIFIER, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00,
};

/* USB 字符串描述符。 */
static const char *uvc_string_descriptors[] = {
    (const char[]){0x09, 0x04},
    "ListenAI",
    "VenusA Face Detect UVC",
    "2025063001",
};

static uint8_t *uvc_frame_buffer;               /* UVC 当前待发送帧缓存。 */
static uint32_t uvc_frame_length;               /* UVC 当前待发送帧长度。 */
static volatile bool uvc_initialized;           /* UVC 模块是否完成初始化。 */
static volatile bool uvc_configured;            /* USB 主机是否完成 UVC 配置。 */
static volatile bool uvc_opened;                /* UVC 视频流是否已被主机打开。 */
static volatile bool uvc_tx_busy;               /* UVC 端点当前是否有发送事务进行中。 */
static volatile bool uvc_frame_pending;         /* 是否已有一帧等待 UVC 任务发送。 */
static volatile bool uvc_copying;               /* 是否正在向 UVC 帧缓存拷贝图像。 */
static volatile TickType_t uvc_open_ready_tick; /* UVC 打开后允许开始发送的 tick。 */
static volatile uint32_t uvc_state_generation;  /* UVC 连接/配置状态变更计数，用于丢弃过期发送。 */
static volatile uint32_t uvc_tx_generation;     /* 当前发送事务对应的状态变更计数。 */
static volatile uint32_t uvc_open_count;        /* UVC 打开次数统计。 */
static volatile uint32_t uvc_close_count;       /* UVC 关闭次数统计。 */
static volatile uint32_t uvc_start_count;       /* UVC 启动发送成功次数统计。 */
static volatile uint32_t uvc_start_fail_count;  /* UVC 启动发送失败次数统计。 */
static TickType_t uvc_last_submit_tick;         /* 上一次接受 UVC 帧提交的 tick。 */

/* UVC 分包发送使用的 USB 非缓存包缓冲区。 */
static USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t uvc_packet_buffer[UVC_MAX_PAYLOAD_SIZE];

/* UVC 控制接口和流接口实例。 */
static struct usbd_interface uvc_intf0;
static struct usbd_interface uvc_intf1;

#if CONFIG_FACE_DETECT_UVC_DRAW_FACE_BOX
/**
 * @brief YUV 颜色值。
 */
typedef struct {
    uint8_t y; /* 亮度分量。 */
    uint8_t u; /* U 色度分量。 */
    uint8_t v; /* V 色度分量。 */
} uvc_yuv_color_t;

/**
 * @brief 待绘制的人脸框。
 */
typedef struct {
    int32_t x;      /* 人脸框左上角 x 坐标。 */
    int32_t y;      /* 人脸框左上角 y 坐标。 */
    int32_t w;      /* 人脸框宽度。 */
    int32_t h;      /* 人脸框高度。 */
    bool highlight; /* 是否高亮显示最大面积人脸。 */
} uvc_face_box_t;

/**
 * @brief UVC 人脸框叠加层状态。
 */
typedef struct {
    uint32_t count;                                /* 当前有效人脸框数量。 */
    uint32_t source_width;                         /* 人脸框来源图像宽度。 */
    uint32_t source_height;                        /* 人脸框来源图像高度。 */
    TickType_t update_tick;                        /* 最近一次更新 tick。 */
    uvc_face_box_t boxes[ACOMP_FD_MAX_RESULT_CNT]; /* 人脸框数组。 */
} uvc_face_overlay_t;

/* 当前人脸框叠加层，由临界区保护。 */
static uvc_face_overlay_t uvc_face_overlay;

/* 普通人脸框颜色：黄色。 */
static const uvc_yuv_color_t uvc_color_yellow = {
    .y = 226,
    .u = 1,
    .v = 149,
};

/* 最大面积人脸框颜色：绿色。 */
static const uvc_yuv_color_t uvc_color_green = {
    .y = 150,
    .u = 44,
    .v = 21,
};
#endif

/**
 * @brief 获取 USB 设备描述符。
 *
 * @param speed USB 工作速度，当前未使用。
 *
 * @return USB 设备描述符指针。
 */
static const uint8_t *uvc_device_descriptor_callback(uint8_t speed)
{
    (void)speed;
    return uvc_device_descriptor;
}

/**
 * @brief 获取 USB 配置描述符。
 *
 * @param speed USB 工作速度，当前未使用。
 *
 * @return USB 配置描述符指针。
 */
static const uint8_t *uvc_config_descriptor_callback(uint8_t speed)
{
    (void)speed;
    return uvc_config_descriptor;
}

/**
 * @brief 获取 USB Device Qualifier 描述符。
 *
 * @param speed USB 工作速度，当前未使用。
 *
 * @return USB Device Qualifier 描述符指针。
 */
static const uint8_t *uvc_device_quality_descriptor_callback(uint8_t speed)
{
    (void)speed;
    return uvc_device_quality_descriptor;
}

/**
 * @brief 获取指定索引的 USB 字符串描述符。
 *
 * @param speed USB 工作速度，当前未使用。
 * @param index 字符串描述符索引。
 *
 * @return 字符串描述符指针；索引无效时返回 NULL。
 */
static const char *uvc_string_descriptor_callback(uint8_t speed, uint8_t index)
{
    (void)speed;

    if (index >= (sizeof(uvc_string_descriptors) / sizeof(uvc_string_descriptors[0]))) {
        return NULL;
    }

    return uvc_string_descriptors[index];
}

/* CherryUSB 描述符回调集合。 */
static const struct usb_descriptor uvc_descriptor = {
    .device_descriptor_callback = uvc_device_descriptor_callback,
    .config_descriptor_callback = uvc_config_descriptor_callback,
    .device_quality_descriptor_callback = uvc_device_quality_descriptor_callback,
    .string_descriptor_callback = uvc_string_descriptor_callback,
};

/**
 * @brief 丢弃待发送帧并复位 UVC 发送状态。
 */
static void uvc_drop_pending_frame(void)
{
    uvc_frame_pending = false;
    uvc_copying = false;
    uvc_tx_busy = false;
}

/**
 * @brief 判断当前 tick 是否已经到达目标 tick。
 *
 * @param now 当前 tick。
 * @param target 目标 tick。
 *
 * @retval true  当前时间已经到达或超过目标时间。
 * @retval false 当前时间尚未到达目标时间。
 */
static bool uvc_tick_reached(TickType_t now, TickType_t target)
{
    return ((int32_t)(now - target) >= 0);
}

#if CONFIG_FACE_DETECT_UVC_DRAW_FACE_BOX
/**
 * @brief 清空 UVC 人脸框叠加层。
 *
 * @param now 当前 tick，用作本次清空的更新时间。
 */
static void uvc_face_overlay_clear(TickType_t now)
{
    taskENTER_CRITICAL();
    uvc_face_overlay.count = 0;
    uvc_face_overlay.source_width = UVC_WIDTH;
    uvc_face_overlay.source_height = UVC_HEIGHT;
    uvc_face_overlay.update_tick = now;
    taskEXIT_CRITICAL();
}

/**
 * @brief 校验人脸检测结果是否可用于 UVC 叠框。
 *
 * @param info 人脸检测结果指针。
 * @param info_len 人脸检测结果数据长度，单位字节。
 *
 * @retval true  结果数量、最大面积索引和长度均合法。
 * @retval false 结果为空或字段不合法。
 */
static bool uvc_face_result_valid(const acomp_fd_result_info_t *info, uint32_t info_len)
{
    uint32_t expected_len;

    if ((info == NULL) || (info_len < sizeof(acomp_fd_result_info_t))) {
        return false;
    }

    if (info->results_cnt > ACOMP_FD_MAX_RESULT_CNT) {
        return false;
    }

    if ((info->results_cnt > 0U) && (info->max_area_results_index >= info->results_cnt)) {
        return false;
    }

    expected_len = sizeof(acomp_fd_result_info_t) + sizeof(acomp_fd_result_t) * info->results_cnt;
    return info_len >= expected_len;
}

/**
 * @brief 获取一份当前人脸框叠加层快照。
 *
 * @param snapshot 输出叠加层快照。
 * @param now 当前 tick，用于判断叠框是否超时。
 *
 * @retval true  快照有效，可用于绘制。
 * @retval false 参数无效、无有效人脸框或叠框已超时。
 */
static bool uvc_face_overlay_snapshot(uvc_face_overlay_t *snapshot, TickType_t now)
{
    if (snapshot == NULL) {
        return false;
    }

    taskENTER_CRITICAL();
    *snapshot = uvc_face_overlay;
    taskEXIT_CRITICAL();

    if ((snapshot->count == 0U) || (snapshot->source_width == 0U) || (snapshot->source_height == 0U)) {
        return false;
    }

    if ((CONFIG_FACE_DETECT_UVC_BOX_TIMEOUT_MS > 0) &&
        uvc_tick_reached(now, snapshot->update_tick + pdMS_TO_TICKS(CONFIG_FACE_DETECT_UVC_BOX_TIMEOUT_MS))) {
        return false;
    }

    return true;
}

/**
 * @brief 按源/目标尺寸比例缩放坐标值。
 *
 * @param value 源坐标值。
 * @param dst 目标尺寸。
 * @param src 源尺寸。
 *
 * @return 缩放后的目标坐标值。
 */
static int32_t uvc_scale_value(int32_t value, uint32_t dst, uint32_t src)
{
    return (int32_t)(((int64_t)value * (int64_t)dst) / (int64_t)src);
}

/**
 * @brief 将矩形边界裁剪到目标图像范围内。
 *
 * @param x0 输入/输出左边界。
 * @param y0 输入/输出上边界。
 * @param x1 输入/输出右边界。
 * @param y1 输入/输出下边界。
 * @param width 目标图像宽度。
 * @param height 目标图像高度。
 *
 * @retval true  裁剪后矩形仍有效。
 * @retval false 参数无效或矩形完全位于图像外部。
 */
static bool uvc_rect_clip(int32_t *x0, int32_t *y0, int32_t *x1, int32_t *y1, uint32_t width, uint32_t height)
{
    if ((x0 == NULL) || (y0 == NULL) || (x1 == NULL) || (y1 == NULL) || (width == 0U) || (height == 0U)) {
        return false;
    }

    if ((*x1 < 0) || (*y1 < 0) || (*x0 >= (int32_t)width) || (*y0 >= (int32_t)height)) {
        return false;
    }

    if (*x0 < 0) {
        *x0 = 0;
    }
    if (*y0 < 0) {
        *y0 = 0;
    }
    if (*x1 >= (int32_t)width) {
        *x1 = (int32_t)width - 1;
    }
    if (*y1 >= (int32_t)height) {
        *y1 = (int32_t)height - 1;
    }

    return (*x0 <= *x1) && (*y0 <= *y1);
}

/**
 * @brief 在 YUY2 帧缓存中设置单个像素颜色。
 *
 * @param buffer YUY2 图像缓存。
 * @param width 图像宽度。
 * @param height 图像高度。
 * @param x 像素 x 坐标。
 * @param y 像素 y 坐标。
 * @param color YUV 颜色。
 */
static void uvc_yuy2_set_pixel(uint8_t *buffer, uint32_t width, uint32_t height, int32_t x, int32_t y,
                               const uvc_yuv_color_t *color)
{
    uint32_t pair_x;
    uint32_t offset;

    if ((buffer == NULL) || (color == NULL) || (x < 0) || (y < 0) || ((uint32_t)x >= width) ||
        ((uint32_t)y >= height)) {
        return;
    }

    pair_x = ((uint32_t)x) & ~1U;
    if ((pair_x + 1U) >= width) {
        return;
    }

    offset = (((uint32_t)y * width) + pair_x) * UVC_BYTES_PER_PIXEL;
    if ((((uint32_t)x) & 1U) == 0U) {
        buffer[offset] = color->y;
    } else {
        buffer[offset + 2U] = color->y;
    }
    buffer[offset + 1U] = color->u;
    buffer[offset + 3U] = color->v;
}

/**
 * @brief 绘制水平线段。
 *
 * @param buffer YUY2 图像缓存。
 * @param width 图像宽度。
 * @param height 图像高度。
 * @param x0 起点 x 坐标。
 * @param x1 终点 x 坐标。
 * @param y 线段 y 坐标。
 * @param color YUV 颜色。
 */
static void uvc_draw_hline(uint8_t *buffer, uint32_t width, uint32_t height, int32_t x0, int32_t x1, int32_t y,
                           const uvc_yuv_color_t *color)
{
    for (int32_t x = x0; x <= x1; x++) {
        uvc_yuy2_set_pixel(buffer, width, height, x, y, color);
    }
}

/**
 * @brief 绘制垂直线段。
 *
 * @param buffer YUY2 图像缓存。
 * @param width 图像宽度。
 * @param height 图像高度。
 * @param x 线段 x 坐标。
 * @param y0 起点 y 坐标。
 * @param y1 终点 y 坐标。
 * @param color YUV 颜色。
 */
static void uvc_draw_vline(uint8_t *buffer, uint32_t width, uint32_t height, int32_t x, int32_t y0, int32_t y1,
                           const uvc_yuv_color_t *color)
{
    for (int32_t y = y0; y <= y1; y++) {
        uvc_yuy2_set_pixel(buffer, width, height, x, y, color);
    }
}

/**
 * @brief 在 UVC 图像上绘制一个人脸框。
 *
 * @param buffer YUY2 图像缓存。
 * @param width UVC 图像宽度。
 * @param height UVC 图像高度。
 * @param box 待绘制的人脸框。
 * @param source_width 人脸框来源图像宽度。
 * @param source_height 人脸框来源图像高度。
 */
static void uvc_draw_rect(uint8_t *buffer, uint32_t width, uint32_t height, const uvc_face_box_t *box,
                          uint32_t source_width, uint32_t source_height)
{
    const uvc_yuv_color_t *color;
    int32_t x0;
    int32_t y0;
    int32_t x1;
    int32_t y1;
    uint32_t thickness;

    if ((box == NULL) || (box->w <= 0) || (box->h <= 0) || (source_width == 0U) || (source_height == 0U)) {
        return;
    }

    x0 = uvc_scale_value(box->x, width, source_width);
    y0 = uvc_scale_value(box->y, height, source_height);
    x1 = uvc_scale_value(box->x + box->w - 1, width, source_width);
    y1 = uvc_scale_value(box->y + box->h - 1, height, source_height);
    if (!uvc_rect_clip(&x0, &y0, &x1, &y1, width, height)) {
        return;
    }

    color = box->highlight ? &uvc_color_green : &uvc_color_yellow;
    thickness = CONFIG_FACE_DETECT_UVC_BOX_THICKNESS;
    for (uint32_t i = 0; i < thickness; i++) {
        int32_t top = y0 + (int32_t)i;
        int32_t bottom = y1 - (int32_t)i;
        int32_t left = x0 + (int32_t)i;
        int32_t right = x1 - (int32_t)i;

        if ((top > bottom) || (left > right)) {
            break;
        }

        uvc_draw_hline(buffer, width, height, left, right, top, color);
        if (bottom != top) {
            uvc_draw_hline(buffer, width, height, left, right, bottom, color);
        }
        uvc_draw_vline(buffer, width, height, left, top, bottom, color);
        if (right != left) {
            uvc_draw_vline(buffer, width, height, right, top, bottom, color);
        }
    }
}

/**
 * @brief 将当前人脸框叠加层绘制到 UVC 图像缓存。
 *
 * @param buffer YUY2 图像缓存。
 * @param width UVC 图像宽度。
 * @param height UVC 图像高度。
 * @param now 当前 tick，用于判断人脸框是否超时。
 */
static void uvc_draw_face_overlay(uint8_t *buffer, uint32_t width, uint32_t height, TickType_t now)
{
    uvc_face_overlay_t overlay;

    if (!uvc_face_overlay_snapshot(&overlay, now)) {
        return;
    }

    for (uint32_t i = 0; i < overlay.count; i++) {
        uvc_draw_rect(buffer, width, height, &overlay.boxes[i], overlay.source_width, overlay.source_height);
    }
}
#endif

/**
 * @brief USB 设备事件回调。
 *
 * @details
 * 当 USB 复位、断开、配置、挂起等状态发生变化时，更新 UVC 状态并丢弃
 * 正在等待或发送中的过期帧。
 *
 * @param busid USB 总线 ID。
 * @param event USB 设备事件。
 */
static void uvc_usbd_event_handler(uint8_t busid, uint8_t event)
{
    if (busid != UVC_BUS_ID) {
        return;
    }

    switch (event) {
    case USBD_EVENT_RESET:
    case USBD_EVENT_DISCONNECTED:
        uvc_configured = false;
        uvc_opened = false;
        uvc_state_generation++;
        uvc_drop_pending_frame();
        break;
    case USBD_EVENT_CONFIGURED:
        uvc_configured = true;
        uvc_state_generation++;
        uvc_drop_pending_frame();
        break;
    case USBD_EVENT_SUSPEND:
        uvc_opened = false;
        uvc_state_generation++;
        uvc_drop_pending_frame();
        break;
    case USBD_EVENT_RESUME:
    case USBD_EVENT_CONNECTED:
    case USBD_EVENT_SET_REMOTE_WAKEUP:
    case USBD_EVENT_CLR_REMOTE_WAKEUP:
    default:
        break;
    }
}

/**
 * @brief CherryUSB 视频流打开回调。
 *
 * @param busid USB 总线 ID。
 * @param intf 被打开的视频接口号。
 */
void usbd_video_open(uint8_t busid, uint8_t intf)
{
    TickType_t now;

    if (busid != UVC_BUS_ID) {
        return;
    }

    now = xTaskGetTickCountFromISR();
    uvc_state_generation++;
    uvc_opened = true;
    uvc_drop_pending_frame();
    uvc_last_submit_tick = 0;
    uvc_open_ready_tick = now + pdMS_TO_TICKS(UVC_OPEN_SETTLE_MS);
    uvc_open_count++;
    LISA_LOGI(TAG, "UVC open intf:%u gen:%u open:%u close:%u", intf, uvc_state_generation, uvc_open_count,
              uvc_close_count);
}

/**
 * @brief CherryUSB 视频流关闭回调。
 *
 * @param busid USB 总线 ID。
 * @param intf 被关闭的视频接口号。
 */
void usbd_video_close(uint8_t busid, uint8_t intf)
{
    TickType_t now;

    if (busid != UVC_BUS_ID) {
        return;
    }

    now = xTaskGetTickCountFromISR();
    uvc_state_generation++;
    uvc_opened = false;
    uvc_drop_pending_frame();
    uvc_open_ready_tick = now + pdMS_TO_TICKS(UVC_OPEN_SETTLE_MS);
    uvc_close_count++;
    LISA_LOGI(TAG, "UVC close intf:%u gen:%u open:%u close:%u", intf, uvc_state_generation, uvc_open_count,
              uvc_close_count);
}

/**
 * @brief UVC 视频输入端点传输完成回调。
 *
 * @param busid USB 总线 ID。
 * @param ep 端点地址。
 * @param nbytes 本次传输字节数。
 */
static void uvc_video_iso_callback(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    if ((busid != UVC_BUS_ID) || (ep != UVC_VIDEO_IN_EP)) {
        return;
    }

    if ((nbytes == 0) || usbd_video_stream_split_transfer(busid, ep)) {
        uvc_tx_busy = false;
    }
}

/* UVC 视频输入端点对象。 */
static struct usbd_endpoint uvc_video_in_ep = {
    .ep_cb = uvc_video_iso_callback,
    .ep_addr = UVC_VIDEO_IN_EP,
};

/**
 * @brief UVC 发送任务。
 *
 * @details
 * 任务轮询 UVC 状态，在主机已配置、视频流已打开、存在待发送帧且端点空闲时，
 * 启动一次 UVC 分包发送。发送开始前后通过 generation 检查过滤 USB 状态变化
 * 导致的过期发送请求。
 *
 * @param arg FreeRTOS 任务参数，当前未使用。
 */
static void uvc_stream_task(void *arg)
{
    (void)arg;

    while (1) {
        bool start_transfer = false;
        uint32_t frame_len = 0;
        uint32_t start_generation = 0;
        TickType_t now = xTaskGetTickCount();

        taskENTER_CRITICAL();
        if (uvc_opened && uvc_configured && uvc_frame_pending && !uvc_tx_busy && !uvc_copying &&
            uvc_tick_reached(now, uvc_open_ready_tick)) {
            uvc_tx_busy = true;
            uvc_frame_pending = false;
            frame_len = uvc_frame_length;
            start_generation = uvc_state_generation;
            uvc_tx_generation = start_generation;
            start_transfer = true;
        }
        taskEXIT_CRITICAL();

        if (start_transfer) {
            int ret;

            taskENTER_CRITICAL();
            if (!uvc_opened || !uvc_configured || (uvc_state_generation != start_generation)) {
                uvc_tx_busy = false;
                start_transfer = false;
            }
            taskEXIT_CRITICAL();

            if (start_transfer) {
                ret = usbd_video_stream_start_write(UVC_BUS_ID, UVC_VIDEO_IN_EP, uvc_packet_buffer, uvc_frame_buffer,
                                                    frame_len, true);
                if (ret < 0) {
                    uvc_start_fail_count++;
                    if ((uvc_start_fail_count & 0x0fU) == 1U) {
                        LISA_LOGW(TAG, "UVC start write failed:%d gen:%u fail:%u", ret, start_generation,
                                  uvc_start_fail_count);
                    }
                    taskENTER_CRITICAL();
                    if (uvc_tx_generation == start_generation) {
                        uvc_tx_busy = false;
                    }
                    taskEXIT_CRITICAL();
                } else {
                    uvc_start_count++;
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

/**
 * @brief 初始化 UVC 设备、描述符、端点和发送任务。
 *
 * @retval 0 初始化成功，或已经初始化。
 * @retval -ENOMEM 帧缓存分配失败或任务创建失败。
 * @return 其他负值为 USB 设备栈初始化失败返回码。
 */
int uvc_stream_init(void)
{
    BaseType_t task_ret;
    int ret;

    if (uvc_initialized) {
        return 0;
    }

    if (CONFIG_IMAGE_FORMAT != LISA_CAMERA_PIXFMT_YUV422) {
        LISA_LOGW(TAG, "UVC expects YUV422/YUY2 image format, current:%d", CONFIG_IMAGE_FORMAT);
    }

    uvc_frame_buffer = (uint8_t *)psram_malloc_align(32, UVC_FRAME_SIZE);
    if (uvc_frame_buffer == NULL) {
        LISA_LOGE(TAG, "alloc UVC frame buffer failed, size:%u", (unsigned)UVC_FRAME_SIZE);
        return -ENOMEM;
    }
    memset(uvc_frame_buffer, 0, UVC_FRAME_SIZE);
    memset(uvc_packet_buffer, 0, sizeof(uvc_packet_buffer));

    usbd_desc_register(UVC_BUS_ID, &uvc_descriptor);
    usbd_add_interface(UVC_BUS_ID, usbd_video_init_intf(UVC_BUS_ID, &uvc_intf0, UVC_FRAME_INTERVAL, UVC_FRAME_SIZE,
                                                        UVC_MAX_PAYLOAD_SIZE));
    usbd_add_interface(UVC_BUS_ID, usbd_video_init_intf(UVC_BUS_ID, &uvc_intf1, UVC_FRAME_INTERVAL, UVC_FRAME_SIZE,
                                                        UVC_MAX_PAYLOAD_SIZE));
    usbd_add_endpoint(UVC_BUS_ID, &uvc_video_in_ep);

    task_ret = xTaskCreate(uvc_stream_task, "uvc_stream", UVC_TASK_STACK_SIZE, NULL, CONFIG_MAIN_TASK_PRIORITY, NULL);
    if (task_ret != pdPASS) {
        LISA_LOGE(TAG, "create UVC task failed:%ld", (long)task_ret);
        return -ENOMEM;
    }

    ret = usbd_initialize(UVC_BUS_ID, USBC_BASE, uvc_usbd_event_handler);
    if (ret < 0) {
        LISA_LOGE(TAG, "CherryUSB device init failed:%d", ret);
        return ret;
    }

    uvc_initialized = true;
    LISA_LOGI(TAG, "UVC ready: YUY2 %ux%u@%dfps, frame:%u payload:%u", (unsigned)UVC_WIDTH, (unsigned)UVC_HEIGHT,
              CONFIG_FACE_DETECT_UVC_FPS, (unsigned)UVC_FRAME_SIZE, (unsigned)UVC_MAX_PAYLOAD_SIZE);
    return 0;
}

/**
 * @brief 查询 UVC 视频流是否已经打开并可接收帧。
 *
 * @retval true  UVC 已初始化、已配置且视频流已打开。
 * @retval false UVC 尚未就绪。
 */
bool uvc_stream_is_open(void)
{
    return uvc_initialized && uvc_configured && uvc_opened;
}

/**
 * @brief 提交一帧摄像头图像到 UVC 输出。
 *
 * @details
 * 仅接受与 UVC 描述符一致的 YUV422/YUY2 图像。函数会按目标 FPS 做限速，
 * 在端点忙、已有待发送帧或帧缓存正在拷贝时返回 -EAGAIN。开启人脸框绘制时，
 * 会先把当前叠加层绘制到 UVC 帧缓存再交给发送任务。
 *
 * @param frame 待提交的摄像头帧。
 *
 * @retval 0 帧已接受并等待发送。
 * @retval -ENOTCONN UVC 尚未打开。
 * @retval -EINVAL 参数无效或帧格式/尺寸/长度不匹配。
 * @retval -EAGAIN 当前限速、端点忙或已有帧等待发送。
 */
int uvc_stream_submit_frame(const camera_frame_t *frame)
{
    TickType_t now;

    if (!uvc_stream_is_open()) {
        return -ENOTCONN;
    }

    if ((frame == NULL) || (frame->buffer == NULL)) {
        return -EINVAL;
    }

    if ((frame->format != LISA_CAMERA_PIXFMT_YUV422) || (frame->width != UVC_WIDTH) || (frame->height != UVC_HEIGHT) ||
        (frame->length < UVC_FRAME_SIZE)) {
        return -EINVAL;
    }

    now = xTaskGetTickCount();
    if ((uvc_last_submit_tick != 0) && ((now - uvc_last_submit_tick) < pdMS_TO_TICKS(UVC_FRAME_PERIOD_MS))) {
        return -EAGAIN;
    }

    taskENTER_CRITICAL();
    if (uvc_tx_busy || uvc_frame_pending || uvc_copying) {
        taskEXIT_CRITICAL();
        return -EAGAIN;
    }
    uvc_copying = true;
    taskEXIT_CRITICAL();

    memcpy(uvc_frame_buffer, frame->buffer, UVC_FRAME_SIZE);
#if CONFIG_FACE_DETECT_UVC_DRAW_FACE_BOX
    uvc_draw_face_overlay(uvc_frame_buffer, UVC_WIDTH, UVC_HEIGHT, now);
#endif

    taskENTER_CRITICAL();
    if (uvc_opened && uvc_configured) {
        uvc_frame_length = UVC_FRAME_SIZE;
        uvc_frame_pending = true;
        uvc_last_submit_tick = now;
    }
    uvc_copying = false;
    taskEXIT_CRITICAL();

    return 0;
}

#if CONFIG_FACE_DETECT_UVC_DRAW_FACE_BOX
/**
 * @brief 更新 UVC 预览画面使用的人脸检测框信息。
 *
 * @details
 * 传入 NULL 会清空叠加层；传入合法人脸结果时，会把结果中的人脸矩形复制到
 * UVC 叠加层，并将最大面积人脸标记为高亮。
 *
 * @param info 人脸检测结果指针；为 NULL 时清空叠加层。
 * @param info_len 人脸检测结果长度，单位字节。
 *
 * @retval 0 更新成功或已清空叠加层。
 * @retval -EINVAL 人脸检测结果不合法。
 */
int uvc_stream_update_face_result(const acomp_fd_result_info_t *info, uint32_t info_len)
{
    uvc_face_overlay_t overlay;
    uint32_t count;
    TickType_t now = xTaskGetTickCount();

    if (info == NULL) {
        uvc_face_overlay_clear(now);
        return 0;
    }

    if (!uvc_face_result_valid(info, info_len)) {
        uvc_face_overlay_clear(now);
        return -EINVAL;
    }

    memset(&overlay, 0, sizeof(overlay));
    count = info->results_cnt;
    overlay.count = count;
    overlay.source_width = UVC_WIDTH;
    overlay.source_height = UVC_HEIGHT;
    overlay.update_tick = now;

    for (uint32_t i = 0; i < count; i++) {
        const acomp_fd_result_t *result = &info->results[i];

        overlay.boxes[i].x = result->face_rect.x;
        overlay.boxes[i].y = result->face_rect.y;
        overlay.boxes[i].w = result->face_rect.w;
        overlay.boxes[i].h = result->face_rect.h;
        overlay.boxes[i].highlight = (i == info->max_area_results_index);
    }

    taskENTER_CRITICAL();
    uvc_face_overlay = overlay;
    taskEXIT_CRITICAL();

    return 0;
}
#endif
