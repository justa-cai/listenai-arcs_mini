/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file lisa_gpio_arcs.c
 * @brief LISA GPIO ARCS 平台适配层
 *
 * 此文件实现 ARCS 芯片平台的 GPIO 硬件适配
 */

#include "lisa_gpio.h"
#include "Driver_GPIO.h"
#include <stddef.h>
#include <string.h>
#include "lisa_mutex.h"
#include "board.h"
#include "gpio.h"

#if CONFIG_LISA_PM
#include "lisa_pm.h"
#include "pm.h"
#endif

#define LOG_TAG "lisa_gpio_arcs"
#include <lisa_log.h>

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

/* ===== GPIO 中断信息 ===== */
#define MAX_GPIO_PINS 32

typedef struct {
    lisa_gpio_irq_callback_t callback;
    void *user_data;
} gpio_irq_info_t;

/* ===== GPIO 设备私有数据 ===== */
typedef struct {
    void *hal_handler;                       /* HAL GPIO 句柄 (GPIOA/GPIOB) */
    uint32_t max_pins;                       /* 最大引脚数 */
    gpio_irq_info_t irq_info[MAX_GPIO_PINS]; /* 中断信息 */
    lisa_mutex_t *mutex;                     /* 互斥锁 */
} lisa_gpio_priv_t;

/* ===== GPIO 设备静态实例 ===== */

#if CONFIG_LISA_GPIOA
static lisa_gpio_priv_t gpioa_priv;
#endif

#if CONFIG_LISA_GPIOB
static lisa_gpio_priv_t gpiob_priv;
#endif

/* ===== 内部辅助函数 ===== */

static inline int check_pin_valid(lisa_device_t *dev, uint32_t pin)
{
    if (!dev || !dev->priv_data) {
        return LISA_DEVICE_ERR_INVALID;
    }
    lisa_gpio_priv_t *priv = (lisa_gpio_priv_t *)dev->priv_data;
    if (pin >= priv->max_pins) {
        return LISA_DEVICE_ERR_RANGE;
    }
    return LISA_DEVICE_OK;
}

static inline uint32_t pin_to_mask(uint32_t pin)
{
    return (1UL << pin);
}

static uint32_t pin_mask_to_index(uint32_t mask)
{
    uint32_t index = 0;
    while (mask > 1) {
        mask >>= 1;
        index++;
    }
    return index;
}

/* HAL中断回调 */
static void gpio_hal_irq_callback(uint32_t event, void *workspace)
{
    lisa_gpio_priv_t *priv = (lisa_gpio_priv_t *)workspace;
    if (!priv) {
        return;
    }

    /* 遍历所有引脚，检查中断事件 */
    for (uint32_t pin = 0; pin < priv->max_pins; pin++) {
        uint32_t pin_mask = pin_to_mask(pin);
        if (event & pin_mask) {
            if (priv->irq_info[pin].callback) {
                priv->irq_info[pin].callback(pin, priv->irq_info[pin].user_data);
            }
        }
    }
}

/* ===== ARCS平台GPIO实现函数 ===== */

static int arcs_gpio_configure(lisa_device_t *dev, uint32_t pin, lisa_gpio_flags_t flags)
{
    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (check_pin_valid(dev, pin) != 0) {
        return LISA_DEVICE_ERR_RANGE;
    }

    /* 检查不合法的标志组合 */
    if ((flags & LISA_GPIO_PULL_UP) && (flags & LISA_GPIO_PULL_DOWN)) {
        LISA_LOGE(LOG_TAG, "Invalid flags: PULL_UP and PULL_DOWN cannot be set simultaneously");
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_gpio_priv_t *priv = (lisa_gpio_priv_t *)dev->priv_data;
    uint32_t pin_mask = pin_to_mask(pin);
    uint32_t control = 0;

    /* 配置方向 */
    uint32_t hal_dir;
    if (flags & LISA_GPIO_OUTPUT) {
        hal_dir = CSK_GPIO_DIR_OUTPUT;
    } else {
        hal_dir = CSK_GPIO_DIR_INPUT;
    }
    GPIO_SetDir(priv->hal_handler, pin_mask, hal_dir);

    /* 配置上下拉 */
    if ((flags & LISA_GPIO_PULL_UP) && !(flags & LISA_GPIO_PULL_DOWN)) {
        control |= CSK_GPIO_MODE_PULL_UP;
    } else if ((flags & LISA_GPIO_PULL_DOWN) && !(flags & LISA_GPIO_PULL_UP)) {
        control |= CSK_GPIO_MODE_PULL_DOWN;
    } else {
        control |= CSK_GPIO_MODE_PULL_NONE;
    }

    /* 配置去抖动 */
    if (flags & LISA_GPIO_DEBOUNCE) {
        control |= CSK_GPIO_DEBOUNCE_ENABLE;
    } else {
        control |= CSK_GPIO_DEBOUNCE_DISABLE;
    }

    GPIO_Control(priv->hal_handler, control, pin_mask);

    /* 如果是输出模式，设置初始电平 */
    if (flags & LISA_GPIO_OUTPUT) {
        if (flags & LISA_GPIO_OUTPUT_INIT_HIGH) {
            GPIO_PinWrite(priv->hal_handler, pin_mask, 1);
        } else {
            GPIO_PinWrite(priv->hal_handler, pin_mask, 0);
        }
    }

    return LISA_DEVICE_OK;
}

static int arcs_gpio_get_config(lisa_device_t *dev, uint32_t pin, lisa_gpio_flags_t *flags)
{
    if (!lisa_device_is_initialized(dev) || !flags) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (check_pin_valid(dev, pin) != 0) {
        return LISA_DEVICE_ERR_RANGE;
    }

    lisa_gpio_priv_t *priv = (lisa_gpio_priv_t *)dev->priv_data;
    _GPIO_ *status;
    uint32_t size;

    if (GPIO_Status(priv->hal_handler, &status, &size) != 0) {
        return LISA_DEVICE_ERR_IO;
    }

    /* 初始化标志位 */
    *flags = 0;

    /* 读取方向配置 */
    if (status[pin].dir == csk_gpio_dir_output) {
        *flags |= LISA_GPIO_OUTPUT;
    } else {
        *flags |= LISA_GPIO_INPUT;
    }

    /* 读取上下拉配置 */
    switch (status[pin].mode) {
    case csk_gpio_mode_pull_up:
        *flags |= LISA_GPIO_PULL_UP;
        break;
    case csk_gpio_mode_pull_down:
        *flags |= LISA_GPIO_PULL_DOWN;
        break;
    default:
        /* 无上下拉，不设置标志 */
        break;
    }

    /* 读取去抖动配置 */
    GPIO_RESOURCES *gpio_res = (GPIO_RESOURCES *)priv->hal_handler;
    uint32_t pin_mask = pin_to_mask(pin);
    if (gpio_res->reg->REG_DEBOUNCEEN.all & pin_mask) {
        *flags |= LISA_GPIO_DEBOUNCE;
    }

    return LISA_DEVICE_OK;
}

static int arcs_gpio_read_pin(lisa_device_t *dev, uint32_t pin)
{
    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    if (check_pin_valid(dev, pin) != 0) {
        return LISA_DEVICE_ERR_RANGE;
    }

    lisa_gpio_priv_t *priv = (lisa_gpio_priv_t *)dev->priv_data;
    uint32_t pin_mask = pin_to_mask(pin);

    /* 检查引脚方向 */
    _GPIO_ *status;
    uint32_t size;

    if (GPIO_Status(priv->hal_handler, &status, &size) != 0) {
        return LISA_DEVICE_ERR_IO;
    }

    /* 根据引脚方向选择读取方式 */
    int ret;
    if (status[pin].dir == csk_gpio_dir_output) {
        /* 输出模式：读取 DATAOUT 寄存器 */
        GPIO_RESOURCES *gpio_res = (GPIO_RESOURCES *)priv->hal_handler;
        ret = (gpio_res->reg->REG_DATAOUT.all & pin_mask) ? 1 : 0;
    } else {
        /* 输入模式：使用 GPIO_PinRead 读取 DATAIN 寄存器 */
        ret = GPIO_PinRead(priv->hal_handler, pin_mask);
        if (ret < 0) {
            return LISA_DEVICE_ERR_IO;
        }
    }

    return (ret != 0) ? LISA_GPIO_HIGH : LISA_GPIO_LOW;
}

static int arcs_gpio_write_pin(lisa_device_t *dev, uint32_t pin, uint32_t value)
{
    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    if (check_pin_valid(dev, pin) != 0) {
        return LISA_DEVICE_ERR_RANGE;
    }

    lisa_gpio_priv_t *priv = (lisa_gpio_priv_t *)dev->priv_data;
    uint32_t pin_mask = pin_to_mask(pin);

    /* 检查引脚方向 */
    _GPIO_ *status;
    uint32_t size;

    if (GPIO_Status(priv->hal_handler, &status, &size) != 0) {
        return LISA_DEVICE_ERR_IO;
    }

    /* 引脚必须配置为输出模式才能写入 */
    if (status[pin].dir != csk_gpio_dir_output) {
        LISA_LOGE(LOG_TAG, "Pin %d is not configured as output", pin);
        return LISA_DEVICE_ERR_INVALID;
    }

    uint32_t hal_value = (value != 0) ? 1 : 0;

    if (GPIO_PinWrite(priv->hal_handler, pin_mask, hal_value) != 0) {
        return LISA_DEVICE_ERR_IO;
    }

    return LISA_DEVICE_OK;
}

static int arcs_gpio_configure_irq(lisa_device_t *dev, uint32_t pin, lisa_gpio_irq_mode_t mode,
                                   lisa_gpio_irq_callback_t callback, void *user_data)
{
    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    if (check_pin_valid(dev, pin) != 0) {
        return LISA_DEVICE_ERR_RANGE;
    }

    lisa_gpio_priv_t *priv = (lisa_gpio_priv_t *)dev->priv_data;
    uint32_t pin_mask = pin_to_mask(pin);
    uint32_t control = 0;

    DEVICE_LOCK(priv);

    /* 保存回调信息 */
    priv->irq_info[pin].callback = callback;
    priv->irq_info[pin].user_data = user_data;

    if (callback == NULL) {
        /* 禁用中断 */
        control = CSK_GPIO_INTR_DISABLE;
        GPIO_Control(priv->hal_handler, control, pin_mask);

        DEVICE_UNLOCK(priv);
        return LISA_DEVICE_OK;
    }

    /* 配置中断触发模式 */
    switch (mode) {
    case LISA_GPIO_IRQ_EDGE_RISING:
        control = CSK_GPIO_SET_INTR_POSITIVE_EDGE;
        break;
    case LISA_GPIO_IRQ_EDGE_FALLING:
        control = CSK_GPIO_SET_INTR_NEGATIVE_EDGE;
        break;
    case LISA_GPIO_IRQ_EDGE_BOTH:
        control = CSK_GPIO_SET_INTR_DUAL_EDGE;
        break;
    case LISA_GPIO_IRQ_LEVEL_HIGH:
        control = CSK_GPIO_SET_INTR_HIGH_LEVEL;
        break;
    case LISA_GPIO_IRQ_LEVEL_LOW:
        control = CSK_GPIO_SET_INTR_LOW_LEVEL;
        break;
    default:
        DEVICE_UNLOCK(priv);
        return LISA_DEVICE_ERR_INVALID;
    }

    GPIO_Control(priv->hal_handler, control, pin_mask);

    /* 注意：不需要调用 GPIO_SetCallback，因为在 GPIO_Initialize 时
     * 已经注册了全局回调 gpio_hal_irq_callback，HAL 层会同时触发
     * 全局回调和引脚级回调，导致中断被处理两次。
     * 我们只使用全局回调机制，在 gpio_hal_irq_callback 中统一分发。
     */
    // GPIO_SetCallback(priv->hal_handler, pin_mask, gpio_hal_irq_callback, (void *)priv);

    DEVICE_UNLOCK(priv);
    return LISA_DEVICE_OK;
}

static int arcs_gpio_enable_irq(lisa_device_t *dev, uint32_t pin)
{
    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    if (check_pin_valid(dev, pin) != 0) {
        return LISA_DEVICE_ERR_RANGE;
    }

    lisa_gpio_priv_t *priv = (lisa_gpio_priv_t *)dev->priv_data;
    uint32_t pin_mask = pin_to_mask(pin);

    GPIO_Control(priv->hal_handler, CSK_GPIO_INTR_ENABLE, pin_mask);

    return LISA_DEVICE_OK;
}

static int arcs_gpio_disable_irq(lisa_device_t *dev, uint32_t pin)
{
    if (!lisa_device_is_initialized(dev)) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    if (check_pin_valid(dev, pin) != 0) {
        return LISA_DEVICE_ERR_RANGE;
    }

    lisa_gpio_priv_t *priv = (lisa_gpio_priv_t *)dev->priv_data;
    uint32_t pin_mask = pin_to_mask(pin);

    GPIO_Control(priv->hal_handler, CSK_GPIO_INTR_DISABLE, pin_mask);

    return LISA_DEVICE_OK;
}

/* ===== ARCS GPIO API 实例 ===== */
static const lisa_gpio_api_t arcs_gpio_api = {
    .configure = arcs_gpio_configure,
    .get_config = arcs_gpio_get_config,
    .read_pin = arcs_gpio_read_pin,
    .write_pin = arcs_gpio_write_pin,
    .configure_irq = arcs_gpio_configure_irq,
    .enable_irq = arcs_gpio_enable_irq,
    .disable_irq = arcs_gpio_disable_irq,
};

/* ===== 设备初始化函数 ===== */

/* HAL 层 GPIO 复位：GPIO_Initialize + pinmux 回写。
 * 由 _init_fn 调用（唤醒后经 reinit 重新走 _init_fn 路径）；只写 HAL 寄存器，
 * 不分配 mutex / sem / 堆内存。 */
static int arcs_gpio_init_hw(lisa_gpio_priv_t *priv)
{
    if (GPIO_Initialize(priv->hal_handler, gpio_hal_irq_callback, priv) != 0) {
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

#if CONFIG_LISA_GPIOA
    if (priv == &gpioa_priv) {
        lisa_gpioa_pinmux();
    }
#endif
#if CONFIG_LISA_GPIOB
    if (priv == &gpiob_priv) {
        lisa_gpiob_pinmux();
    }
#endif

    priv->max_pins = MAX_GPIO_PINS;
    return LISA_DEVICE_OK;
}

#if CONFIG_LISA_GPIOA
static int arcs_gpioa_init(void)
{
    /* 清空私有数据 */
    memset(&gpioa_priv, 0, sizeof(lisa_gpio_priv_t));

    /* 获取 HAL GPIOA 句柄 */
    gpioa_priv.hal_handler = GPIOA();
    if (!gpioa_priv.hal_handler) {
        LISA_LOGE(LOG_TAG, "Failed to get GPIOA handler");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    gpioa_priv.mutex = lisa_mutex_create();
    if (!gpioa_priv.mutex) {
        LISA_LOGE(LOG_TAG, "Failed to create mutex");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    return arcs_gpio_init_hw(&gpioa_priv);
}
#endif

#if CONFIG_LISA_GPIOB
static int arcs_gpiob_init(void)
{
    /* 清空私有数据 */
    memset(&gpiob_priv, 0, sizeof(lisa_gpio_priv_t));

    /* 获取 HAL GPIOB 句柄 */
    gpiob_priv.hal_handler = GPIOB();
    if (!gpiob_priv.hal_handler) {
        LISA_LOGE(LOG_TAG, "Failed to get GPIOB handler");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    gpiob_priv.mutex = lisa_mutex_create();
    if (!gpiob_priv.mutex) {
        LISA_LOGE(LOG_TAG, "Failed to create mutex");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    return arcs_gpio_init_hw(&gpiob_priv);
}
#endif

/* ===== 设备反初始化函数 ===== */

/**
 * @brief 停止并释放单个 GPIO 实例的全部软硬件资源，恢复芯片上电初始状态
 *
 * 由 lisa_device_destroy() 经各实例 deinit 包装调用：
 *   1) GPIO_Uninitialize：关时钟门 / disable IRQ / 清 cb_event / 注销 ISR；
 *   2) 释放 OS 资源 mutex；
 *   3) memset 整个 priv（含 hal_handler / irq_info[] / max_pins），回到 _init 之前
 *      的零初值。
 *
 * 注：GPIOB 的 PMU wakeup-source 状态（s_gpiob_wakeup_*）由独立的 wakeup_ops 管理，
 *     与 GPIO 控制器 init 无关，不在此清理——以免 destroy 抹掉用户配置的唤醒源。
 *
 * 约定：调用方需保证此时无并发业务在使用本设备。
 */
static int arcs_gpio_deinit_instance(lisa_gpio_priv_t *priv)
{
    if (priv == NULL) {
        return LISA_DEVICE_ERR_INVALID;
    }

    if (priv->hal_handler) {
        GPIO_Uninitialize(priv->hal_handler);
    }
    if (priv->mutex) {
        lisa_mutex_delete(priv->mutex);
    }

    memset(priv, 0, sizeof(*priv));
    return LISA_DEVICE_OK;
}

#if CONFIG_LISA_GPIOA
static int arcs_gpioa_deinit(void)
{
    return arcs_gpio_deinit_instance(&gpioa_priv);
}
#endif

#if CONFIG_LISA_GPIOB
static int arcs_gpiob_deinit(void)
{
    return arcs_gpio_deinit_instance(&gpiob_priv);
}
#endif

#if CONFIG_LISA_PM
/* ===== System PM 回调 =====
 *
 * 与 lisa_audio 一致，本驱动采用“睡前 destroy / 唤醒后 reinit”模型：应用在睡眠前
 * 调 lisa_device_destroy(gpioX) 释放全部软硬件资源（GPIO_Uninitialize + mutex），
 * 唤醒后调 lisa_device_reinit(gpioX) 重建到 _init 后的状态，并由业务重新 configure /
 * configure_irq 恢复业务 IO。因此 prepare_suspend / resume_restore 不再需要（原先
 * 它们只做 GPIO_Uninitialize / _init_hw，已被 destroy/reinit 覆盖）。
 *
 * check_idle：GPIO 无传输概念，永远返回 1（不阻塞 AUTO_LIGHT_SLEEP）。
 *
 * 注：GPIOB 仍通过 wakeup_ops 提供 PMU 唤醒源能力，与本 system_ops 相互独立。
 */
static int32_t lisa_gpio_pm_check_idle(void *ctx)
{
    (void)ctx;
    return 1;
}

static const lisa_pm_system_ops_t arcs_gpio_pm_ops = {
    .check_idle = lisa_gpio_pm_check_idle,
    .prepare_suspend = NULL,
    .resume_restore = NULL,
};

/* ===== GPIOB wakeup-source 实现 =====
 *
 * 仅 GPIOB_00..09 可作 PMU 唤醒源（HAL pm_enable_gpio_wakeup 限制）。
 * 仅支持电平触发 LEVEL_LOW / LEVEL_HIGH，可按 pin 混合配置触发电平。
 *
 * per-pin trigger cache 长度 = ARCS_GPIOB_WAKEUP_PINS。
 * 未配置标记用 int8_t -1；其余存 lisa_gpio_wakeup_trigger_t。
 * configure / clear 只更新 cache；set_enabled 时一次性下发 HAL：
 * - gpio_mask 表示启用哪些 GPIOB wakeup pin
 * - gpio_level bit=1 表示对应 pin high-active，bit=0 表示 low-active
 */
#define ARCS_GPIOB_WAKEUP_PINS              10
#define ARCS_GPIOB_WAKEUP_NOT_CONFIGURED    ((int8_t)-1)

static int8_t s_gpiob_wakeup_trigger_cache[ARCS_GPIOB_WAKEUP_PINS] = {
    ARCS_GPIOB_WAKEUP_NOT_CONFIGURED, ARCS_GPIOB_WAKEUP_NOT_CONFIGURED,
    ARCS_GPIOB_WAKEUP_NOT_CONFIGURED, ARCS_GPIOB_WAKEUP_NOT_CONFIGURED,
    ARCS_GPIOB_WAKEUP_NOT_CONFIGURED, ARCS_GPIOB_WAKEUP_NOT_CONFIGURED,
    ARCS_GPIOB_WAKEUP_NOT_CONFIGURED, ARCS_GPIOB_WAKEUP_NOT_CONFIGURED,
    ARCS_GPIOB_WAKEUP_NOT_CONFIGURED, ARCS_GPIOB_WAKEUP_NOT_CONFIGURED,
};

static uint32_t              s_gpiob_wakeup_active_mask = 0;
static bool                  s_gpiob_wakeup_enabled = false;

static int32_t arcs_gpiob_wakeup_configure(lisa_device_t *dev, uint32_t sub_idx,
                                            uint32_t trigger)
{
    (void)dev;
    if (sub_idx >= ARCS_GPIOB_WAKEUP_PINS) {
        return LISA_DEVICE_ERR_RANGE;
    }
    switch ((lisa_gpio_wakeup_trigger_t)trigger) {
    case LISA_GPIO_WAKEUP_LEVEL_LOW:
    case LISA_GPIO_WAKEUP_LEVEL_HIGH:
        s_gpiob_wakeup_trigger_cache[sub_idx] = (int8_t)trigger;
        return 0;
    default:
        return LISA_DEVICE_ERR_INVALID;
    }
}

static int32_t arcs_gpiob_wakeup_clear(lisa_device_t *dev, uint32_t sub_idx)
{
    (void)dev;
    if (sub_idx >= ARCS_GPIOB_WAKEUP_PINS) {
        return LISA_DEVICE_ERR_RANGE;
    }
    s_gpiob_wakeup_trigger_cache[sub_idx] = ARCS_GPIOB_WAKEUP_NOT_CONFIGURED;
    return 0;
}

static int32_t arcs_gpiob_wakeup_set_enabled(lisa_device_t *dev, bool enable)
{
    (void)dev;

    if (enable) {
        if (s_gpiob_wakeup_enabled) {
            return 0;
        }
        uint32_t mask = 0;
        uint32_t level_mask = 0;
        for (uint32_t i = 0; i < ARCS_GPIOB_WAKEUP_PINS; ++i) {
            int8_t trig = s_gpiob_wakeup_trigger_cache[i];
            if (trig == ARCS_GPIOB_WAKEUP_NOT_CONFIGURED) {
                continue;
            }
            uint32_t bit = 1U << i;
            mask |= bit;
            if (trig == (int8_t)LISA_GPIO_WAKEUP_LEVEL_HIGH) {
                level_mask |= bit;
            }
        }
        if (mask == 0) {
            s_gpiob_wakeup_enabled = true;
            return 0;  /* cache 全空，幂等空启用 */
        }
        int32_t ret = pm_enable_gpio_wakeup(mask, level_mask);
        if (ret != 0) {
            return ret;
        }
        s_gpiob_wakeup_active_mask = mask;
        s_gpiob_wakeup_enabled = true;
        return 0;
    } else {
        if (!s_gpiob_wakeup_enabled) {
            return 0;
        }
        if (s_gpiob_wakeup_active_mask != 0) {
            int32_t ret = pm_disable_gpio_wakeup(s_gpiob_wakeup_active_mask);
            if (ret != 0) {
                return ret;
            }
        }
        s_gpiob_wakeup_active_mask = 0;
        s_gpiob_wakeup_enabled = false;
        return 0;
    }
}

static const lisa_pm_wakeup_ops_t arcs_gpiob_wakeup_ops = {
    .configure   = arcs_gpiob_wakeup_configure,
    .clear       = arcs_gpiob_wakeup_clear,
    .set_enabled = arcs_gpiob_wakeup_set_enabled,
};

#endif /* CONFIG_LISA_PM */

/* ===== 设备注册 ===== */


#if CONFIG_LISA_GPIOA
LISA_DEVICE_REGISTER_DEINIT(gpioa, &arcs_gpio_api, &gpioa_priv, NULL, arcs_gpioa_init,
                            arcs_gpioa_deinit, LISA_DEVICE_LEVEL_NORMAL, LISA_DEVICE_PRIORITY_NORMAL);
#if CONFIG_LISA_PM
LISA_DEVICE_PM_ATTACH(gpioa, &arcs_gpio_pm_ops, NULL, &gpioa_priv);
#endif
#endif

#if CONFIG_LISA_GPIOB
LISA_DEVICE_REGISTER_DEINIT(gpiob, &arcs_gpio_api, &gpiob_priv, NULL, arcs_gpiob_init,
                            arcs_gpiob_deinit, LISA_DEVICE_LEVEL_NORMAL, LISA_DEVICE_PRIORITY_NORMAL);
#if CONFIG_LISA_PM
LISA_DEVICE_PM_ATTACH(gpiob, &arcs_gpio_pm_ops, &arcs_gpiob_wakeup_ops, &gpiob_priv);
#endif
#endif
