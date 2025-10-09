#ifndef __SPI_CAMERA_H__
#define __SPI_CAMERA_H__

#include "sensor.h"
#include "camera_xfer.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_BUF_NUM     5

struct camera_window {
    uint16_t x;  /*!< X coordinate of the top-left corner */
    uint16_t y;  /*!< Y coordinate of the top-left corner */
    uint16_t w;  /*!< Width of the window */
    uint16_t h;  /*!< Height of the window */
};

/**
 * @brief Camera硬件引脚配置结构体
 */
typedef struct {
    pin_info_t  pwdn;               /*!< Power down引脚组 */
    pin_info_t  xclk_out;           /*!< 外部时钟输出引脚组 */
} hw_pin_config_t;

/**
 * @brief Camera硬件配置结构体
 */
typedef struct {
    hw_pin_config_t pin_config;         /*!< 引脚配置 */
    uint32_t xclk_freq_hz;              /*!< 外部时钟频率 */
    uint32_t pwdn_delay_us;             /*!< Power down延时(微秒) */
    uint32_t xclk_delay_us;             /*!< 时钟输出后延时(微秒) */
} hw_config_t;

/**
 * @brief Configuration structure for camera initialization
 */
typedef struct {
    uint32_t xclk_freq_hz;          /*!< Frequency of XCLK signal, in Hz. */
    pixformat_t pixel_format;       /*!< Format of the pixel data: PIXFORMAT_ + YUV422|GRAYSCALE|RGB565|JPEG  */
    framesize_t frame_size;         /*!< Size of the output image: FRAMESIZE_ + QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA  */
    int jpeg_quality;               /*!< Quality of JPEG output. 0-63 lower means higher quality  */
    uint8_t colorbar;
    size_t buf_count;
    struct camera_window roi;       /*!< Region of interest for OCR, if not set, use full frame */
    bool is_v_flip;
    bool is_h_mirror;
    i2c_config_t i2c_config;
    xfer_hw_config_t xfer_config;
    hw_config_t hw_config;   /*!< Camera hardware configuration */
} camera_config_t;

void camera_get_img(void);
struct cam_ipeg_mem *camera_dqbuf(struct cam_ipeg_mem *spi_mem, unsigned int timeout_msec);
void camera_qbuf(struct cam_ipeg_mem *spi_mem);
void camera_qreset(void);

int camera_init(const camera_config_t *config);
int camera_start(void);
int camera_stop(void);
int camera_deinit(void);

int camera_set_gainceiling(uint8_t gain);
int camera_set_window(int x, int y, int w, int h);
int camera_get_window(uint16_t *w, uint16_t *h);
int camera_set_handle_mode(bool hmirror, bool vflip);
int camera_set_sensor_reg(int reg, int value, int mask);
int camera_get_sensor_reg(int reg, int* value,  int mask);
#ifdef __cplusplus
}
#endif
#endif
