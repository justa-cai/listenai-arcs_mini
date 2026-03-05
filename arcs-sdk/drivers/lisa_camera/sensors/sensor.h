#pragma once

#include "lisa_device.h"
#include "lisa_log.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CAMERA_OV2640,
    CAMERA_OV3660,
    CAMERA_OV5640,
    CAMERA_OV7670,
    CAMERA_OV7725,
    CAMERA_OV9655,
    CAMERA_NT99141,
    CAMERA_GC0308,
    CAMERA_GC0310,
    CAMERA_GC0328,
    CAMERA_GC032A,
    CAMERA_GC2145,
    CAMERA_BF20A6,
    CAMERA_BF3005,
    CAMERA_BF3901,
    CAMERA_SC030IOT,
    CAMERA_SC031GS,
    CAMERA_SC101IOT,
    CAMERA_MODEL_MAX,
    CAMERA_NONE,
} camera_model_t;

typedef enum { 
    OV2640_SCCB_ADDR   = 0x30,// 0x60 >> 1
    OV3660_SCCB_ADDR   = 0x3C,// 0x78 >> 1
    OV5640_SCCB_ADDR   = 0x3C,// 0x78 >> 1
    OV7670_SCCB_ADDR   = 0x21,// 0x42 >> 1
    OV7725_SCCB_ADDR   = 0x21,// 0x42 >> 1
    OV9655_SCCB_ADDR   = 0x30,// 0x60 >> 1
    NT99141_SCCB_ADDR  = 0x2A,// 0x54 >> 1
    GC0308_SCCB_ADDR   = 0x21,// 0x42 >> 1
    GC0310_SCCB_ADDR   = 0x21,// 0x42 >> 1
    GC0328_SCCB_ADDR   = 0x21,// 0x42 >> 1
    GC032A_SCCB_ADDR   = 0x21,// 0x42 >> 1
    GC2145_SCCB_ADDR   = 0x3C,// 0x78 >> 1
    BF20A6_SCCB_ADDR   = 0x6E,
    BF3005_SCCB_ADDR   = 0x6E,
    BF3901_SCCB_ADDR   = 0x6E,
    SC030IOT_SCCB_ADDR = 0x68,// 0xd0 >> 1
    SC031GS_SCCB_ADDR  = 0x30,
    SC101IOT_SCCB_ADDR = 0x68,// 0xd0 >> 1
} camera_sccb_addr_t;

typedef enum {
    OV2640_PID = 0x26,
    OV3660_PID = 0x3660,
    OV5640_PID = 0x5640,
    OV7670_PID = 0x76,
    OV7725_PID = 0x77,
    OV9650_PID = 0x96,//
    OV9655_PID = 0x96,//
    NT99141_PID = 0x1410,
    GC0308_PID = 0x9B,
    GC0310_PID = 0xA310,
    GC0328_PID = 0x9D,
    GC032A_PID = 0x232A,
    GC2145_PID = 0x2145,
    BF20A6_PID = 0x20A6,
    BF3005_PID = 0x30,
    BF3901_PID = 0x3901,
    SC030IOT_PID = 0x9A46,
    SC031GS_PID = 0x0031,
    SC101IOT_PID = 0xdA4A,
} camera_pid_t;

typedef enum {
    PIXFORMAT_RGB565,    // 2BPP/RGB565
    PIXFORMAT_YUV422,    // 2BPP/YUV422
    PIXFORMAT_YUV420,    // 1.5BPP/YUV420
    PIXFORMAT_GRAYSCALE, // 1BPP/GRAYSCALE
    PIXFORMAT_JPEG,      // JPEG/COMPRESSED
    PIXFORMAT_RGB888,    // 3BPP/RGB888
    PIXFORMAT_RAW,       // RAW
    PIXFORMAT_RGB444,    // 3BP2P/RGB444
    PIXFORMAT_RGB555,    // 3BP2P/RGB555
    PIXFORMAT_INVALID,
} pixformat_t;

typedef enum {
    FRAMESIZE_96X96,    // 96x96
    FRAMESIZE_128X180,  // 128x180
    FRAMESIZE_QQVGA,    // 160x120
    FRAMESIZE_QCIF,     // 176x144
    FRAMESIZE_HQVGA,    // 240x176
    FRAMESIZE_240X240,  // 240x240
    FRAMESIZE_QVGA,     // 320x240
    FRAMESIZE_CIF,      // 400x296
    FRAMESIZE_HVGA,     // 480x320
    FRAMESIZE_VGA,      // 640x480
    FRAMESIZE_SVGA,     // 800x600
    FRAMESIZE_XGA,      // 1024x768
    FRAMESIZE_HD,       // 1280x720
    FRAMESIZE_SXGA,     // 1280x1024
    FRAMESIZE_UXGA,     // 1600x1200
    // 3MP Sensors
    FRAMESIZE_FHD,      // 1920x1080
    FRAMESIZE_P_HD,     //  720x1280
    FRAMESIZE_P_3MP,    //  864x1536
    FRAMESIZE_QXGA,     // 2048x1536
    // 5MP Sensors
    FRAMESIZE_QHD,      // 2560x1440
    FRAMESIZE_WQXGA,    // 2560x1600
    FRAMESIZE_P_FHD,    // 1080x1920
    FRAMESIZE_QSXGA,    // 2560x1920
    FRAMESIZE_INVALID
} framesize_t;

typedef struct {
    const camera_model_t model;
    const char *name;
    const camera_sccb_addr_t sccb_addr;
    const camera_pid_t pid;
    const uint16_t max_width;
    const uint16_t max_height;
    const uint16_t supported_formats;
} camera_sensor_info_t;

typedef enum {
    ASPECT_RATIO_4X3,
    ASPECT_RATIO_3X2,
    ASPECT_RATIO_16X10,
    ASPECT_RATIO_5X3,
    ASPECT_RATIO_16X9,
    ASPECT_RATIO_21X9,
    ASPECT_RATIO_5X4,
    ASPECT_RATIO_1X1,
    ASPECT_RATIO_9X16
} aspect_ratio_t;

typedef enum {
    GAINCEILING_2X,
    GAINCEILING_4X,
    GAINCEILING_8X,
    GAINCEILING_16X,
    GAINCEILING_32X,
    GAINCEILING_64X,
    GAINCEILING_128X,
} gainceiling_t;

typedef struct {
    uint16_t max_width;
    uint16_t max_height;
    uint16_t start_x;
    uint16_t start_y;
    uint16_t end_x;
    uint16_t end_y;
    uint16_t offset_x;
    uint16_t offset_y;
    uint16_t total_x;
    uint16_t total_y;
} ratio_settings_t;

typedef struct {
    const uint16_t width;
    const uint16_t height;
    const aspect_ratio_t aspect_ratio;
} resolution_info_t;

// Resolution table (in sensor.c)
extern const resolution_info_t resolution[];
// camera sensor table (in sensor.c)
extern const camera_sensor_info_t camera_sensor[];

typedef struct {
    uint8_t MIDH;
    uint8_t MIDL;
    uint16_t PID;
    uint8_t VER;
} sensor_id_t;

typedef struct {
    framesize_t framesize;//0 - 10
    bool scale;
    bool binning;
    uint8_t quality;//0 - 63
    int8_t brightness;//-2 - 2
    int8_t contrast;//-2 - 2
    int8_t saturation;//-2 - 2
    int8_t sharpness;//-2 - 2
    uint8_t denoise;
    uint8_t special_effect;//0 - 6
    uint8_t wb_mode;//0 - 4
    uint8_t awb;
    uint8_t awb_gain;
    uint8_t aec;
    uint8_t aec2;
    int8_t ae_level;//-2 - 2
    uint16_t aec_value;//0 - 1200
    uint8_t agc;
    uint8_t agc_gain;//0 - 30
    uint8_t gainceiling;//0 - 6
    uint8_t bpc;
    uint8_t wpc;
    uint8_t raw_gma;
    uint8_t lenc;
    uint8_t hmirror;
    uint8_t vflip;
    uint8_t dcw;
    uint8_t colorbar;
} camera_status_t;

typedef struct sensor_s sensor_t;
typedef struct sensor_s {
    sensor_id_t id;             // Sensor ID.
    uint8_t  slv_addr;          // Sensor I2C slave address.
    pixformat_t pixformat;
    camera_status_t status;
    uint32_t xclk_freq_hz;

    // Sensor function pointers
    int  (*init_status)         (sensor_t *sensor);
    int  (*reset)               (sensor_t *sensor); // Reset the configuration of the sensor, and return 0 if reset is successful
    int  (*start)               (sensor_t *sensor); // Start sensor and return 0 if successful
    int  (*stop)                (sensor_t *sensor); // Stop sensor and return 0 if successful
    int  (*set_pixformat)       (sensor_t *sensor, pixformat_t pixformat);
    pixformat_t (*get_pixformat) (sensor_t *sensor);
    // int  (*set_framesize)       (sensor_t *sensor, framesize_t framesize);
    int  (*set_contrast)        (sensor_t *sensor, int level);
    int  (*set_brightness)      (sensor_t *sensor, int level);
    int  (*set_saturation)      (sensor_t *sensor, int level);
    int  (*set_sharpness)       (sensor_t *sensor, int level);
    int  (*set_denoise)         (sensor_t *sensor, int level);
    int  (*set_gainceiling)     (sensor_t *sensor, gainceiling_t gainceiling);
    int  (*set_quality)         (sensor_t *sensor, int quality);
    int  (*set_colorbar)        (sensor_t *sensor, int enable);
    int  (*set_whitebal)        (sensor_t *sensor, int enable);
    int  (*set_gain_ctrl)       (sensor_t *sensor, int enable);
    int  (*set_exposure_ctrl)   (sensor_t *sensor, int enable);
    int  (*set_hmirror)         (sensor_t *sensor, int enable);
    int  (*set_vflip)           (sensor_t *sensor, int enable);
    int  (*set_window)          (sensor_t *sensor, int16_t x, int16_t y, uint16_t w, uint16_t h);
    int  (*get_window)          (sensor_t *sensor, uint16_t *w, uint16_t *h);

    int  (*set_aec2)            (sensor_t *sensor, int enable);
    int  (*set_awb_gain)        (sensor_t *sensor, int enable);
    int  (*set_agc_gain)        (sensor_t *sensor, int gain);
    int  (*set_aec_value)       (sensor_t *sensor, int gain);

    int  (*set_special_effect)  (sensor_t *sensor, int effect);
    int  (*set_wb_mode)         (sensor_t *sensor, int mode);
    int  (*set_ae_level)        (sensor_t *sensor, int level);

    int  (*set_dcw)             (sensor_t *sensor, int enable);
    int  (*set_bpc)             (sensor_t *sensor, int enable);
    int  (*set_wpc)             (sensor_t *sensor, int enable);

    int  (*set_raw_gma)         (sensor_t *sensor, int enable);
    int  (*set_lenc)            (sensor_t *sensor, int enable);

    int  (*get_reg)             (sensor_t *sensor, int reg, int mask);
    int  (*set_reg)             (sensor_t *sensor, int reg, int mask, int value);
    int  (*set_res_raw)         (sensor_t *sensor, int startX, int startY, int endX, int endY, int offsetX, int offsetY, int totalX, int totalY, int outputX, int outputY, bool scale, bool binning);
    int  (*set_pll)             (sensor_t *sensor, int bypass, int mul, int sys, int root, int pre, int seld5, int pclken, int pclk);
    int  (*set_xclk)            (sensor_t *sensor, int timer, int xclk);
} sensor_t;


uint8_t sensor_twi_read_reg8(uint8_t slv_addr, uint8_t reg);
uint8_t sensor_twi_read_reg16(uint8_t slv_addr, uint16_t reg);
int sensor_twi_write_reg8(uint8_t slv_addr, uint8_t reg, uint8_t value);
int sensor_twi_write_reg16(uint8_t slv_addr, uint16_t reg, uint8_t value);
int sensor_twi_write_raw8(uint8_t slvaddr, uint8_t *values, int count);

#define PIXFORMAT_MASK_RGB565        (1 << PIXFORMAT_RGB565)
#define PIXFORMAT_MASK_YUV422        (1 << PIXFORMAT_YUV422)
#define PIXFORMAT_MASK_YUV420        (1 << PIXFORMAT_YUV420)
#define PIXFORMAT_MASK_GRAYSCALE     (1 << PIXFORMAT_GRAYSCALE)
#define PIXFORMAT_MASK_JPEG          (1 << PIXFORMAT_JPEG)
#define PIXFORMAT_MASK_RGB888        (1 << PIXFORMAT_RGB888)
#define PIXFORMAT_MASK_RAW           (1 << PIXFORMAT_RAW)
#define PIXFORMAT_MASK_RGB444        (1 << PIXFORMAT_RGB444)
#define PIXFORMAT_MASK_RGB555        (1 << PIXFORMAT_RGB555)

#define CAMERA_READ_REG8(addr, reg)               sensor_twi_read_reg8(addr, reg)
#define CAMERA_READ_REG16(addr, reg)              sensor_twi_read_reg16(addr, reg)
#define CAMERA_WRITE_REG8(addr, reg, value)       sensor_twi_write_reg8(addr, reg, value)
#define CAMERA_WRITE_REG16(addr, reg, value)      sensor_twi_write_reg16(addr, reg, value)
#define CAMERA_WRITE_RAW8(addr, values, count)    sensor_twi_write_raw8(addr, values, count)
#define CAMERA_DELAY_MS(ums)                      SysTick_Delay_Ms(ums)
#define CAMERA_LOG(fmt, ...)                      LOGI("CAMERA: "fmt"\r\n", ##__VA_ARGS__)
#define CAMERA_LOGD(fmt, ...)                     LOGD("CAMERA: "fmt"\r\n", ##__VA_ARGS__)
#define CAMERA_LOGI(fmt, ...)                     LOGI("CAMERA: "fmt"\r\n", ##__VA_ARGS__)
#define CAMERA_LOGE(fmt, ...)                     LOGE("CAMERA: "fmt"\r\n", ##__VA_ARGS__)
#define CAMERA_LOGW(fmt, ...)                     LOGW("CAMERA: "fmt"\r\n", ##__VA_ARGS__)


camera_sensor_info_t *camera_sensor_get_info(camera_model_t model);

int32_t sensor_twi_init(lisa_device_t *dev);

// int sensor_twi_write_array(uint8_t slv_addr, struct regval_list *reglist, int array_size);

#ifdef __cplusplus
}
#endif
