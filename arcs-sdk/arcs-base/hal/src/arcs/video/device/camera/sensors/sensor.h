/*
 * This file is part of the OpenMV project.
 * Copyright (c) 2013/2014 Ibrahim Abdelkader <i.abdalkader@gmail.com>
 * This work is licensed under the MIT license, see the file LICENSE for details.
 *
 * Sensor abstraction layer.
 *
 */
#ifndef __SENSOR_H__
#define __SENSOR_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>


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
} pixformat_t;

typedef enum {
    FRAMESIZE_96X96,    // 96x96
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
    const framesize_t max_size;
    const bool support_jpeg;
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

typedef struct _sensor sensor_t;
typedef struct _sensor {
    sensor_id_t id;             // Sensor ID.
    uint8_t  slv_addr;          // Sensor I2C slave address.
    pixformat_t pixformat;
    camera_status_t status;
    uint32_t xclk_freq_hz;

    // Sensor function pointers
    int  (*init_status)         (sensor_t *sensor);
    int  (*reset)               (sensor_t *sensor); // Reset the configuration of the sensor, and return 0 if reset is successful
    int  (*set_pixformat)       (sensor_t *sensor, pixformat_t pixformat);
    int  (*set_framesize)       (sensor_t *sensor, framesize_t framesize);
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

typedef struct {
    int      (*drv_init)        (uint8_t index);
    uint8_t  (*read_reg8)       (uint8_t slv_addr, uint8_t reg);
    uint8_t  (*read_reg16)      (uint8_t slv_addr, uint16_t reg);
    int      (*write_reg8)      (uint8_t slv_addr, uint8_t reg, uint8_t value);
    int      (*write_reg16)     (uint8_t slv_addr, uint16_t reg, uint8_t value);
    void     (*delay_ms)        (uint32_t nms);
    void     (*delay_us)        (uint32_t nus);
    int      (*log)             (const char* format, ...);
} sensor_port_callback_t;


camera_sensor_info_t *camera_sensor_get_info(camera_model_t model);
int camera_sensor_port_callback(sensor_port_callback_t *callback);

uint8_t camera_sensor_read_reg8(uint8_t slv_addr, uint8_t reg);
uint8_t camera_sensor_read_reg16(uint8_t slv_addr, uint16_t reg);
int camera_sensor_write_reg8(uint8_t slv_addr, uint8_t reg, uint8_t value);
int camera_sensor_write_reg16(uint8_t slv_addr, uint16_t reg, uint8_t value);
void camera_sensor_delay_ms(uint32_t nms);
void camera_sensor_delay_us(uint32_t nus);
int camera_sensor_log(const char* format, ...);

#define CAMERA_READ_REG8(addr, reg)               camera_sensor_read_reg8(addr, reg)
#define CAMERA_READ_REG16(addr, reg)              camera_sensor_read_reg16(addr, reg)
#define CAMERA_WRITE_REG8(addr, reg, value)       camera_sensor_write_reg8(addr, reg, value)
#define CAMERA_WRITE_REG16(addr, reg, value)      camera_sensor_write_reg16(addr, reg, value)
#define CAMERA_DELAY_MS(ums)                      camera_sensor_delay_ms(ums)
#define CAMERA_DELAY_US(uus)                      camera_sensor_delay_us(uus)
#define CAMERA_LOG(fmt, ...)                      camera_sensor_log("CAMERA: "fmt"\r\n", ##__VA_ARGS__)


#define CAMERA_LOG_LEVEL_NONE     0
#define CAMERA_LOG_LEVEL_ERROR    1
#define CAMERA_LOG_LEVEL_WARN     2
#define CAMERA_LOG_LEVEL_INFO     3
#define CAMERA_LOG_LEVEL_DEBUG    4

#ifndef CAMERA_LOG_LEVEL
#define CAMERA_LOG_LEVEL          CAMERA_LOG_LEVEL_DEBUG
#endif

#define CAMERA_LOGE(fmt, ...)     do {if (CAMERA_LOG_LEVEL >= CAMERA_LOG_LEVEL_ERROR)  { CAMERA_LOG("ERR: "fmt,##__VA_ARGS__);}} while(0)
#define CAMERA_LOGW(fmt, ...)     do {if (CAMERA_LOG_LEVEL >= CAMERA_LOG_LEVEL_WARN)   { CAMERA_LOG("WRN: "fmt,##__VA_ARGS__);}} while(0)
#define CAMERA_LOGI(fmt, ...)     do {if (CAMERA_LOG_LEVEL >= CAMERA_LOG_LEVEL_INFO)   { CAMERA_LOG("INF: "fmt,##__VA_ARGS__);}} while(0)
#define CAMERA_LOGD(fmt, ...)     do {if (CAMERA_LOG_LEVEL >= CAMERA_LOG_LEVEL_DEBUG)  { CAMERA_LOG("DBG: "fmt,##__VA_ARGS__);}} while(0)



#ifdef __cplusplus
}
#endif

#endif /* __SENSOR_H__ */
