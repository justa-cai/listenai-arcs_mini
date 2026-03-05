#include <string.h>
#include "lisa_mutex.h"
#include "lisa_device.h"
#include "lisa_dvp.h"
#include "Driver_DVP.h"
#include "Driver_GPDMA.h"
#include "board.h"

#define LOG_TAG "lisa_dvp"
#include "lisa_log.h"

/* DVP 设备私有数据结构 */
typedef struct {
    void *hal_handler;              /* HAL 层 DVP 句柄 */
    lisa_dvp_callback_t callback;   /* 用户回调函数 */
    void *user_data;               /* 用户数据 */
    bool initialized;              /* 初始化状态 */
    uint8_t gpdma_ch;              /* GPDMA 通道号 */
    volatile bool stop_flag;       /* 停止标志 */
    lisa_mutex_t *lock;            /* 互斥锁 */
} lisa_dvp_priv_t;

/* DVP 设备私有数据实例 */
static lisa_dvp_priv_t dvp_priv;

/* GPDMA 事件回调函数 */
static void dvp_gpdma_event_callback(uint32_t event, void *workspace)
{
    if (dvp_priv.stop_flag) {
        return;
    }

    if (event & CSK_GPDMA_EVENT_TRANSFER_DONE) {
        if (dvp_priv.callback) {
            dvp_priv.callback(LISA_DVP_EVENT_DONE, dvp_priv.user_data);
        }
    } else if (event & CSK_GPDMA_EVENT_PIPO0_DONE) {
        if (dvp_priv.callback) {
            dvp_priv.callback(LISA_DVP_EVENT_PING_DONE, dvp_priv.user_data);
        }
    } else if (event & CSK_GPDMA_EVENT_PIPO1_DONE) {
        if (dvp_priv.callback) {
            dvp_priv.callback(LISA_DVP_EVENT_PONG_DONE, dvp_priv.user_data);
        }
    }
}

/* DVP 初始化函数 */
static int arcs_dvp_setup(const lisa_device_t *dev, const lisa_dvp_config_t *config, lisa_dvp_callback_t callback, void *user_data)
{
    if (!dev || !config) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_mutex_lock(dvp_priv.lock, -1);

    if (dvp_priv.initialized) {
        LISA_LOGW(LOG_TAG, "DVP already initialized");
        lisa_mutex_unlock(dvp_priv.lock);
        return LISA_DEVICE_ERR_EXISTS;
    }

    lisa_dvp_pinmux();

    dvp_priv.hal_handler = DVP0();
    if (!dvp_priv.hal_handler) {
        LISA_LOGE(LOG_TAG, "Failed to get DVP0 handler");
        lisa_mutex_unlock(dvp_priv.lock);
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    DVP_InitTypeDef hal_config;
    hal_config.FrameWidth = config->dvp_hal_config.frame_width;
    hal_config.FrameHeight = config->dvp_hal_config.frame_height;
    hal_config.PixelOffset = config->dvp_hal_config.pixel_offset;
    hal_config.LineOffset = config->dvp_hal_config.line_offset;
    hal_config.DataAlign = (config->dvp_hal_config.data_align == LISA_DVP_DATA_ALIGN_LEFT) ? DVP_DATA_ALIGN_LEFT : DVP_DATA_ALIGN_RIGHT;
    hal_config.VSPolarity = (config->dvp_hal_config.vsync_polarity == LISA_DVP_POL_RISING) ? DVP_POL_RISING : DVP_POL_FALLING;
    hal_config.HSPolarity = (config->dvp_hal_config.hsync_polarity == LISA_DVP_POL_RISING) ? DVP_POL_RISING : DVP_POL_FALLING;
    hal_config.PCKPolarity = (config->dvp_hal_config.pclk_polarity == LISA_DVP_POL_RISING) ? DVP_POL_RISING : DVP_POL_FALLING;

    switch (config->dvp_hal_config.input_format) {
        case LISA_DVP_INPUT_FORM_YUV422_Y0CBY1CR:
            hal_config.InputFormat = DVP_INPUT_FORM_YUV422_Y0CBY1CR;
            break;
        case LISA_DVP_INPUT_FORM_LUMINA_8BIT:
            hal_config.InputFormat = DVP_INPUT_FORM_LUMINA_8BIT;
            break;
        default:
            lisa_mutex_unlock(dvp_priv.lock);
            return LISA_DEVICE_ERR_INVALID;
    }

    int32_t ret = DVP_Initialize(dvp_priv.hal_handler, NULL, &hal_config);
    if (ret != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "DVP_Initialize failed: %d", ret);
        lisa_mutex_unlock(dvp_priv.lock);
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    ret = GPDMA_Initialize();
    if (ret != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "GPDMA_Initialize failed: %d", ret);
        DVP_Uninitialize(dvp_priv.hal_handler);
        lisa_mutex_unlock(dvp_priv.lock);
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    csk_gpdma_init_t gpdma_config = {
        .dma_ch = config->gpdma_ch,
        .burst_len = gpdma_burst_len_8spl,
        .src_mode = address_mode_normal,
        .dst_mode = address_mode_normal,
        .tfr_mode = tfr_mode_p2m,
        .src_inc_mode = inc_mode_fix,
        .dst_inc_mode = inc_mode_increase,
        .prio_lvl = prio_mode_vhigh,
        .sample_unit = gpdma_sample_unit_word,
        .handshake = dvp_hs_num5,
    };

    ret = GPDMA_Config(&gpdma_config, dvp_gpdma_event_callback, NULL);
    if (ret != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "GPDMA_Config failed: %d", ret);
        DVP_Uninitialize(dvp_priv.hal_handler);
        GPDMA_Uninitialize();
        lisa_mutex_unlock(dvp_priv.lock);
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    dvp_priv.callback = callback;
    dvp_priv.user_data = user_data;
    dvp_priv.gpdma_ch = config->gpdma_ch;
    dvp_priv.initialized = true;
    dvp_priv.stop_flag = true;

    LISA_LOGI(LOG_TAG, "DVP initialized successfully with GPDMA channel %d", dvp_priv.gpdma_ch);
    lisa_mutex_unlock(dvp_priv.lock);
    return LISA_DEVICE_OK;
}

/* DVP 启动函数 */
static int arcs_dvp_start(const lisa_device_t *dev, void *buf, uint32_t len)
{
    if (!dev || !buf || len == 0) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_mutex_lock(dvp_priv.lock, -1);

    if (!dvp_priv.initialized) {
        LISA_LOGE(LOG_TAG, "DVP not initialized");
        lisa_mutex_unlock(dvp_priv.lock);
        return LISA_DEVICE_ERR_NOT_READY;
    }

    if (!dvp_priv.stop_flag) {
        LISA_LOGW(LOG_TAG, "DVP already started");
        lisa_mutex_unlock(dvp_priv.lock);
        return LISA_DEVICE_OK;
    }

    dvp_priv.stop_flag = false;

    int32_t ret = GPDMA_Start_Normal(dvp_priv.gpdma_ch, (void*)DVP0_Buf(), buf, len / sizeof(uint32_t));
    if (ret != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "GPDMA_Start_Normal failed: %d", ret);
        dvp_priv.stop_flag = true;
        lisa_mutex_unlock(dvp_priv.lock);
        return LISA_DEVICE_ERR_IO;
    }

    ret = DVP_Start(dvp_priv.hal_handler);
    if (ret != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "DVP_Start failed: %d", ret);
        GPDMA_Stop(dvp_priv.gpdma_ch);
        dvp_priv.stop_flag = true;
        lisa_mutex_unlock(dvp_priv.lock);
        return LISA_DEVICE_ERR_IO;
    }

    LISA_LOGI(LOG_TAG, "DVP capture started");
    lisa_mutex_unlock(dvp_priv.lock);
    return LISA_DEVICE_OK;
}

/* DVP 启动 Ping-Pong 模式函数 */
static int arcs_dvp_start_pingpong(const lisa_device_t *dev, void *ping_buf, void *pong_buf, uint32_t len)
{
    if (!dev || !ping_buf || !pong_buf || len == 0) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_mutex_lock(dvp_priv.lock, -1);

    if (!dvp_priv.initialized) {
        LISA_LOGE(LOG_TAG, "DVP not initialized");
        lisa_mutex_unlock(dvp_priv.lock);
        return LISA_DEVICE_ERR_NOT_READY;
    }

    if (!dvp_priv.stop_flag) {
        LISA_LOGW(LOG_TAG, "DVP already started");
        lisa_mutex_unlock(dvp_priv.lock);
        return LISA_DEVICE_OK;
    }

    dvp_priv.stop_flag = false;

    int32_t ret = GPDMA_Config_Addr_Mode(dvp_priv.gpdma_ch, address_mode_normal, address_mode_pipo);
    if (ret != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "GPDMA_Config_Addr_Mode failed: %d", ret);
        dvp_priv.stop_flag = true;
        lisa_mutex_unlock(dvp_priv.lock);
        return LISA_DEVICE_ERR_IO;
    }

    ret = GPDMA_Start_PiPo(dvp_priv.gpdma_ch, (void*)DVP0_Buf(), (void*)DVP0_Buf(), ping_buf, pong_buf, len / sizeof(uint32_t));
    if (ret != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "GPDMA_Start_PiPo failed: %d", ret);
        dvp_priv.stop_flag = true;
        lisa_mutex_unlock(dvp_priv.lock);
        return LISA_DEVICE_ERR_IO;
    }

    ret = DVP_Start(dvp_priv.hal_handler);
    if (ret != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "DVP_Start failed: %d", ret);
        GPDMA_Stop(dvp_priv.gpdma_ch);
        dvp_priv.stop_flag = true;
        lisa_mutex_unlock(dvp_priv.lock);
        return LISA_DEVICE_ERR_IO;
    }

    LISA_LOGI(LOG_TAG, "DVP Ping-Pong capture started");
    lisa_mutex_unlock(dvp_priv.lock);
    return LISA_DEVICE_OK;
}

/* DVP 停止函数 */
static int arcs_dvp_stop(const lisa_device_t *dev)
{
    if (!dev) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_mutex_lock(dvp_priv.lock, -1);

    if (!dvp_priv.initialized) {
        LISA_LOGE(LOG_TAG, "DVP not initialized");
        lisa_mutex_unlock(dvp_priv.lock);
        return LISA_DEVICE_ERR_NOT_READY;
    }

    if (dvp_priv.stop_flag) {
        LISA_LOGW(LOG_TAG, "DVP already stopped");
        lisa_mutex_unlock(dvp_priv.lock);
        return LISA_DEVICE_OK;
    }
    dvp_priv.stop_flag = true;

    DVP_Stop(dvp_priv.hal_handler);
    GPDMA_Stop(dvp_priv.gpdma_ch);

    LISA_LOGI(LOG_TAG, "DVP stopped successfully");
    lisa_mutex_unlock(dvp_priv.lock);
    return LISA_DEVICE_OK;
}

/* DVP 启用时钟输出函数 */
static int arcs_dvp_enable_clockout(const lisa_device_t *dev, uint32_t clock)
{
    if (!dev) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_mutex_lock(dvp_priv.lock, -1);

    /* 检查是否已初始化 */
    if (!dvp_priv.initialized) {
        LISA_LOGE(LOG_TAG, "DVP not initialized");
        lisa_mutex_unlock(dvp_priv.lock);
        return LISA_DEVICE_ERR_NOT_READY;
    }

    /* 启用 HAL 层 DVP 时钟输出 */
    int32_t ret = DVP_EnableClockout(dvp_priv.hal_handler, clock);
    if (ret != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "DVP_EnableClockout failed: %d", ret);
        lisa_mutex_unlock(dvp_priv.lock);
        return LISA_DEVICE_ERR_IO;
    }

    LISA_LOGI(LOG_TAG, "DVP clockout enabled successfully, freq: %u Hz", clock);
    lisa_mutex_unlock(dvp_priv.lock);
    return LISA_DEVICE_OK;
}

/* DVP 重新加载缓冲区函数（普通模式） */
static int arcs_dvp_reload(const lisa_device_t *dev, void *buf, uint32_t len)
{
    if (!dev || !buf || len == 0) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (!dvp_priv.initialized) {
        LISA_LOGE(LOG_TAG, "DVP not initialized");
        return LISA_DEVICE_ERR_NOT_READY;
    }

    if (dvp_priv.stop_flag) {
        LISA_LOGW(LOG_TAG, "DVP not running");
        return LISA_DEVICE_ERR_NOT_READY;
    }

    int32_t ret = GPDMA_Start_Normal(dvp_priv.gpdma_ch, (void*)DVP0_Buf(), buf, len / sizeof(uint32_t));
    if (ret != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "GPDMA_Start_Normal reload failed: %d", ret);
        return LISA_DEVICE_ERR_IO;
    }

    return LISA_DEVICE_OK;
}

/* DVP 重新加载 Ping-Pong 缓冲区函数 */
static int arcs_dvp_reload_pingpong(const lisa_device_t *dev, void *buf)
{
    if (!dev || !buf) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (!dvp_priv.initialized) {
        LISA_LOGE(LOG_TAG, "DVP not initialized");
        return LISA_DEVICE_ERR_NOT_READY;
    }

    if (dvp_priv.stop_flag) {
        LISA_LOGW(LOG_TAG, "DVP not running");
        return LISA_DEVICE_ERR_NOT_READY;
    }

    int32_t ret = GPDMA_PiPo_Reload(dvp_priv.gpdma_ch, (void*)DVP0_Buf(), buf);
    if (ret != CSK_DRIVER_OK) {
        LISA_LOGE(LOG_TAG, "GPDMA_PiPo_Reload failed: %d", ret);
        return LISA_DEVICE_ERR_IO;
    }

    return LISA_DEVICE_OK;
}

/* DVP 设备 API 结构体 */
static const lisa_dvp_api_t arcs_dvp_api = {
    .setup            = arcs_dvp_setup,
    .stop             = arcs_dvp_stop,
    .start            = arcs_dvp_start,
    .reload           = arcs_dvp_reload,
    .start_pingpong   = arcs_dvp_start_pingpong,
    .reload_pingpong  = arcs_dvp_reload_pingpong,
    .enable_clockout  = arcs_dvp_enable_clockout
};

/* DVP 设备初始化函数 */
static int arcs_dvp_init(void)
{
    /* 初始化私有数据 */
    memset(&dvp_priv, 0, sizeof(dvp_priv));
    dvp_priv.lock = lisa_mutex_create();
    if (!dvp_priv.lock) {
        LISA_LOGE(LOG_TAG, "Failed to create mutex");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }
    
    LISA_LOGI(LOG_TAG, "DVP driver initialized");
    return LISA_DEVICE_OK;
}

/* 注册 DVP 设备 */
LISA_DEVICE_REGISTER(dvp0, &arcs_dvp_api, &dvp_priv, NULL, arcs_dvp_init, LISA_DEVICE_PRIORITY_NORMAL);