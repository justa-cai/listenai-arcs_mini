#ifndef CAMERA_H
#define CAMERA_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 初始化摄像头和JPEG编码器
 * @return 0:成功, -1:失败
 */
int camera_init(void);

/**
 * @brief 捕获一帧图像并转换为 JPEG
 * @param buffer 输出JPEG缓冲区指针
 * @param len 输出JPEG数据长度
 * @return 0:成功, -1:失败
 */
int camera_capture_frame(uint8_t **buffer, uint32_t *len);

/**
 * @brief 获取摄像头实际分辨率
 * @param width 输出宽度
 * @param height 输出高度
 */
void camera_get_resolution(uint32_t *width, uint32_t *height);

#endif /* CAMERA_H */
