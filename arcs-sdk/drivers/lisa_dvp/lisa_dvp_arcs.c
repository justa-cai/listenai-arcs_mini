#include <string.h>
#include "lisa_mutex.h"
#include "lisa_device.h"
#include "lisa_dvp.h"
#include "Driver_DVP.h"
#include "Driver_GPDMA.h"
#include "board.h"

#define LOG_TAG "lisa_dvp"
#include "lisa_log.h"

#if CONFIG_LISA_PM
#include "lisa_pm.h"
#endif

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

static void arcs_dvp_release_locked(void)
{
    if (!dvp_priv.initialized) {
        return;
    }

    if (!dvp_priv.stop_flag) {
        DVP_Stop(dvp_priv.hal_handler);
        GPDMA_Stop(dvp_priv.gpdma_ch);
    }

    GPDMA_Uninitialize();
    DVP_Uninitialize(dvp_priv.hal_handler);

    dvp_priv.callback = NULL;
    dvp_priv.user_data = NULL;
    dvp_priv.hal_handler = NULL;
    dvp_priv.gpdma_ch = 0;
    dvp_priv.initialized = false;
    dvp_priv.stop_flag = true;
}

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
        LISA_LOGW(LOG_TAG, "reconfigure existing DVP instance");
        arcs_dvp_release_locked();
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

/**
 * @brief 创建 DVP 设备 OS 资源（启动期 arcs_dvp_init / 唤醒后 reinit 调用）
 *
 * 仅创建 mutex；由 arcs_dvp_deinit 在 destroy 时配对删除。
 */
static int arcs_dvp_init_resources(lisa_dvp_priv_t *priv)
{
    priv->lock = lisa_mutex_create();
    if (!priv->lock) {
        LISA_LOGE(LOG_TAG, "Failed to create mutex");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }
    return LISA_DEVICE_OK;
}

/**
 * @brief 幂等的 DVP HAL 硬件初始化
 *
 * 由 arcs_dvp_init 调用，将 priv 拉到 “未配置 / stop” 形态。DVP HAL 的
 * Initialize / GPDMA 配置依赖业务侧 lisa_dvp_config_t（frame size / format 等），
 * 由 arcs_dvp_setup 完成；本函数不分配 mutex / 堆内存。
 */
static int arcs_dvp_init_hw(lisa_dvp_priv_t *priv)
{
    priv->hal_handler = NULL;
    priv->callback    = NULL;
    priv->user_data   = NULL;
    priv->gpdma_ch    = 0;
    priv->initialized = false;
    priv->stop_flag   = true;
    return LISA_DEVICE_OK;
}

/* DVP 设备初始化函数 */
static int arcs_dvp_init(void)
{
    /* 初始化私有数据 */
    memset(&dvp_priv, 0, sizeof(dvp_priv));
    int ret = arcs_dvp_init_resources(&dvp_priv);
    if (ret) {
        return ret;
    }
    ret = arcs_dvp_init_hw(&dvp_priv);
    if (ret) {
        return ret;
    }

    LISA_LOGI(LOG_TAG, "DVP driver initialized");
    return LISA_DEVICE_OK;
}

/**
 * @brief 停止并释放 DVP 设备的全部软硬件资源，恢复芯片上电初始状态
 *
 * 由 lisa_device_destroy() 调用。释放顺序与 arcs_dvp_init 申请顺序相反：
 *   1) 取 lock，复用 arcs_dvp_release_locked 完成 HAL 拆卸（必要时先 DVP_Stop +
 *      GPDMA_Stop，再 GPDMA_Uninitialize + DVP_Uninitialize）并清业务字段；
 *   2) 释放并删除 mutex；
 *   3) memset 整个 priv，回到 arcs_dvp_init 之前的零初值。
 *
 * 注：GPDMA 在 rgb / qspilcd 等驱动间共享，本批未做 refcount，依赖 HAL
 *     GPDMA_Initialize / GPDMA_Uninitialize 自身的幂等性；refcount 留作 follow-up。
 *
 * 约定：调用方需保证此时采集已停止、无并发业务在使用本设备。
 */
static int arcs_dvp_deinit(void)
{
    lisa_dvp_priv_t *priv = &dvp_priv;

    if (priv->lock) {
        lisa_mutex_lock(priv->lock, -1);
        arcs_dvp_release_locked();
        lisa_mutex_unlock(priv->lock);
        lisa_mutex_delete(priv->lock);
    } else {
        arcs_dvp_release_locked();
    }

    memset(&dvp_priv, 0, sizeof(dvp_priv));
    return LISA_DEVICE_OK;
}

#if CONFIG_LISA_PM
/* ===== System PM 回调 =====
 *
 * 与 lisa_audio 一致，本驱动采用“睡前 destroy / 唤醒后 reinit”模型：应用在睡眠前
 * 调 lisa_device_destroy(dvp0) 释放全部软硬件资源（含 DVP/GPDMA HAL 拆卸与 mutex），
 * 唤醒后在 PM after_wake 回调中调 lisa_device_reinit(dvp0) 重建到 arcs_dvp_init 后的
 * 状态，并由业务重新 lisa_dvp_setup。因此 prepare_suspend / resume_restore 不再需要
 * （原先它们只做 HAL 拆卸 / 字段清零，已被 destroy/reinit 覆盖，且二者运行于 PM
 * 临界区无法做重活）。
 *
 * 仅保留 check_idle：在 AUTO_LIGHT_SLEEP 策略下，采集运行中（DVP + GPDMA 持续搬运
 * 帧数据）阻止系统自动进入轻睡眠。只读 priv 运行标记，不取 mutex / 不访问 HAL。
 * busy = initialized && !stop_flag（stop_flag 为 inverted-logic running 标记）。
 */
static int32_t arcs_dvp_pm_check_idle(void *ctx)
{
    lisa_dvp_priv_t *priv = (lisa_dvp_priv_t *)ctx;
    if (priv == NULL) {
        return 1; /* 上下文异常时允许睡眠，不阻塞整机 */
    }
    /* DVP busy iff initialized 且未停止；stop_flag 为 inverted-logic running */
    return (priv->initialized && !priv->stop_flag) ? 0 : 1;
}

static const lisa_pm_system_ops_t arcs_dvp_pm_ops = {
    .check_idle      = arcs_dvp_pm_check_idle,
    .prepare_suspend = NULL,
    .resume_restore  = NULL,
};
#endif /* CONFIG_LISA_PM */

/* 注册 DVP 设备 */

LISA_DEVICE_REGISTER_DEINIT(dvp0, &arcs_dvp_api, &dvp_priv, NULL, arcs_dvp_init,
                            arcs_dvp_deinit, LISA_DEVICE_LEVEL_NORMAL, LISA_DEVICE_PRIORITY_NORMAL);
#if CONFIG_LISA_PM
LISA_DEVICE_PM_ATTACH(dvp0, &arcs_dvp_pm_ops, NULL, &dvp_priv);
#endif
