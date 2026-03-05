#ifndef __LISA_CAMERA_BUS_H__
#define __LISA_CAMERA_BUS_H__

#include "lisa_camera.h"

#ifdef __cplusplus
extern "C" {
#endif

// 前向声明总线接口结构体
typedef struct lisa_camera_bus_if lisa_camera_bus_if_t;

/**
 * @brief 获取空闲帧缓冲区回调函数类型（普通上下文）
 */
typedef lisa_camera_fb_t *(*lisa_camera_get_free_fb_t)(void *ctx);

/**
 * @brief 获取空闲帧缓冲区回调函数类型（ISR 上下文）
 */
typedef lisa_camera_fb_t *(*lisa_camera_get_free_fb_from_isr_t)(void *ctx);

/**
 * @brief Camera 总线操作函数指针结构体
 *
 * 定义了一组标准化的总线操作接口，由具体的总线驱动（DVP, SPI等）实现。
 * lisa_camera_core 驱动通过这些接口与底层硬件总线交互。
 */
struct lisa_camera_bus_if {
    /**
     * @brief 初始化总线
     *
     * @param dev lisa_camera 设备指针
     * @param bus_config 总线配置
     * @return 0 成功, <0 失败
     */
    int (*init)(lisa_device_t *dev, const lisa_camera_bus_config_t *bus_config);

    /**
     * @brief 设置帧大小
     *
     * @param dev lisa_camera 设备指针
     * @param width 帧宽度
     * @param height 帧高度
     * @return 0 成功, <0 失败
     */
    int (*set_framesize)(lisa_device_t *dev, uint16_t width, uint16_t height);

    /**
     * @brief 设置像素格式
     *
     * @param dev lisa_camera 设备指针
     * @param pixformat 像素格式
     * @return 0 成功, <0 失败
     */
    int (*set_pixformat)(lisa_device_t *dev, uint16_t pixformat);

    /**
     * @brief 启动数据捕获
     *
     * @param dev lisa_camera 设备指针
     * @param callback 帧完成回调函数
     * @param get_free_fb 获取空闲帧缓冲区回调（普通上下文）
     * @param get_free_fb_from_isr 获取空闲帧缓冲区回调（ISR 上下文）
     * @param user_data 用户数据，将传递给回调函数
     * @return 0 成功, <0 失败
     */
    int (*start_capture)(lisa_device_t *dev, lisa_camera_frame_callback_t callback,
                         lisa_camera_get_free_fb_t get_free_fb,
                         lisa_camera_get_free_fb_from_isr_t get_free_fb_from_isr,
                         void *user_data);

    /**
     * @brief 停止数据捕获
     *
     * @param dev lisa_camera 设备指针
     * @return 0 成功, <0 失败
     */
    int (*stop_capture)(lisa_device_t *dev);
};

#ifdef __cplusplus
}
#endif

#endif // __LISA_CAMERA_BUS_H__