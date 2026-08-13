/**
 * @file fd_image_stream.c
 * @brief 人脸检测图像输入流封装。
 *
 * @details
 * 本文件负责把摄像头输出的 @ref video_frame_t 转换为 ACOMP 人脸检测组件
 * 需要的输入帧格式，并通过 ACOMP stream 通道发送到远端算法侧。
 */

#include "stdio.h"
#include <errno.h>
#include <string.h>
#include <stdbool.h>

#include "acomp_fd.h"
#include "fd_image_stream.h"

/* 当前模块日志标签。 */
#define TAG "fd_image_stream"
#include "lisa_log.h"

/* 人脸检测图像输入流通道索引。 */
#define FD_IMAGE_FRAME_STREAM_CH_INDEX (0)
/* 人脸检测图像输入流通道名称。 */
#define FD_IMAGE_FRAME_STREAM_CH_CNAME "stream.fd_image"
/* 单帧图像载荷最大大小，按 YUV422/RGB565 每像素 2 字节估算。 */
#define FD_IMAGE_FRAME_PAYLOAD_SIZE    (CONFIG_IMAGE_WIDTH * CONFIG_IMAGE_HEIGHT * 2)
/* stream 缓冲区大小，包含输入帧头和图像载荷。 */
#define FD_IMAGE_FRAME_STREAM_BUF_SIZE (FD_IMAGE_FRAME_PAYLOAD_SIZE + sizeof(acomp_fd_input_frame_t))

/**
 * @brief 将摄像头像素格式转换为人脸检测组件输入格式。
 *
 * @param camera_format 摄像头输出的像素格式。
 * @param fd_format 输出的人脸检测组件像素格式。
 *
 * @retval true  格式转换成功。
 * @retval false 参数无效或摄像头像素格式不支持。
 */
static bool fd_image_stream_format_get(lisa_camera_pixel_format_t camera_format, acomp_fd_pixel_format *fd_format)
{
    if (fd_format == NULL) {
        return false;
    }

    if (camera_format == LISA_CAMERA_PIXFMT_YUV422) {
        *fd_format = PIX_FMT_YUV422_YUYV_PACKED;
    } else if (camera_format == LISA_CAMERA_PIXFMT_RGB565) {
        *fd_format = PIX_FMT_RGB565;
    } else if (camera_format == LISA_CAMERA_PIXFMT_GRAY) {
        *fd_format = PIX_FMT_GRAY;
    } else {
        return false;
    }

    return true;
}

// static void fd_image_stream_cancel_buffer(uint8_t *buffer, uint32_t len, uint16_t desc_idx)
// {
//     int ret;

//     if (buffer == NULL) {
//         return;
//     }

//     ret = acomp_fd_stream_tx_buffer_cancel(FD_IMAGE_FRAME_STREAM_CH_INDEX, buffer, len, desc_idx);
//     if (ret != 0) {
//         LISA_LOGE(TAG, "fd stream cancel failed:%d", ret);
//     }
// }

/**
 * @brief 初始化人脸检测图像输入 stream 通道。
 *
 * @retval 0 初始化完成。
 */
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

    acomp_fd_stream_ch_enable(FD_IMAGE_FRAME_STREAM_CH_INDEX, &desc);

    return 0;
}

/**
 * @brief 根据配置释放上层传入的摄像头帧。
 *
 * @param data 摄像头帧指针。
 * @param release_frame 是否调用帧内释放回调归还底层缓存。
 */
static void fd_image_stream_release(video_frame_t *data, bool release_frame)
{
    if (release_frame && (data != NULL) && (data->func != NULL)) {
        data->func(data->func_param);
    }
}

/**
 * @brief 将摄像头帧封装为 ACOMP 人脸检测输入帧并发送。
 *
 * @details
 * 函数会校验输入帧、转换像素格式、申请 stream 发送缓冲区、填充
 * @ref acomp_fd_input_frame_t 头部和图像数据，然后提交给人脸检测组件。
 * 根据 @p release_frame 决定发送完成或失败后是否释放原始摄像头帧。
 *
 * @param data 待发送的摄像头帧。
 * @param release_frame 是否在函数退出前释放 @p data 对应的底层帧缓存。
 *
 * @retval 0 发送成功。
 * @retval -EINVAL 参数无效、格式不支持、帧过大或 stream 缓冲区过小。
 * @retval -EAGAIN 当前没有可用 stream 发送缓冲区。
 * @return 其他值为底层 stream 提交失败返回码。
 */
static int fd_image_stream_send_internal(video_frame_t *data, bool release_frame)
{
    uint8_t *buffer;
    uint32_t buf_size;
    uint16_t desc_idx;
    acomp_fd_input_frame_t *fd_frame;
    acomp_fd_pixel_format fd_format;
    uint32_t required_len;
    static int frame_index = 0;
    int ret;

    if (data == NULL || data->func == NULL) {
        return -EINVAL;
    }

    if (data->buffer == NULL) {
        fd_image_stream_release(data, release_frame);
        return -EINVAL;
    }

    if (!fd_image_stream_format_get(data->format, &fd_format)) {
        LISA_LOGE(TAG, "unsupported camera format:%d", data->format);
        fd_image_stream_release(data, release_frame);
        return -EINVAL;
    }

    if (data->length > FD_IMAGE_FRAME_PAYLOAD_SIZE) {
        LISA_LOGE(TAG, "fd frame too large: frame=%u max=%u", data->length, FD_IMAGE_FRAME_PAYLOAD_SIZE);
        fd_image_stream_release(data, release_frame);
        return -EINVAL;
    }

    required_len = sizeof(acomp_fd_input_frame_t) + data->length;

    buffer = acomp_fd_stream_tx_buffer_alloc(FD_IMAGE_FRAME_STREAM_CH_INDEX, &buf_size, &desc_idx);
    if (buffer == NULL || buf_size == 0) {
        LISA_LOGW(TAG, "fd stream buffer unavailable, drop frame");
        fd_image_stream_release(data, release_frame);
        return -EAGAIN;
    }

    if (buf_size < required_len) {
        LISA_LOGE(TAG, "fd stream buffer too small: buf=%u frame=%u", buf_size, data->length);
        // fd_image_stream_cancel_buffer(buffer, buf_size, desc_idx);
        fd_image_stream_release(data, release_frame);
        return -EINVAL;
    }

    fd_frame = (acomp_fd_input_frame_t *)buffer;
    fd_frame->format = fd_format;
    fd_frame->index = frame_index++;
    fd_frame->width = data->width;
    fd_frame->height = data->height;
    fd_frame->length = data->length;
    memcpy(fd_frame->data, data->buffer, data->length);

    ret = acomp_fd_stream_tx_buffer_submit(FD_IMAGE_FRAME_STREAM_CH_INDEX, buffer, buf_size, desc_idx);
    fd_image_stream_release(data, release_frame);
    if (ret != 0) {
        LISA_LOGE(TAG, "fd stream submit failed:%d", ret);
        // fd_image_stream_cancel_buffer(buffer, buf_size, desc_idx);
        return ret;
    }

    return 0;
}

/**
 * @brief 发送摄像头帧到人脸检测输入流，并在退出前释放原始帧。
 *
 * @param data 待发送的摄像头帧。
 *
 * @return 参见 @ref fd_image_stream_send_internal。
 */
int fd_image_stream_send(video_frame_t *data)
{
    return fd_image_stream_send_internal(data, true);
}

/**
 * @brief 发送摄像头帧到人脸检测输入流，但不释放原始帧。
 *
 * @details
 * 该接口适用于调用者还需要继续使用或自行释放摄像头帧的场景。
 *
 * @param data 待发送的摄像头帧。
 *
 * @return 参见 @ref fd_image_stream_send_internal。
 */
int fd_image_stream_send_no_release(video_frame_t *data)
{
    return fd_image_stream_send_internal(data, false);
}
