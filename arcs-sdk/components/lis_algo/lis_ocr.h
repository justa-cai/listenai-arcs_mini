#ifndef __LIS_OCR_H__
#define __LIS_OCR_H__

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus/*需要被.c文件使用的函数声明*/

#include "lis_algo.h"

#define OCR_PIC_ONE_SHOT (0)
#define OCR_PIC_CONTINUE_SHOT (1)

#define OCR_MODE_RIGHT_HAND (0)
#define OCR_MODE_LEFT_HAND (1)

#define OCR_SCAN_MODE_SINGLE_LINE (0)
#define OCR_SCAN_MODE_MULTI_LINE (1)

#define OCR_KEY_DOWN (0)
#define OCR_KEY_UP (1)

#define OCR_IMG_WIDTH         (96)
#define OCR_IMG_HEIGHT        (240)

typedef enum
{
    e_scan_mode_singleline = 0, // 单行模式
    e_scan_mode_multiline,
    e_scan_mode_invalid,
    e_scan_mode_factory,
    e_scan_mode_debug = 4,
    e_scan_mode_age,
    e_scan_mode_adjust, // 校准
    e_scan_mode_offline_ocr,
    e_scan_mode_adjust_serial, // 成像测试
    e_scan_mode_maximum_picture,
    e_scan_mode_reboot = 10,
    e_scan_mode_offline_image_uart, // 一致性
    e_scan_mode_max,
} scan_mode_e;

typedef enum
{
    SCAN_LED_OFF=0,
    SCAN_LED_ON,
}scan_led_e;

typedef enum
{
    CV_IMAGE_TYPE_INCRE = 0,
    CV_IMAGE_TYPE_ORIGINAL,
    CV_IMAGE_TYPE_ADJUST,
    CV_IMAGE_TYPE_INVALID,
} image_type;

typedef enum
{
    CV_IMAGE_FORMAT_ONLY_Y = 0, // 灰度图
    CV_IMAGE_FORMAT_INVALID,
} image_format;

typedef struct
{
    int8_t x_shift;
    int8_t y_shift;
    uint16_t width;
    uint16_t height;
    uint16_t uid;
    uint16_t checksum;
    image_type type;
    image_format format;
    uint16_t data_len;
    void *data;
} __attribute__((packed)) cv_image_send_para;

/* --------------------------------------------------- */
typedef enum
{
    LIS_OCR_STATE_IDLE = 0,  // 未启动
    LIS_OCR_STATE_ING,       // 进行中
    LIS_OCR_STATE_ERR,       // 内部出错
    LIS_OCR_STATE_RESULT,    // 可获取结果
    LIS_OCR_STATE_OVER,      // 正常结束
    LIS_OCR_STATE_EARLY_OVER // 提前结束
} lis_ocr_status;
typedef enum
{
    LIS_OCR_MODE_RIGHT_HAND = 0,
    LIS_OCR_MODE_LEFT_HAND
} lis_ocr_handmode;

lis_err_t lis_ocr_init(void);
void lis_ocr_deinit(void);
// 获取ocr的状态
lis_ocr_status lis_ocr_get_status(void);

// 当lis_ocr_status_get获取到OCR_STATE_RESULT后，会调用该接口获取OCR结果
char *lis_ocr_get_result(void);

// 设置OCR扫描左右手模式,默认右手模式
lis_err_t lis_ocr_set_handmode(lis_ocr_handmode mode);

lis_err_t lis_ocr_get_handmode(lis_ocr_handmode *mode);

// 设置拼接ROI区域
lis_err_t lis_stitch_set_roi(uint32_t x, uint32_t y, uint32_t width, uint32_t height);

lis_err_t lis_ocr_set_exposure(uint8_t val);

// 获取拼接前摄像头采集的每帧图片
lis_err_t lis_ocr_get_perframe_image(char *buf, int *buf_size); // 非当前业务强相关接口，可后续讨论实现！！！

// 获取经过拼接算法拼接后送入OCR的图片
lis_err_t lis_ocr_get_stitched_image(char *buf, int *buf_size); // 非当前业务强相关接口，可后续讨论实现！！！

/* ------------------OCR产测接口------------------------ */
// 产测模式图片size:320*240
lis_err_t lis_ocr_set_scan_mode(scan_mode_e mode);

// 产测设置灯亮
lis_err_t lis_ocr_set_scan_led(scan_led_e onoff);

//
lis_err_t lis_ocr_start(void);
//
lis_err_t lis_ocr_stop(void);

// 获取ocr 扫描按键时间
int lis_ocr_get_scan_key_duration(void);

// OCR结果回调函数
typedef int32_t (*ocr_result_callback_t)(void *rslt, uint32_t size);

// 注册ocr结果回调函数
int32_t lis_ocr_result_register(ocr_result_callback_t cb);

// 获取系统启动方式
lis_err_t lis_ocr_get_sys_startup(int *val);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __LIS_OCR_H__
