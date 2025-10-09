#ifndef __VIDEO_CAMERA_H__
#define __VIDEO_CAMERA_H__

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif

#endif /* __VIDEO_CAMERA_H__ */