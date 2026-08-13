/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file lisa_adc_arcs.c
 * @brief LISA ADC ARCS 平台适配层
 *
 * 此文件实现 ARCS 芯片平台的 ADC 硬件适配
 */

#include "lisa_adc.h"
#include "Driver_GPADC.h"
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include "lisa_mutex.h"
#include "board.h"

#if CONFIG_LISA_PM
#include "lisa_pm.h"
#endif

#define LOG_TAG "lisa_adc_arcs"
#include <lisa_log.h>

/* ADC 触发读取次数 */
#define LISA_ADC_TRIGGER_COUNT 3

#define DEVICE_LOCK(priv)                                                                                              \
    do {                                                                                                               \
        if (priv->mutex) {                                                                                             \
            lisa_mutex_lock(priv->mutex, LISA_OS_WAIT_FOREVER);                                                        \
        }                                                                                                              \
    } while (0)

#define DEVICE_UNLOCK(priv)                                                                                            \
    do {                                                                                                               \
        if (priv->mutex) {                                                                                             \
            lisa_mutex_unlock(priv->mutex);                                                                            \
        }                                                                                                              \
    } while (0)

/* ===== ADC 通道映射定义 ===== */
#define CHANNEL_TO_SEL(ch) (1UL << (ch))

/* 定义特殊通道号 */
#define LISA_ADC_CHANNEL_VBAT  (6)   /* VBAT通道 */
#define LISA_ADC_CHANNEL_TEMP  (7)   /* 温度传感器通道 */

/* ===== 参考电压映射表 ===== */
typedef struct {
    lisa_adc_reference_t ref_type;  /* 枚举类型 */
    uint32_t fixed_voltage_mv;      /* 固定电压值(毫伏, 0表示依赖硬件) */
    uint8_t vref_sel;               /* ARCS硬件: vref_sel (0-3) */
    uint8_t vin_buf_enable;         /* ARCS硬件: vin_buf_enable */
} reference_map_t;

static const reference_map_t reference_map[] = {
    /* 枚举类型                        固定电压  vref_sel  vin_buf  说明 */
    {LISA_ADC_REF_VDD_1V2,            1200,     0,        0},  /* ref=0: Vbg 1.2V */
    {LISA_ADC_REF_VDD_3V6,            3600,     0,        1},  /* ref=0: Vbg 1.2V, vin_buf=1 (采样值*3 = 3.6V) */
    {LISA_ADC_REF_VDD_IO_AUTO,        0,        2,        0},  /* ref=2: 硬件自动选择分压系数 1/2(≤2.4V) 或 1/3(>2.4V) */
    {LISA_ADC_REF_VDD_IO_AUTO_MUL3,   0,        2,        1},  /* ref=2: 自动分压 + vin_buf=1 (采样值*3) */
    {LISA_ADC_REF_EXTERNAL,           0,        3,        0},  /* ref=3: Vref_ext 外部参考电压 */
};

/* ===== 通道配置条目 ===== */
typedef struct {
    lisa_adc_reference_t reference;   /* 参考电压类型 */
    lisa_adc_resolution_t resolution; /* 分辨率 */
    uint8_t vref_sel;                 /* 硬件配置: 参考电压选择 */
    uint8_t vin_buf_enable;           /* 硬件配置: 输入缓冲使能 */
    bool configured;                  /* 是否已配置 */
} channel_config_entry_t;

/* ===== ADC 设备私有数据 ===== */
typedef struct {
    void *hal_handler;                    /* HAL GPADC 句柄 */
    lisa_mutex_t *mutex;                  /* 互斥锁 */
    channel_config_entry_t channel_configs[8];  /* 每个通道的配置 */
    volatile bool conversion_in_flight;   /* 转换是否进行中：HAL_GPADC_Start 入口置 1，
                                             轮询完成或错误退出清 0 */
} lisa_adc_priv_t;

/* ===== ADC 设备静态实例 ===== */
static lisa_adc_priv_t adc0_priv;

/* ===== 内部辅助函数 ===== */

/**
 * @brief 根据参考电压枚举查找硬件配置
 */
static const reference_map_t *find_reference_map(lisa_adc_reference_t ref)
{
    for (size_t i = 0; i < sizeof(reference_map) / sizeof(reference_map[0]); i++) {
        if (reference_map[i].ref_type == ref) {
            return &reference_map[i];
        }
    }
    return NULL;
}

/**
 * @brief 检查通道号有效性
 */
static inline int check_channel_valid(uint32_t channel)
{
    /* GPADC支持的通道: 0-5(普通通道), 6(VBAT), 7(TEMP) */
    if (channel <= CSK_GPADC_CHANNEL5) {
        return LISA_DEVICE_OK;  /* 普通通道 0-5 */
    }
    if (channel == LISA_ADC_CHANNEL_VBAT || channel == LISA_ADC_CHANNEL_TEMP) {
        return LISA_DEVICE_OK;  /* 特殊通道 VBAT/TEMP */
    }
    return LISA_DEVICE_ERR_RANGE;
}

/**
 * @brief 将通道号转换为HAL通道选择位
 */
static uint32_t channel_to_hal_sel(uint32_t channel)
{
    switch (channel) {
    case 0:
        return CSK_GPADC_CHANNEL_SEL_0;
    case 1:
        return CSK_GPADC_CHANNEL_SEL_1;
    case 2:
        return CSK_GPADC_CHANNEL_SEL_2;
    case 3:
        return CSK_GPADC_CHANNEL_SEL_3;
    case 4:
        return CSK_GPADC_CHANNEL_SEL_4;
    case 5:
        return CSK_GPADC_CHANNEL_SEL_5;
    case LISA_ADC_CHANNEL_VBAT:
        return CSK_GPADC_CHANNEL_SEL_VBAT;
    case LISA_ADC_CHANNEL_TEMP:
        return CSK_GPADC_CHANNEL_SEL_TEMP;
    default:
        return 0;
    }
}

/* ===== ARCS平台ADC实现函数 ===== */

/**
 * @brief 配置 ADC 通道
 */
static int arcs_adc_channel_setup(lisa_device_t *dev, uint32_t channel,
                                   const lisa_adc_channel_config_t *config)
{
    if (!lisa_device_is_initialized(dev) || !config) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (check_channel_valid(channel) != LISA_DEVICE_OK) {
        return LISA_DEVICE_ERR_RANGE;
    }

    /* 查找参考电压映射 */
    const reference_map_t *ref_map = find_reference_map(config->reference);
    if (!ref_map) {
        LISA_LOGE(LOG_TAG, "Unsupported reference type: %d", config->reference);
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }

    /* 检查分辨率支持 (ARCS只支持10-bit) */
    if (config->resolution != LISA_ADC_RESOLUTION_10BIT) {
        LISA_LOGE(LOG_TAG, "Unsupported resolution: %d-bit (only 10-bit supported)",
                  config->resolution);
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }

    lisa_adc_priv_t *priv = (lisa_adc_priv_t *)dev->priv_data;

    DEVICE_LOCK(priv);

    /* 保存通道配置 */
    channel_config_entry_t *entry = &priv->channel_configs[channel];
    entry->reference = config->reference;
    entry->resolution = config->resolution;
    entry->vref_sel = ref_map->vref_sel;
    entry->vin_buf_enable = ref_map->vin_buf_enable;
    entry->configured = true;

    DEVICE_UNLOCK(priv);

    LISA_LOGI(LOG_TAG, "Channel %lu configured: ref=%d, %u-bit, vref_sel=%u, vin_buf=%u",
              channel, config->reference, config->resolution,
              entry->vref_sel, entry->vin_buf_enable);

    return LISA_DEVICE_OK;
}

/**
 * @brief 读取单次 ADC 转换结果
 */
static int arcs_adc_read(lisa_device_t *dev, uint32_t channel, uint16_t *value)
{
    if (!lisa_device_is_initialized(dev) || !value) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (check_channel_valid(channel) != LISA_DEVICE_OK) {
        return LISA_DEVICE_ERR_RANGE;
    }

    lisa_adc_priv_t *priv = (lisa_adc_priv_t *)dev->priv_data;
    uint32_t hal_channel_sel = channel_to_hal_sel(channel);

    if (hal_channel_sel == 0) {
        LISA_LOGE(LOG_TAG, "Invalid channel %lu", channel);
        return LISA_DEVICE_ERR_RANGE;
    }

    DEVICE_LOCK(priv);

    /* 检查通道是否已配置 */
    channel_config_entry_t *cfg = &priv->channel_configs[channel];
    if (!cfg->configured) {
        LISA_LOGW(LOG_TAG, "Channel %lu not configured, using default 1.2V reference", channel);
        /* 使用默认配置: 1.2V 参考电压, 10-bit */
        const reference_map_t *ref_map = find_reference_map(LISA_ADC_REF_VDD_1V2);
        cfg->reference = LISA_ADC_REF_VDD_1V2;
        cfg->resolution = LISA_ADC_RESOLUTION_10BIT;
        cfg->vref_sel = ref_map->vref_sel;
        cfg->vin_buf_enable = ref_map->vin_buf_enable;
        cfg->configured = true;
    }


    HAL_GPADC_SetVrefSel(priv->hal_handler, cfg->vref_sel);
    HAL_GPADC_SetVinBuf_Enable(priv->hal_handler, cfg->vin_buf_enable);
    HAL_GPADC_Control(priv->hal_handler, hal_channel_sel | CSK_GPADC_DMA_ENABLE(0));
    HAL_GPADC_SetTriggerNum(GPADC(), LISA_ADC_TRIGGER_COUNT);

    /* 启动ADC转换：进入前置位 busy flag，给 PM check_idle 看到 */
    priv->conversion_in_flight = true;
    if (HAL_GPADC_Start(priv->hal_handler) != 0) {
        priv->conversion_in_flight = false;
        DEVICE_UNLOCK(priv);
        LISA_LOGE(LOG_TAG, "Failed to start ADC");
        return LISA_DEVICE_ERR_IO;
    }

    /* 等待转换完成 */
    if (HAL_GPADC_PollForConversion(priv->hal_handler, 0) != 0) {
        priv->conversion_in_flight = false;
        DEVICE_UNLOCK(priv);
        LISA_LOGE(LOG_TAG, "ADC conversion timeout");
        return LISA_DEVICE_ERR_IO;
    }

    /* 连续读取 LISA_ADC_TRIGGER_COUNT 次，使用最后一次的值 */
    uint16_t adc_value = 0;
    for (int i = 0; i < LISA_ADC_TRIGGER_COUNT; i++) {
        adc_value = HAL_GPADC_GetValue(priv->hal_handler, hal_channel_sel);
    }
    *value = adc_value;

    priv->conversion_in_flight = false;
    DEVICE_UNLOCK(priv);

    LISA_LOGD(LOG_TAG, "Channel %lu: raw value 0x%x (%u)", channel, adc_value, adc_value);
    return LISA_DEVICE_OK;
}

/* ===== ARCS ADC API 实例 ===== */
static const lisa_adc_api_t arcs_adc_api = {
    .read = arcs_adc_read,
    .channel_setup = arcs_adc_channel_setup,
};

/* ===== 设备初始化函数 ===== */

/**
 * @brief 初始化 OS 资源（仅启动期一次，不可在 resume 路径调用）
 */
static int arcs_adc_init_resources(lisa_adc_priv_t *priv)
{
    priv->mutex = lisa_mutex_create();
    if (!priv->mutex) {
        LISA_LOGE(LOG_TAG, "Failed to create mutex");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }
    return LISA_DEVICE_OK;
}

/**
 * @brief 初始化 HAL 硬件（幂等：启动期 _init 与唤醒后 resume_restore 共用）
 *
 * HAL_GPADC 仅暴露 Initialize / Uninitialize（无 PowerControl）；suspend 路径
 * 调 HAL_GPADC_Uninitialize 清 HAL 状态、并清应用层 channel_configs，
 * resume 路径再次调用本函数完成 HAL_GPADC_Initialize + pinmux。
 */
static int arcs_adc_init_hw(lisa_adc_priv_t *priv)
{
    /* 获取 HAL GPADC 句柄 */
    priv->hal_handler = GPADC();
    if (!priv->hal_handler) {
        LISA_LOGE(LOG_TAG, "Failed to get GPADC handler");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    /* 初始化 HAL GPADC */
    if (HAL_GPADC_Initialize(priv->hal_handler) != 0) {
        LISA_LOGE(LOG_TAG, "Failed to initialize GPADC");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    /* 唤醒路径下硬件被清零，应用层通道配置缓存必须同步清除，
     * 强制 wake 后业务侧重新 channel_setup() 才能拿到正确参考电压 */
    for (int i = 0; i < 8; i++) {
        priv->channel_configs[i].configured = false;
    }

    priv->conversion_in_flight = false;

    lisa_adc_pinmux();

    return LISA_DEVICE_OK;
}

static int arcs_adc0_init(void)
{
    /* 清空私有数据 */
    memset(&adc0_priv, 0, sizeof(lisa_adc_priv_t));

    int ret = arcs_adc_init_resources(&adc0_priv);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    ret = arcs_adc_init_hw(&adc0_priv);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    LISA_LOGI(LOG_TAG, "ADC0 initialized successfully");

    return LISA_DEVICE_OK;
}

#if CONFIG_LISA_PM
/* ===== System PM 三回调 =====
 *
 * 契约（详见 docs/superpowers/specs/2026-05-12-lisa-device-pm-ops-design.md §6.2）：
 * - check_idle 只读 priv->conversion_in_flight；同步 read 路径在 HAL_GPADC_Start
 *   入口置 1、轮询结束/出错清 0，期间禁止 AUTO_LIGHT_SLEEP。
 *   不取 mutex / 不读 HAL，避免在 PM 临界区阻塞或递归。
 * - prepare_suspend：HAL_GPADC 暴露 Uninitialize（无 PowerControl），
 *   调 HAL_GPADC_Uninitialize 清 HAL 状态后再清应用层 channel_configs，
 *   强制应用 wake 后重新 channel_setup()。
 * - resume_restore 复用 arcs_adc_init_hw，把硬件拉回 _init 出口形态。
 *
 * Red-line check（spec §5）：
 *   1) _init_hw 不调 lisa_mutex_create / sem_create / 堆分配          ✓
 *   2) prepare_suspend 不释放 mutex                                    ✓
 *   3) prepare_suspend 不调 lisa_mem_free（本驱动无堆 buffer）          ✓
 *   4) prepare_suspend 只清 channel_configs，priv->mutex 保留          ✓
 *   5) resume_restore 不重建 OS 资源                                    ✓
 *   6) check_idle 只读 priv，无 lock / HAL / log                        ✓
 *   7) _init_hw 幂等：suspend 已先 HAL_GPADC_Uninitialize；
 *      启动期首次调用 HAL 状态本就为零                                  ✓
 */
static int32_t arcs_adc_pm_check_idle(void *ctx)
{
    lisa_adc_priv_t *priv = (lisa_adc_priv_t *)ctx;
    if (priv == NULL) {
        return 1; /* 上下文异常时允许睡眠，不阻塞整机 */
    }
    return priv->conversion_in_flight ? 0 : 1;
}

static int32_t arcs_adc_pm_prepare_suspend(void *ctx)
{
    lisa_adc_priv_t *priv = (lisa_adc_priv_t *)ctx;
    if (priv == NULL) {
        return -1;
    }

    /* HAL 仅暴露 Uninitialize（无 PowerControl）；调之以清 HAL 状态。
     * priv->hal_handler 仍指向同一全局 HAL 资源，无需置 NULL；resume 路径
     * 在 _init_hw 内重新取回并 Initialize。 */
    if (priv->hal_handler != NULL) {
        HAL_GPADC_Uninitialize(priv->hal_handler);
    }

    /* 清应用层通道配置缓存，强制 wake 后业务侧重新 channel_setup() */
    for (int i = 0; i < 8; i++) {
        priv->channel_configs[i].configured = false;
    }

    return 0;
}

static int32_t arcs_adc_pm_resume_restore(void *ctx)
{
    lisa_adc_priv_t *priv = (lisa_adc_priv_t *)ctx;
    if (priv == NULL) {
        return -1;
    }
    return arcs_adc_init_hw(priv);
}

static const lisa_pm_system_ops_t arcs_adc_pm_ops = {
    .check_idle      = arcs_adc_pm_check_idle,
    .prepare_suspend = arcs_adc_pm_prepare_suspend,
    .resume_restore  = arcs_adc_pm_resume_restore,
};
#endif /* CONFIG_LISA_PM */

/* ===== 设备注册 ===== */


LISA_DEVICE_REGISTER(adc0, &arcs_adc_api, &adc0_priv, NULL, arcs_adc0_init, LISA_DEVICE_LEVEL_NORMAL, LISA_DEVICE_PRIORITY_NORMAL);
#if CONFIG_LISA_PM
LISA_DEVICE_PM_ATTACH(adc0, &arcs_adc_pm_ops, NULL, &adc0_priv);
#endif
