#ifndef __lis_algo_h__
#define __lis_algo_h__
#include "FreeRTOS.h"
#include "task.h"
#include "ic_message.h"
#include "log_print.h"
#include "lisa_log.h"

#define OS_NO_WAIT      0
#define OS_WAIT_FOREVER 0xFFFFFFFF

#define OS_EFAIL       -1
#define OS_EOK          0

#define OS_MEM_IRAM    0   //芯片内部ram
#define OS_MEM_ERAM    1   //芯片外部ram 

#define ASSERT(exp, fmt, ...)   do { if (!(exp)) { LOGE("ASSERED: " fmt, ##__VA_ARGS__); assert(exp); } } while (0)

#define ESP_LOGE(tag, fmt, ...)                                  \
	do {                                                         \
		LOGE("[%s] " fmt, tag, ##__VA_ARGS__);       \
	} while (0)

#define ESP_LOGW(tag, fmt, ...)                                  \
	do {                                                         \
		LOGW("[%s] " fmt, tag, ##__VA_ARGS__);       \
	} while (0)

#define ESP_LOGI(tag, fmt, ...)                                  \
	do {                                                         \
		LOGI("[%s] " fmt, tag, ##__VA_ARGS__);       \
	} while (0)

#define ESP_LOGD(tag, fmt, ...)                                  \
	do {                                                         \
		LOGD("[%s] " fmt, tag, ##__VA_ARGS__);       \
	} while (0)

#define TAOYUN_OS 1
#define TRANSLATION_RESULT_SIZE (1024)
#define TRANSLATION_INPUT_SIZE (1024)

typedef enum
{
    lis_err_ok = 0,
    lis_err_busy,
    lis_err_err,
    lis_err_failed
    //...
} lis_err_t;

typedef enum
{
    TYPE_TRANS = 0,
    TYPE_TTS,
    TYPE_OCR,
    TYPE_REC,
    TYPE_AUDIO,
    TYPE_FS,
    TYPE_UTIL,
    TYPE_SDIO,
    TYPE_DB,
    TYPE_FENCI,
} algo_func_type_e;

#define _PACKED_ __attribute__((packed))

typedef struct
{
    uint8_t buff[TRANSLATION_INPUT_SIZE];
    uint32_t size;
    uint32_t type;
} _PACKED_ trans_item_t;

typedef struct
{
    void *buff;
    uint32_t size;
    uint32_t speed;
    uint32_t vol;
    uint8_t role;
    int param;
    int param_value;
} _PACKED_ xtts_item_t;

typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
} ocr_roi_t;

typedef struct
{
    uint32_t ioctl_type;
    uint32_t arg;
    union {
        uint32_t value;
        ocr_roi_t ocr_roi;
    };
} _PACKED_ ocr_ioctl_t;
typedef struct
{
    uint32_t func;
    union
    {
        trans_item_t trans;
        xtts_item_t xtts;
        ocr_ioctl_t ocr;
    };
} _PACKED_ __attribute__((aligned(32))) func_descriptors_t;

typedef enum
{
    FUNC_TRANS_START_E = 1,
    FUNC_TRANS_STATUS_E,
    FUNC_TRANS_RESULT_E,
    FUNC_TRANS_STOP_E,
} translation_func_e;

typedef enum
{
    OCR_SCAN_MODE = 0,
    OCR_HANDMODE,
    OCR_IMG_320X240,
    OCR_IMG_128X180,
    OCR_IMG_STICH, // 拼接图片
    OCR_RESULT,
    OCR_STATUS,
    OCR_SCAN_KEY_DURATION,
    OCR_STITCH_ROI,
    OCR_SENSOR_GAIN,
    OCR_SCAN_LED,
} ocr_arg_e;

typedef enum
{
    OCR_IOCTL_START = 1,
    OCR_IOCTL_STATUS,
    OCR_IOCTL_SET,
    OCR_IOCTL_GET,
    OCR_IOCTL_STOP,
    OCR_IOCTL_RESULT, // 业务主动获取OCR结果
    OCR_IOCTL_RESULT_NOTIFY, // 算法主动通知OCR结果
    OCR_IOCTL_SYS_START_UP, // 系统启动方式
} ocr_func_e;

typedef enum
{
    XTTS_PARAM_ROLE,
    XTTS_PARAM_VOLUME,
    XTTS_PARAM_VOICE_SPEED,
    XTTS_PARAM_VOLUME_INCREASE,
} xtts_func_param_e;

typedef enum
{
    FUNC_XTTS_START_E = 1,
    FUNC_XTTS_STATUS_E,
    FUNC_XTTS_STOP_E,
    FUNC_XTTS_SET_PARAM_E,
} xtts_func_e;

typedef enum
{
    UTIL_GET_SM_E = 1,
    UTIL_GET_FY_E,
    UTIL_GET_KEY_E,
    UTIL_LED_CTRL_E,
    UTIL_CSK_DEEP_SLEEP_E,
    UTIL_CSK_VERSION_E,
    UTIL_CAM_ROI_E,
    UTIL_SCAN_CTRL_E,
    UTIL_RESOURCE_COPY_CMD_E,
    UTIL_RESOURCE_COPY_STATUS_E,
    UTIL_DISK_CHECK_E,
    UTIL_GET_ROI_INFO_E,
    UTIL_REWRITE_240X320_REG_E, // rewrite camea register before reboot csk
    UTIL_GET_ROI_REGISTER_E,
    UTIL_IMG_REFLOW_CTRL_E,
    UTIL_IMG_REFLOW_CLEAR_E,
    UTIL_IMG_REFLOW_GET_CONFIG,
    UTIL_LOGDFILE_GET_CONFIG_E,
    UTIL_LOGDFILE_SET_ESP_LOGID_E,
    UTIL_LOGDFILE_CLEAR_E,
    UTIL_LOGDFILE_INIT_E,
    UTIL_LOGDFILE_DEINIT_E,
    UTIL_XTTS_RESOURCE_COPY_CMD_E,
    UTIL_SDIO_SRCTOR_RD_E,
    UTIL_SDIO_SRCTOR_WR_E,
    UTIL_ESP_RESET_CSK_E,
    UTIL_SET_LED_CTRL_DELAY_E,
} util_func_e;
typedef struct
{
    uint32_t func;
    uint32_t status;
    char result[TRANSLATION_RESULT_SIZE];
} __attribute__((aligned(32))) func_ack_t;

extern int lsf_cp2ap_func(uint32_t type, uint8_t *tx, uint32_t tx_size, uint8_t *rx, uint32_t rx_size, TickType_t xTicksToWait);

extern void* os_mem_alloc(unsigned int size);
extern void* os_mem_calloc(unsigned int num, unsigned int size);
extern void* os_mem_realloc(void* old_ptr, unsigned int new_size);
extern void os_mem_free(void* ptr);

// uint32_t os_ticks_get(void);

//psram strdup
char *pendup(char *s);
#endif