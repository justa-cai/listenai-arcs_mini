#ifndef __VIDEO_CAMERA_H__
#define __VIDEO_CAMERA_H__

#ifdef __cplusplus
extern "C" {
#endif

// 图片尺寸240x176
#define CAMERA_IMAGE_WIDTH  480
#define CAMERA_IMAGE_HEIGHT 320
#define CAMERA_IMAGE_SIZE   (IMAGE_WIDTH * IMAGE_HEIGHT * 2)  // RGB565格式

//UI display图片尺寸
#define DISPLAY_IMAGE_WIDTH  160
#define DISPLAY_IMAGE_HEIGHT 180

/**
 * @brief Initialize the camera video system
 * 
 * This function initializes the camera hardware with default configuration
 * and creates a task to handle video frame acquisition and processing.
 * 
 * @return 0 on success, negative value on failure
 */
int video_camera_init(void);

/**
 * @brief Start capturing video frames
 * 
 * This function starts the camera hardware to begin capturing frames.
 * Should be called after video_camera_init().
 * 
 * @return 0 on success, negative value on failure
 */
int video_camera_start(void);

/**
 * @brief Stop capturing video frames
 * 
 * This function stops the camera hardware from capturing frames.
 * 
 * @return 0 on success, negative value on failure
 */
int video_camera_stop(void);

/**
 * @brief 从摄像头捕获单张照片
 * @param out_data 输出缓冲区指针（数据会被复制到此缓冲区）
 * @param out_size 输出数据大小（字节）
 * @param timeout_ms 超时时间（毫秒）
 * @return 成功返回0，失败返回-1
 * 
 * 注意：此函数会自动start->capture->stop，调用者需要提供足够大的缓冲区
 */
int video_camera_capture_photo(uint8_t *out_data, size_t *out_size, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* __VIDEO_CAMERA_H__ */