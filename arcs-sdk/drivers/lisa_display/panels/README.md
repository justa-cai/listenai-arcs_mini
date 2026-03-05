# LISA Display Panel 移植适配指南

本文档说明如何为 LISA Display 驱动框架添加新的显示面板（Panel）驱动支持。

## 概述

LISA Display 采用分层架构，Panel 驱动层负责实现特定显示控制器（如 ST7789、AXS15231B 等）的初始化序列和专用命令。每个 Panel 驱动需要实现 `lisa_display_panel_driver_t` 接口。

## 目录结构

```
drivers/lisa_display/panels/
├── CMakeLists.txt          # 构建配置
├── Kconfig                 # Panel 选择配置
├── Kconfig.st7789p3        # ST7789P3 参数配置
├── Kconfig.axs15231b       # AXS15231B 参数配置
├── panel_st7789p3.c        # ST7789P3 驱动实现
├── panel_axs15231b.c       # AXS15231B 驱动实现
└── README.md               # 本文档
```

## 移植步骤

### 1. 创建 Panel 驱动源文件

在 `panels/` 目录下创建 `panel_<芯片型号>.c` 文件：

```c
/*
 * Copyright (c) 2025, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include "lisa_display_panel.h"
#include "lisa_mem.h"
#include "lisa_device.h"
#include "mipi_dcs.h"
#include "lisa_gpio.h"

#define LOG_TAG "panel.<芯片型号>"
#include "lisa_log.h"

/* ========================================================================
 * 1. 定义私有数据结构
 * ======================================================================== */
struct panel_<芯片型号>_priv {
    lisa_display_orientation_t orientation;
    // 其他芯片特定状态...
};

/* ========================================================================
 * 2. 定义初始化序列
 * ======================================================================== */
static const uint8_t init_sequence[] = {
    // 格式: cmd, len, data[0], data[1], ...
    // 示例:
    // 0x36, 1, 0x00,        // MADCTL
    // 0x3A, 1, 0x05,        // COLMOD: RGB565
    // ...
};

/* ========================================================================
 * 3. 实现 Panel 驱动接口
 * ======================================================================== */

static int <芯片型号>_init(lisa_display_panel_t *panel)
{
    // 1. 分配私有数据
    struct panel_<芯片型号>_priv *priv = lisa_mem_alloc(sizeof(*priv));
    if (!priv) {
        return LISA_DEVICE_ERR_NO_MEM;
    }
    panel->priv_data = priv;

    // 2. 设置显示能力
    panel->caps.width        = CONFIG_PANEL_<芯片型号>_WIDTH;
    panel->caps.height       = CONFIG_PANEL_<芯片型号>_HEIGHT;
    panel->caps.pixel_format = LISA_DISPLAY_PIXEL_FORMAT_RGB_565;
    panel->caps.orientation  = LISA_DISPLAY_ORIENTATION_0;
    panel->caps.supported_pixel_formats = (1U << LISA_DISPLAY_PIXEL_FORMAT_RGB_565);
    priv->orientation = LISA_DISPLAY_ORIENTATION_0;

    // 3. 硬件复位
    if (panel->rst_gpio) {
        lisa_gpio_write_pin(panel->rst_gpio, panel->rst_pin, 1);
        lisa_thread_mdelay(10);
        lisa_gpio_write_pin(panel->rst_gpio, panel->rst_pin, 0);
        lisa_thread_mdelay(10);
        lisa_gpio_write_pin(panel->rst_gpio, panel->rst_pin, 1);
        lisa_thread_mdelay(120);  // 根据芯片手册调整
    }

    // 4. 退出睡眠模式
    panel_write_cmd_data(panel, LCD_CMD_SLEEP_OUT, 8, NULL, 0);
    lisa_thread_mdelay(120);

    // 5. 发送初始化序列
    const uint8_t *p = init_sequence;
    while (p < init_sequence + sizeof(init_sequence)) {
        uint8_t cmd = *p++;
        uint8_t len = *p++;
        panel_write_cmd_data(panel, cmd, 8, p, len);
        p += len;
    }

    // 6. 颜色反转（可选）
#ifdef CONFIG_LISA_DISPLAY_COLOR_INVERT
    panel_write_cmd_data(panel, LCD_CMD_INVERT_ON, 8, NULL, 0);
#endif

    LISA_LOGI(LOG_TAG, "<芯片型号> initialized");
    return LISA_DEVICE_OK;
}

static int <芯片型号>_get_capabilities(lisa_display_panel_t *panel,
                                       lisa_display_capabilities_t *caps)
{
    if (!caps) {
        return LISA_DEVICE_ERR_INVALID;
    }
    memcpy(caps, &panel->caps, sizeof(*caps));
    return LISA_DEVICE_OK;
}

static int <芯片型号>_write(lisa_display_panel_t *panel, uint16_t x, uint16_t y,
                            const lisa_display_buffer_desc_t *desc, const void *buf)
{
    if (!desc || !buf) {
        return LISA_DEVICE_ERR_INVALID;
    }

    // 设置显示窗口
    lisa_mem_coord_t x_coord, y_coord;
    lisa_display_panel_mem_area_t area = {
        .panel_w = panel->caps.width,
        .panel_h = panel->caps.height,
        .x_offset = CONFIG_PANEL_<芯片型号>_X_OFFSET,
        .y_offset = CONFIG_PANEL_<芯片型号>_Y_OFFSET,
        .x = x, .y = y,
        .w = desc->width, .h = desc->height,
    };
    panel_set_mem_area(panel, &area, x_coord, y_coord);

    panel_write_cmd_data(panel, LCD_CMD_CASET, 8, x_coord, sizeof(x_coord));
    panel_write_cmd_data(panel, LCD_CMD_RASET, 8, y_coord, sizeof(y_coord));

    // 写入像素数据
    return panel_draw_pixels(panel, LCD_CMD_RAMWR, 8, x, y,
                             desc->width, desc->height, buf);
}

static int <芯片型号>_blanking_on(lisa_display_panel_t *panel)
{
    return panel_write_cmd_data(panel, LCD_CMD_DISPLAY_OFF, 8, NULL, 0);
}

static int <芯片型号>_blanking_off(lisa_display_panel_t *panel)
{
    return panel_write_cmd_data(panel, LCD_CMD_DISPLAY_ON, 8, NULL, 0);
}

static int <芯片型号>_set_brightness(lisa_display_panel_t *panel, uint8_t brightness)
{
    return panel_set_backlight_brightness(&panel->backlight, brightness);
}

static int <芯片型号>_set_orientation(lisa_display_panel_t *panel,
                                      lisa_display_orientation_t orientation)
{
    struct panel_<芯片型号>_priv *priv = panel->priv_data;

    panel->caps.orientation = orientation;
    priv->orientation = orientation;
    return LISA_DEVICE_OK;
}

/* ========================================================================
 * 4. 导出驱动接口
 * ======================================================================== */
const lisa_display_panel_driver_t lisa_display_<芯片型号>_driver = {
    .init             = <芯片型号>_init,
    .get_capabilities = <芯片型号>_get_capabilities,
    .write            = <芯片型号>_write,
    .blanking_on      = <芯片型号>_blanking_on,
    .blanking_off     = <芯片型号>_blanking_off,
    .set_brightness   = <芯片型号>_set_brightness,
    .set_orientation  = <芯片型号>_set_orientation,
};

/* ========================================================================
 * 5. 注册设备
 * ======================================================================== */
static int panel_<芯片型号>_device_init(void)
{
    LISA_LOGD(LOG_TAG, "<芯片型号> device registered");
    return LISA_DEVICE_OK;
}

/*
 * 重要: 第一个参数必须为 "lcd_panel"
 * Display 设备层通过 lisa_device_get("lcd_panel") 获取 Panel 驱动接口
 * 如果名称不匹配，将导致 Display 驱动无法正常工作
 */
LISA_DEVICE_REGISTER(lcd_panel, &lisa_display_<芯片型号>_driver, NULL, NULL,
                     &panel_<芯片型号>_device_init, LISA_DEVICE_PRIORITY_HIGH);
```

> **重要**: `LISA_DEVICE_REGISTER` 的第一个参数必须为 `lcd_panel`，这是 Display 设备层获取 Panel 驱动接口的固定名称。

### 2. 添加 Kconfig 配置

创建 `Kconfig.<芯片型号>` 文件：

```kconfig
if LISA_DISPLAY_PANEL_<芯片型号>

config PANEL_<芯片型号>_WIDTH
    int "Panel width"
    default 240
    help
        Display panel width in pixels.

config PANEL_<芯片型号>_HEIGHT
    int "Panel height"
    default 320
    help
        Display panel height in pixels.

config PANEL_<芯片型号>_X_OFFSET
    int "X offset"
    default 0
    help
        X coordinate offset for the display area.

config PANEL_<芯片型号>_Y_OFFSET
    int "Y offset"
    default 0
    help
        Y coordinate offset for the display area.

endif # LISA_DISPLAY_PANEL_<芯片型号>
```

修改 `Kconfig` 文件，添加新面板选项：

```kconfig
config LISA_DISPLAY_PANEL_<芯片型号>
    bool "<芯片型号> Panel Driver"
    help
        Enable driver for <芯片型号> display panel.
```

并在文件末尾添加：

```kconfig
rsource "Kconfig.<芯片型号>"
```

### 3. 更新 CMakeLists.txt

在 `CMakeLists.txt` 中添加条件编译：

```cmake
listenai_library_sources_ifdef(CONFIG_LISA_DISPLAY_PANEL_<芯片型号> panel_<芯片型号>.c)
```

## Panel 驱动接口说明

| 接口 | 说明 | 必须实现 |
|-----|------|---------|
| `init` | 初始化面板，发送初始化序列 | ✓ |
| `get_capabilities` | 获取显示能力（分辨率、像素格式等） | ✓ |
| `write` | 写入像素数据到指定区域 | ✓ |
| `blanking_on` | 关闭显示（进入 blanking 模式） | ✓ |
| `blanking_off` | 开启显示（退出 blanking 模式） | ✓ |
| `set_brightness` | 设置背光亮度 | ✓ |
| `set_orientation` | 设置显示方向 | 可选 |

## 公共辅助函数

Panel 抽象层提供以下辅助函数，可在驱动中直接使用：

| 函数 | 说明 |
|-----|------|
| `panel_write_cmd_data()` | 发送命令和数据 |
| `panel_draw_pixels()` | 绘制像素数据 |
| `panel_set_backlight_brightness()` | 设置背光亮度 |
| `panel_reset_pin_set()` | 控制复位引脚 |
| `panel_set_mem_area()` | 计算显示区域坐标 |

## 常用 MIPI DCS 命令

`mipi_dcs.h` 中定义了标准 MIPI DCS 命令，常用命令包括：

| 宏定义 | 命令码 | 说明 |
|-------|-------|------|
| `LCD_CMD_SLEEP_OUT` | 0x11 | 退出睡眠模式 |
| `LCD_CMD_DISPLAY_ON` | 0x29 | 开启显示 |
| `LCD_CMD_DISPLAY_OFF` | 0x28 | 关闭显示 |
| `LCD_CMD_CASET` | 0x2A | 设置列地址 |
| `LCD_CMD_RASET` | 0x2B | 设置行地址 |
| `LCD_CMD_RAMWR` | 0x2C | 写入显存 |
| `LCD_CMD_MADCTL` | 0x36 | 内存访问控制（方向） |
| `LCD_CMD_COLMOD` | 0x3A | 像素格式设置 |
| `LCD_CMD_INVERT_ON` | 0x21 | 开启颜色反转 |
| `LCD_CMD_INVERT_OFF` | 0x20 | 关闭颜色反转 |

## 移植注意事项

1. **初始化序列**: 从芯片手册或参考代码获取正确的初始化序列
2. **时序要求**: 注意复位和命令之间的延时要求
3. **坐标偏移**: 某些屏幕模组可能需要 X/Y 偏移配置
4. **颜色格式**: 确认屏幕支持的像素格式（RGB565/RGB888 等）
5. **背光控制**: 部分屏幕支持通过命令控制背光，部分需要 PWM

## 参考资料

- 芯片数据手册（Datasheet）

