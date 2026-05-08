/*
 * Copyright (c) 2025, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdio.h>
#include "lisa_device.h"
#include "lisa_display.h"
#include "lisa_display_panel.h"
#include "lisa_display_bus.h"
#include "lisa_gpio.h"
#include <lisa_mutex.h>
#include <lisa_mem.h>
#include <lisa_semaphore.h>
#include <lisa_thread.h>

#define LOG_TAG "lisa_display"
#include <lisa_log.h>

/* 前向声明静态变量（用于锁共享判断）*/
typedef struct lisa_display_priv lisa_display_priv_t;

struct lisa_display_priv {
    lisa_mutex_t *mutex;
    lisa_display_panel_t *panel;
    lisa_device_t *te_gpio;
    uint32_t te_pin;
    lisa_semaphore_t *te_sync_sem;
#if CONFIG_LISA_DISPLAY_COMPOSITE
    void (*composite_activate)(int disp_idx);
    void (*composite_deactivate)(int disp_idx);
    lisa_display_capabilities_t panel_caps;
    lisa_display_capabilities_t composite_caps;
    uint8_t *composite_buf;
#endif
    uint8_t display_inst;
};

/* 静态变量前向声明 */
static lisa_display_priv_t arcs_display_priv0;
#ifdef CONFIG_LISA_DUAL_DISPLAY
static lisa_display_priv_t arcs_display_priv1;
#endif

#define DEVICE_LOCK(priv) if ((priv)->mutex) { lisa_mutex_lock((priv)->mutex, LISA_OS_WAIT_FOREVER); }
#define DEVICE_UNLOCK(priv) if ((priv)->mutex) { lisa_mutex_unlock((priv)->mutex); }

#ifdef CONFIG_LISA_DISPLAY_COMPOSITE
#define COMPOSITE_CNT (2)

#define FOR_EACH_DISPLAY(priv, code)                    \
    do {                                                \
        for (int idx = 0; idx < COMPOSITE_CNT; idx++) { \
            if (priv->composite_activate) {             \
                priv->composite_activate(idx);          \
            }                                           \
            code;                                       \
            if (priv->composite_deactivate) {           \
                priv->composite_deactivate(idx);        \
            }                                           \
        }                                               \
    } while (0)
#endif

static void te_gpio_irq_handler(uint32_t pin, void *arg)
{
    lisa_display_priv_t *priv = (lisa_display_priv_t *)arg;
    if (!priv || !priv->te_sync_sem) {
        return;
    }

    lisa_semaphore_give(priv->te_sync_sem);
}

static void arcs_configure_te(lisa_display_priv_t *priv)
{
    if (!priv || !priv->te_gpio || !priv->te_sync_sem) {
        return;
    }

    lisa_gpio_configure(priv->te_gpio, priv->te_pin, LISA_GPIO_INPUT);
    lisa_gpio_configure_irq(priv->te_gpio, priv->te_pin, LISA_GPIO_IRQ_EDGE_RISING, te_gpio_irq_handler, priv);
    lisa_gpio_enable_irq(priv->te_gpio, priv->te_pin);
}

static lisa_device_t *arcs_get_bus_hw_ref(const lisa_display_config_t *config)
{
    switch (config->bus_type) {
    case LISA_DISPLAY_BUS_SPI_4WIRE:
        return config->bus_config.spi_4wire.spi_dev;
    case LISA_DISPLAY_BUS_QSPI:
        return config->bus_config.qspi.qspi_dev;
    case LISA_DISPLAY_BUS_RGB:
        return config->bus_config.rgb.rgb_dev;
    default:
        return NULL;
    }
}

static const char *arcs_get_default_panel_name(void)
{
#if defined(CONFIG_LISA_DISPLAY_PANEL_ST7789P3)
    return "st7789p3";
#elif defined(CONFIG_LISA_DISPLAY_PANEL_AXS15231B)
    return "axs15231b";
#elif defined(CONFIG_LISA_DISPLAY_PANEL_ST7701S)
    return "st7701s";
#elif defined(CONFIG_LISA_DISPLAY_PANEL_GC9A01) || \
      defined(CONFIG_LISA_DISPLAY_PANEL_GC9D01N)
    return "gc9a01";
#elif defined(CONFIG_LISA_DISPLAY_PANEL_NV3030B)
    return "nv3030b";
#else
    return NULL;
#endif
}

static void arcs_reset_attached_panel(lisa_display_panel_t *panel)
{
    if (!panel) {
        return;
    }

    panel->panel_dev = NULL;
    panel->bus_dev = NULL;
    panel->cmd_bus_dev = NULL;
    panel->bus_hw_ref = NULL;
    panel->rst_gpio = NULL;
    panel->rst_pin = 0;
    memset(&panel->backlight, 0, sizeof(panel->backlight));
    memset(&panel->caps, 0, sizeof(panel->caps));
    panel->bus_mutex = NULL;
    panel->bus_mutex_owner = false;
    panel->priv_data = NULL;
}

static int arcs_attach_bus(const lisa_device_t *dev, const lisa_display_config_t *config)
{
    int ret;

    if (!dev || !config) {
        return LISA_DEVICE_ERR_INVALID;
    }

    lisa_display_priv_t *priv = (lisa_display_priv_t *)dev->priv_data;
    lisa_display_panel_t *panel = priv->panel;

    const char *panel_name = config->panel_name ? config->panel_name : arcs_get_default_panel_name();
    if (!config->panel_name) {
        if (!panel_name) {
            LISA_LOGE(LOG_TAG, "Panel name is NULL and no single-panel default is available");
            return LISA_DEVICE_ERR_INVALID;
        }
        LISA_LOGI(LOG_TAG, "Panel name not specified, fallback to single-panel default: %s", panel_name);
    }

    lisa_device_t *panel_dev = lisa_device_get(panel_name);
    if (!panel_dev) {
        LISA_LOGE(LOG_TAG, "Failed to get panel device: %s", panel_name);
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    panel->panel_dev = panel_dev;

    lisa_device_t *bus_dev = NULL;
    char bus_dev_name[32];

    if (config->bus_type == LISA_DISPLAY_BUS_QSPI) {
        /* QSPI 目前仍为单实例 */
        bus_dev = lisa_device_get("panel_bus_qspi");
    }
    else if (config->bus_type == LISA_DISPLAY_BUS_SPI_4WIRE) {
        /* SPI 4-wire 支持多实例: panel_bus_spi_4wire_0, panel_bus_spi_4wire_1, ... */
        snprintf(bus_dev_name, sizeof(bus_dev_name), "panel_bus_spi_4wire_%d", priv->display_inst);
        bus_dev = lisa_device_get(bus_dev_name);
    }
    else if (config->bus_type == LISA_DISPLAY_BUS_RGB) {
        /* RGB 目前仍为单实例 */
        bus_dev = lisa_device_get("panel_bus_rgb");
    }
    else {
        LISA_LOGE(LOG_TAG, "Unsupported bus type: %d", config->bus_type);
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }

    if (!bus_dev) {
        LISA_LOGE(LOG_TAG, "Failed to get bus device");
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    panel->bus_dev = bus_dev;
    panel->bus_hw_ref = arcs_get_bus_hw_ref(config);
    lisa_display_bus_api_t *bus_api = (lisa_display_bus_api_t *)bus_dev->api;
    if (!bus_api || !bus_api->attach) {
        arcs_reset_attached_panel(panel);
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }

    lisa_display_panel_driver_t *driver = (lisa_display_panel_driver_t *)panel->panel_dev->api;
    if (!driver) {
        arcs_reset_attached_panel(panel);
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }

#ifndef CONFIG_LISA_DISPLAY_COMPOSITE
    panel->rst_gpio  = config->rst_gpio;
    panel->rst_pin   = config->rst_pin;
#endif
    panel->backlight = config->backlight;

    /* 处理面板初始化参数（如果由上层传入）——复制到 panel 实例以保证生命周期 */
    if (config->panel_init_params && config->panel_init_params_len > 0) {
        panel->init_params = lisa_mem_alloc(config->panel_init_params_len);
        if (!panel->init_params) {
            LISA_LOGE(LOG_TAG, "Failed to allocate memory for panel init params");
            return LISA_DEVICE_ERR_NO_MEM;
        }
        memcpy(panel->init_params, config->panel_init_params, config->panel_init_params_len);
        panel->init_params_len = config->panel_init_params_len;
    }

    /* 配置命令总线(如果指定了独立命令总线) */
    if (config->cmd_bus_type != LISA_DISPLAY_CMD_BUS_NONE) {
        const lisa_display_cmd_bus_api_t *cmd_bus_api = NULL;
        lisa_device_t *cmd_bus_dev = NULL;

        /* 根据命令总线类型获取对应的 API */
        switch (config->cmd_bus_type) {
        case LISA_DISPLAY_CMD_BUS_SW_SPI:
            cmd_bus_dev = lisa_device_get("cmd_bus_sw_spi");
            if (!cmd_bus_dev) {
                LOGE("Failed to get cmd bus device");
                arcs_reset_attached_panel(panel);
                return LISA_DEVICE_ERR_INIT_FAIL;
            }
            cmd_bus_api = (lisa_display_cmd_bus_api_t *)cmd_bus_dev->api;
            break;
        default:
            LOGE("Unsupported cmd_bus_type: %d", config->cmd_bus_type);
            arcs_reset_attached_panel(panel);
            return LISA_DEVICE_ERR_NOT_SUPPORT;
        }

        if (cmd_bus_api && cmd_bus_api->configure) {
            /* 调用配置接口初始化命令总线 */
            ret = cmd_bus_api->configure(config->cmd_bus_type, &config->cmd_bus_config);
            if (ret != LISA_DEVICE_OK) {
                LOGE("Failed to configure command bus: %d", ret);
                arcs_reset_attached_panel(panel);
                return ret;
            }
            /* 保存命令总线设备到 Panel（关键：自动路由依赖此指针）*/
            panel->cmd_bus_dev = cmd_bus_dev;
            LOGI("Command bus configured successfully");
        }
    }

#ifdef CONFIG_LISA_DISPLAY_COMPOSITE
    if (config->rst_gpio) {
        lisa_gpio_configure(config->rst_gpio, config->rst_pin, LISA_GPIO_CONFIG_OUTPUT_LOW);
    }
#else
    if (panel->rst_gpio) {
        lisa_gpio_configure(panel->rst_gpio, panel->rst_pin, LISA_GPIO_CONFIG_OUTPUT_LOW);
    }
#endif

    /* 初始化总线互斥锁：检查是否有其他 Panel 已使用相同的 bus_dev */
    panel->bus_mutex = NULL;
    panel->bus_mutex_owner = false;

    /* 遍历所有已初始化的 Display 实例，查找是否共享总线 */
    if (arcs_display_priv0.panel && arcs_display_priv0.panel != panel &&
        arcs_display_priv0.panel->bus_hw_ref == panel->bus_hw_ref) {
        /* 共享 display0 的总线锁 */
        panel->bus_mutex = arcs_display_priv0.panel->bus_mutex;
        LOGI("Sharing bus mutex with display0");
    }
#ifdef CONFIG_LISA_DUAL_DISPLAY
    else if (arcs_display_priv1.panel && arcs_display_priv1.panel != panel &&
             arcs_display_priv1.panel->bus_hw_ref == panel->bus_hw_ref) {
        /* 共享 display1 的总线锁 */
        panel->bus_mutex = arcs_display_priv1.panel->bus_mutex;
        LOGI("Sharing bus mutex with display1");
    }
#endif
    else {
        /* 独立总线，创建新锁 */
        panel->bus_mutex = lisa_mutex_create();
        if (!panel->bus_mutex) {
            LOGE("Failed to create bus mutex");
            arcs_reset_attached_panel(panel);
            return LISA_DEVICE_ERR_NO_MEM;
        }
        panel->bus_mutex_owner = true;
        LOGI("Created new bus mutex for independent bus");
    }

    if (config->te_gpio) {
        priv->te_sync_sem = lisa_semaphore_create(1);
        if (!priv->te_sync_sem) {
            if (panel->bus_mutex_owner) {
                lisa_mutex_delete(panel->bus_mutex);
            }
            arcs_reset_attached_panel(panel);
            return LISA_DEVICE_ERR_NO_MEM;
        }
        priv->te_gpio = config->te_gpio;
        priv->te_pin = config->te_pin;
        arcs_configure_te(priv);
    }

#ifdef CONFIG_LISA_DISPLAY_COMPOSITE
    priv->composite_activate = config->composite_activate;
    priv->composite_deactivate = config->composite_deactivate;
#endif

    ret = bus_api->attach(panel->bus_dev, config->bus_type, &config->bus_config);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "Bus attach failed: %d", ret);
        if (priv->te_sync_sem) {
            lisa_semaphore_delete(priv->te_sync_sem);
            priv->te_sync_sem = NULL;
        }
        if (panel->init_params) {
            lisa_mem_free(panel->init_params);
            panel->init_params = NULL;
            panel->init_params_len = 0;
        }
        if (panel->bus_mutex_owner) {
            lisa_mutex_delete(panel->bus_mutex);
        }
        arcs_reset_attached_panel(panel);
        return ret;
    }

    if (driver->init) {
#ifdef CONFIG_LISA_DISPLAY_COMPOSITE
        if (config->rst_gpio) {
            lisa_gpio_write_pin(config->rst_gpio, config->rst_pin, LISA_GPIO_HIGH);
            lisa_thread_mdelay(10);
            lisa_gpio_write_pin(config->rst_gpio, config->rst_pin, LISA_GPIO_LOW);
            lisa_thread_mdelay(10);
            lisa_gpio_write_pin(config->rst_gpio, config->rst_pin, LISA_GPIO_HIGH);
            lisa_thread_mdelay(120);
        }

        FOR_EACH_DISPLAY(priv, {
            ret = driver->init(panel);
            if (ret != LISA_DEVICE_OK) {
                lisa_mem_free(panel);
                return ret;
            }
        });
#else
        ret = driver->init(panel);
        if (ret != LISA_DEVICE_OK) {
            if (priv->te_sync_sem) {
                lisa_semaphore_delete(priv->te_sync_sem);
                priv->te_sync_sem = NULL;
            }
            if (panel->init_params) {
                lisa_mem_free(panel->init_params);
                panel->init_params = NULL;
                panel->init_params_len = 0;
            }
            if (panel->bus_mutex_owner) {
                lisa_mutex_delete(panel->bus_mutex);
            }
            arcs_reset_attached_panel(panel);
            return ret;
        }
#endif
    }

#ifdef CONFIG_LISA_DISPLAY_COMPOSITE
    driver->get_capabilities(panel, &priv->panel_caps);

    priv->composite_caps.width = priv->panel_caps.width * COMPOSITE_CNT;
    priv->composite_caps.height = priv->panel_caps.height;
    priv->composite_caps.pixel_format = priv->panel_caps.pixel_format;
    priv->composite_caps.orientation = priv->panel_caps.orientation;
    priv->composite_caps.supported_pixel_formats = priv->panel_caps.supported_pixel_formats;

    priv->composite_buf = lisa_mem_alloc(priv->panel_caps.width * priv->panel_caps.height * COMPOSITE_CNT * sizeof(uint16_t));
    if (!priv->composite_buf) {
        LISA_LOGE(LOG_TAG, "Failed to allocate composite buffer");
        if (priv->te_sync_sem) {
            lisa_semaphore_delete(priv->te_sync_sem);
            priv->te_sync_sem = NULL;
        }
        lisa_mem_free(panel);
        return LISA_DEVICE_ERR_NO_MEM;
    }
#endif

    return LISA_DEVICE_OK;
}

static int arcs_display_write(lisa_device_t *dev, uint16_t x, uint16_t y, const lisa_display_buffer_desc_t *desc, const void *buf)
{
    lisa_display_priv_t *priv = (lisa_display_priv_t *)dev->priv_data;
    lisa_display_panel_driver_t *driver = priv->panel->panel_dev->api;
    if (!driver || !driver->write) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    if (priv->te_sync_sem) {
        if (lisa_semaphore_take(priv->te_sync_sem, 100) != LISA_OK) {
            LISA_LOGW(LOG_TAG, "TE signal not received in time");
        }
    }

    DEVICE_LOCK(priv);
#ifdef CONFIG_LISA_DISPLAY_COMPOSITE
    int ret;
    uint8_t *draw_buf = (uint8_t *)buf;
    uint16_t draw_x1 = x;
    uint16_t draw_x2 = x + desc->width;

    uint16_t disp_w = priv->panel_caps.width;
    uint16_t disp_idx1 = draw_x1 / disp_w;
    uint16_t disp_idx2 = (draw_x2 + disp_w - 1) / disp_w;
    if (disp_idx2 > COMPOSITE_CNT) {
        disp_idx2 = COMPOSITE_CNT;
    }

    lisa_display_buffer_desc_t real_desc = {
        .height = desc->height,
    };

    uint16_t disp_x1, disp_x2;
    uint16_t real_x1, real_x2, real_w;
    for (int disp_idx = disp_idx1; disp_idx < disp_idx2; disp_idx++) {
        disp_x1 = disp_w * disp_idx;
        disp_x2 = disp_x1 + disp_w;

        if (draw_x1 > disp_x1) {
            real_x1 = draw_x1 - disp_x1;
        } else {
            real_x1 = 0;
        }

        if (draw_x2 < disp_x2) {
            real_x2 = draw_x2 - disp_x1;
        } else {
            real_x2 = disp_w;
        }

        real_w = real_x2 - real_x1;
        if (real_w == 0) {
            continue;
        }

        uint16_t src_x = (disp_x1 + real_x1) - draw_x1;
        for (uint16_t row = 0; row < desc->height; row++) {
            const uint8_t *src_row = &draw_buf[desc->pitch * row + src_x * sizeof(uint16_t)];
            uint8_t *dst_row = &priv->composite_buf[real_w * row * sizeof(uint16_t)];
            memcpy(dst_row, src_row, real_w * sizeof(uint16_t));
        }

        real_desc.buf_size = real_w * desc->height * sizeof(uint16_t);
        real_desc.width = real_w;
        real_desc.pitch = real_w * sizeof(uint16_t);

        if (priv->composite_activate) {
            priv->composite_activate(disp_idx);
        }

        ret = driver->write(priv->panel, real_x1, y, &real_desc, priv->composite_buf);
        if (ret != 0) {
            LISA_LOGE(LOG_TAG, "Failed to write to composite display %d", disp_idx);
            break;
        }

        if (priv->composite_deactivate) {
            priv->composite_deactivate(disp_idx);
        }
    }
#else
    int ret = driver->write(priv->panel, x, y, desc, buf);
#endif
    DEVICE_UNLOCK(priv);
    return ret;
}

static int arcs_get_capabilities(lisa_device_t *dev, lisa_display_capabilities_t *caps)
{
    lisa_display_priv_t *priv = (lisa_display_priv_t *)dev->priv_data;
#ifdef CONFIG_LISA_DISPLAY_COMPOSITE
    memcpy(caps, &priv->composite_caps, sizeof(*caps));
    return LISA_DEVICE_OK;
#else
    lisa_display_panel_driver_t *driver = priv->panel->panel_dev->api;
    if (!driver || !driver->get_capabilities) {
        return LISA_DEVICE_ERR_NOT_READY;
    }
    return driver->get_capabilities(priv->panel, caps);
#endif
}

static int arcs_set_orientation(lisa_device_t *dev, lisa_display_orientation_t orientation)
{
#ifdef CONFIG_LISA_DISPLAY_COMPOSITE
    return LISA_DEVICE_ERR_NOT_SUPPORT;
#else
    lisa_display_priv_t *priv = (lisa_display_priv_t *)dev->priv_data;
    lisa_display_panel_driver_t *driver = priv->panel->panel_dev->api;
    if (!driver || !driver->set_orientation) {
        return LISA_DEVICE_ERR_NOT_READY;
    }

    if (orientation != LISA_DISPLAY_ORIENTATION_0 && orientation != LISA_DISPLAY_ORIENTATION_90
        && orientation != LISA_DISPLAY_ORIENTATION_270) {
        return LISA_DEVICE_ERR_NOT_SUPPORT;
    }

    DEVICE_LOCK(priv);
    int ret = driver->set_orientation(priv->panel, orientation);
    DEVICE_UNLOCK(priv);
    return ret;
#endif
}

static int arcs_blanking_on(lisa_device_t *dev)
{
    lisa_display_priv_t *priv = (lisa_display_priv_t *)dev->priv_data;
    lisa_display_panel_driver_t *driver = priv->panel->panel_dev->api;
    if (!driver || !driver->blanking_on) {
        return LISA_DEVICE_ERR_NOT_READY;
    }
    DEVICE_LOCK(priv);
#ifdef CONFIG_LISA_DISPLAY_COMPOSITE
    int ret;
    FOR_EACH_DISPLAY(priv, {
        ret = driver->blanking_on(priv->panel);
        if (ret != LISA_DEVICE_OK) {
            DEVICE_UNLOCK(priv);
            return ret;
        }
    });
#else
    int ret = driver->blanking_on(priv->panel);
#endif
    DEVICE_UNLOCK(priv);
    return ret;
}

static int arcs_blanking_off(lisa_device_t *dev)
{
    lisa_display_priv_t *priv = (lisa_display_priv_t *)dev->priv_data;
    lisa_display_panel_driver_t *driver = priv->panel->panel_dev->api;
    if (!driver || !driver->blanking_off) {
        return LISA_DEVICE_ERR_NOT_READY;
    }
    DEVICE_LOCK(priv);
#ifdef CONFIG_LISA_DISPLAY_COMPOSITE
    int ret;
    FOR_EACH_DISPLAY(priv, {
        ret = driver->blanking_off(priv->panel);
        if (ret != LISA_DEVICE_OK) {
            DEVICE_UNLOCK(priv);
            return ret;
        }
    });
#else
    int ret = driver->blanking_off(priv->panel);
#endif
    DEVICE_UNLOCK(priv);
    return ret;
}

static int arcs_set_brightness(lisa_device_t *dev, uint8_t brightness)
{
    lisa_display_priv_t *priv = (lisa_display_priv_t *)dev->priv_data;
    lisa_display_panel_driver_t *driver = priv->panel->panel_dev->api;
    if (!driver || !driver->set_brightness) {
        return LISA_DEVICE_ERR_NOT_READY;
    }
    DEVICE_LOCK(priv);
    int ret = driver->set_brightness(priv->panel, brightness);
    DEVICE_UNLOCK(priv);
    return ret;
}

/**
 * @brief 初始化单个 Display 实例
 * @param priv Display 私有数据指针
 * @param inst_id 实例 ID
 * @return LISA_DEVICE_OK 成功, 其他值失败
 */
static int lisa_display_init_instance(lisa_display_priv_t *priv, uint8_t inst_id)
{
    if (!priv) {
        return LISA_DEVICE_ERR_INVALID;
    }

    /* 清零私有数据结构 */
    memset(priv, 0, sizeof(lisa_display_priv_t));

    /* 创建设备级互斥锁 */
    priv->mutex = lisa_mutex_create();
    if (!priv->mutex) {
        LISA_LOGE(LOG_TAG, "Failed to create mutex for display%d", inst_id);
        return LISA_DEVICE_ERR_INIT_FAIL;
    }

    /* 分配 Panel 结构体内存 */
    priv->panel = lisa_mem_alloc(sizeof(lisa_display_panel_t));
    if (!priv->panel) {
        LISA_LOGE(LOG_TAG, "Failed to allocate memory for panel%d", inst_id);
        lisa_mutex_delete(priv->mutex);
        priv->mutex = NULL;
        return LISA_DEVICE_ERR_NO_MEM;
    }

    /* 初始化实例 ID 和 Panel 结构 */
    priv->display_inst = inst_id;
    memset(priv->panel, 0, sizeof(lisa_display_panel_t));

    LISA_LOGI(LOG_TAG, "Display instance %d initialized", inst_id);
    return LISA_DEVICE_OK;
}

/**
 * @brief 清理 Display 实例资源
 * @param priv Display 私有数据指针
 */
static void lisa_display_cleanup_instance(lisa_display_priv_t *priv)
{
    if (!priv) {
        return;
    }

    if (priv->panel) {
        lisa_mem_free(priv->panel);
        priv->panel = NULL;
    }

    if (priv->mutex) {
        lisa_mutex_delete(priv->mutex);
        priv->mutex = NULL;
    }
}

/**
 * @brief 通用单实例初始化逻辑
 * @param priv Display 私有数据指针
 * @param inst_id 实例 ID
 * @return LISA_DEVICE_OK 成功, 其他值失败
 */
static int lisa_display_init_by_idx(lisa_display_priv_t *priv, uint8_t inst_id)
{
    int ret = lisa_display_init_instance(priv, inst_id);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    ret = panel_rotate_init(priv->panel);
    if (ret != LISA_DEVICE_OK) {
        LISA_LOGE(LOG_TAG, "Failed to initialize rotate context for display%d", inst_id);
        lisa_display_cleanup_instance(priv);
        return ret;
    }

    return LISA_DEVICE_OK;
}

/**
 * @brief Display0 初始化入口
 */
static int lisa_display_init_0(void)
{
    return lisa_display_init_by_idx(&arcs_display_priv0, 0);
}

#ifdef CONFIG_LISA_DUAL_DISPLAY
/**
 * @brief Display1 初始化入口
 */
static int lisa_display_init_1(void)
{
    return lisa_display_init_by_idx(&arcs_display_priv1, 1);
}
#endif

static const lisa_display_api_t arcs_display_api = {
    .write            = arcs_display_write,
    .attach_bus       = arcs_attach_bus,
    .blanking_on      = arcs_blanking_on,
    .blanking_off     = arcs_blanking_off,
    .set_brightness   = arcs_set_brightness,
    .get_capabilities = arcs_get_capabilities,
    .set_orientation  = arcs_set_orientation,
};


LISA_DEVICE_REGISTER(display, &arcs_display_api, &arcs_display_priv0, NULL, lisa_display_init_0, LISA_DEVICE_LEVEL_NORMAL, LISA_DEVICE_PRIORITY_NORMAL);

#ifdef CONFIG_LISA_DUAL_DISPLAY
LISA_DEVICE_REGISTER(display1, &arcs_display_api, &arcs_display_priv1, NULL, lisa_display_init_1, LISA_DEVICE_LEVEL_NORMAL, LISA_DEVICE_PRIORITY_NORMAL);
#endif

