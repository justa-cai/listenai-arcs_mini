# Display 驱动

基于 lisa_device 框架的显示设备驱动，为 ARCS 平台提供统一的 LCD 显示屏控制接口。

## 功能特性

- **总线支持**: 支持 SPI 4-Wire、SPI 3-Wire、QSPI、RGB 并行等多种总线接口，支持独立命令通道（软件 SPI）
- **面板支持**: 支持 ST7789P3、AXS15231B、ST7701S、NV3030B 等多款显示控制器
- **背光控制**: 支持 PWM 调光和单线调光两种背光控制方式
- **显示方向**: 支持 0°/90°/180°/270° 四种显示方向，可选 CPDMA 硬件加速旋转
- **像素格式**: 支持 RGB565、RGB888、BGR565、ARGB8888、单色等多种像素格式
- **TE 同步**: 支持撕裂效应（Tearing Effect）同步，防止画面撕裂
- **线程安全**: 双层锁机制保护，支持多线程和多屏并发安全访问
- **多屏异显**: 支持双屏独立控制，智能识别共享总线并自动管理锁资源

## 配置选项

在 `prj.conf` 中启用驱动：

```kconfig
# 启用 Display 驱动
CONFIG_LISA_DISPLAY_DEVICE=y

# 选择面板驱动（四选一）
CONFIG_LISA_DISPLAY_PANEL_ST7789P3=y    # ST7789P3 面板
# CONFIG_LISA_DISPLAY_PANEL_AXS15231B=y # AXS15231B 面板
# CONFIG_LISA_DISPLAY_PANEL_ST7701S=y   # ST7701S 面板
# CONFIG_LISA_DISPLAY_PANEL_NV3030B=y   # NV3030B 面板（170x320，SPI 4-Wire）

# 选择总线类型（可多选，根据使用场景组合）
CONFIG_LISA_DISPLAY_BUS_SPI_4WIRE=y     # 4线 SPI
# CONFIG_LISA_DISPLAY_BUS_SPI_3WIRE=y   # 3线 SPI
# CONFIG_LISA_DISPLAY_BUS_QSPI=y        # QSPI
# CONFIG_LISA_DISPLAY_BUS_RGB=y         # RGB 并行（需配合命令通道）
# CONFIG_LISA_DISPLAY_CMD_BUS_SW_SPI=y  # 软件 SPI 命令通道（RGB 模式必需）

# 可选功能
CONFIG_LISA_DISPLAY_TE_SYNC=y           # 启用 TE 同步
CONFIG_LISA_DISPLAY_CPDMA_ROTATE=y      # 启用 CPDMA 硬件旋转
CONFIG_LISA_DISPLAY_CPDMA_CH=2          # CPDMA 通道号
CONFIG_LISA_DISPLAY_COLOR_INVERT=y      # 启用颜色反转
```

## API 接口

### 总线配置接口

```c
int lisa_display_attach_bus(lisa_device_t *dev, const lisa_display_config_t *config);
```

**重要**: 必须先调用 `lisa_display_attach_bus()` 配置总线后，才能使用其他显示功能 API。

### 显示控制接口

```c
int lisa_display_get_capabilities(lisa_device_t *dev, lisa_display_capabilities_t *caps);
int lisa_display_blanking_on(lisa_device_t *dev);
int lisa_display_blanking_off(lisa_device_t *dev);
int lisa_display_set_brightness(lisa_device_t *dev, uint8_t brightness);
int lisa_display_set_orientation(lisa_device_t *dev, lisa_display_orientation_t orientation);
```

### 数据写入接口

```c
int lisa_display_write(lisa_device_t *dev, uint16_t x, uint16_t y,
                       const lisa_display_buffer_desc_t *desc, const void *buf);
```

## 使用示例

### 多屏异显配置

多屏异显功能允许同时使用两个不同的显示屏,每个屏幕可以使用不同的 Panel 驱动和总线配置。

**Kconfig 配置**:
```kconfig
# 启用双显示屏支持
CONFIG_LISA_DUAL_DISPLAY=y

# 启用需要的 Panel 驱动(可多选)
CONFIG_LISA_DISPLAY_PANEL_ST7789P3=y
CONFIG_LISA_DISPLAY_PANEL_AXS15231B=y

# 启用总线驱动
CONFIG_LISA_DISPLAY_BUS_SPI_4WIRE=y
CONFIG_LISA_DISPLAY_BUS_QSPI=y
```

**代码示例**:
```c
#include "lisa_display.h"
#include "lisa_gpio.h"

// 获取两个显示设备
lisa_device_t *display0 = lisa_device_get("display");
lisa_device_t *display1 = lisa_device_get("display1");

lisa_device_t *gpioa = lisa_device_get("gpioa");
lisa_device_t *gpiob = lisa_device_get("gpiob");
lisa_device_t *spi1 = lisa_device_get("spi1");
lisa_device_t *qspi = lisa_device_get("qspilcd0");
lisa_device_t *pwm = lisa_device_get("pwm0");

// 配置 Display0: ST7789P3 + SPI 4-Wire
lisa_display_config_t config0 = {
    .panel_name = "st7789p3",  // Panel 设备名称
    .bus_type = LISA_DISPLAY_BUS_SPI_4WIRE,
    .bus_instance = 0,  // 使用第一个 SPI 总线实例
    .bus_config.spi_4wire = {
        .spi_dev = spi1,
        .cs_gpio = gpiob,
        .cs_pin = 1,
        .dc_gpio = gpiob,
        .dc_pin = 0,
        .spi_freq = 50000000,
    },
    .backlight = {
        .type = LISA_DISPLAY_BACKLIGHT_TYPE_PWM,
        .config.pwm = {
            .dev = pwm,
            .channel = 0,
            .freq = 2000,
        },
    },
    .rst_gpio = gpioa,
    .rst_pin = 1,
};
lisa_display_attach_bus(display0, &config0);

// 配置 Display1: AXS15231B + QSPI
lisa_display_config_t config1 = {
    .panel_name = "axs15231b",  // Panel 设备名称
    .bus_type = LISA_DISPLAY_BUS_QSPI,
    .bus_instance = 0,
    .bus_config.qspi = {
        .qspi_dev = qspi,
        .qspi_freq = 50000000,
        .cs_gpio = gpioa,
        .cs_pin = 19,
    },
    .backlight = {
        .type = LISA_DISPLAY_BACKLIGHT_TYPE_PWM,
        .config.pwm = {
            .dev = pwm,
            .channel = 1,
            .freq = 2000,
        },
    },
    .rst_gpio = gpioa,
    .rst_pin = 2,
};
lisa_display_attach_bus(display1, &config1);

// 现在可以独立控制两个显示屏
lisa_display_blanking_off(display0);
lisa_display_set_brightness(display0, 80);

lisa_display_blanking_off(display1);
lisa_display_set_brightness(display1, 60);

// 在 display0 上显示红色
lisa_display_capabilities_t caps0;
lisa_display_get_capabilities(display0, &caps0);
uint16_t *buffer0 = lisa_mem_alloc(caps0.width * caps0.height * 2);
for (int i = 0; i < caps0.width * caps0.height; i++) {
    buffer0[i] = LISA_DISPLAY_COLOR_RED;
}
lisa_display_buffer_desc_t desc0 = {
    .width = caps0.width,
    .height = caps0.height,
    .buf_size = caps0.width * caps0.height * 2,
};
lisa_display_write(display0, 0, 0, &desc0, buffer0);

// 在 display1 上显示蓝色
lisa_display_capabilities_t caps1;
lisa_display_get_capabilities(display1, &caps1);
uint16_t *buffer1 = lisa_mem_alloc(caps1.width * caps1.height * 2);
for (int i = 0; i < caps1.width * caps1.height; i++) {
    buffer1[i] = LISA_DISPLAY_COLOR_BLUE;
}
lisa_display_buffer_desc_t desc1 = {
    .width = caps1.width,
    .height = caps1.height,
    .buf_size = caps1.width * caps1.height * 2,
};
lisa_display_write(display1, 0, 0, &desc1, buffer1);
```

### 基础显示（SPI 4-Wire 单屏）

```c
#include "lisa_display.h"
#include "lisa_gpio.h"

// 1. 获取设备
lisa_device_t *display = lisa_device_get("display");
lisa_device_t *gpioa = lisa_device_get("gpioa");
lisa_device_t *gpiob = lisa_device_get("gpiob");
lisa_device_t *spi = lisa_device_get("spi1");
lisa_device_t *pwm = lisa_device_get("pwm0");

if (!display || !gpioa || !gpiob || !spi || !pwm) {
    return -1;
}

// 2. 配置总线
lisa_display_config_t config = {
    .panel_name = "st7789p3",  // 指定 Panel 设备名称
    .bus_type = LISA_DISPLAY_BUS_SPI_4WIRE,
    .bus_config.spi_4wire = {
        .spi_dev = spi,
        .cs_gpio = gpiob,
        .cs_pin = 1,
        .dc_gpio = gpiob,
        .dc_pin = 0,
        .spi_freq = 50000000,  // 50MHz
    },
    .backlight = {
        .type = LISA_DISPLAY_BACKLIGHT_TYPE_PWM,
        .config.pwm = {
            .dev = pwm,
            .channel = 0,
            .freq = 2000,
        },
    },
    .rst_gpio = gpioa,
    .rst_pin = 1,
};
lisa_display_attach_bus(display, &config);

// 3. 获取显示能力
lisa_display_capabilities_t caps;
lisa_display_get_capabilities(display, &caps);

// 4. 分配帧缓冲区
size_t buf_size = caps.width * caps.height * sizeof(uint16_t);
uint16_t *buffer = lisa_mem_alloc(buf_size);

// 5. 开启显示
lisa_display_blanking_off(display);
lisa_display_set_brightness(display, 80);  // 80% 亮度

// 6. 写入显示数据
lisa_display_buffer_desc_t desc = {
    .width = caps.width,
    .height = caps.height,
    .buf_size = buf_size,
};

// 填充红色
for (int i = 0; i < caps.width * caps.height; i++) {
    buffer[i] = LISA_DISPLAY_COLOR_RED;
}
lisa_display_write(display, 0, 0, &desc, buffer);
```

### 设置显示方向

```c
#include "lisa_display.h"

lisa_device_t *display = lisa_device_get("display");

// 设置为 90 度旋转
int ret = lisa_display_set_orientation(display, LISA_DISPLAY_ORIENTATION_90);
if (ret != LISA_DEVICE_OK) {
    // 错误处理
}
```

### 使用 QSPI 总线

```c
#include "lisa_display.h"

lisa_device_t *display = lisa_device_get("display");
lisa_device_t *qspi = lisa_device_get("qspilcd0");
lisa_device_t *gpio = lisa_device_get("gpioa");

lisa_display_config_t config = {
    .panel_name = "axs15231b",  // AXS15231B 面板
    .bus_type = LISA_DISPLAY_BUS_QSPI,
    .bus_config.qspi = {
        .qspi_dev = qspi,
        .qspi_freq = 50000000,
        .cs_gpio = gpio,
        .cs_pin = 19,
    },
    .rst_gpio = gpio,
    .rst_pin = 1,
};
lisa_display_attach_bus(display, &config);
```

### 使用 RGB 并行总线（双通道架构）

```c
#include "lisa_display.h"
#include "lisa_rgb.h"

// 获取设备
lisa_device_t *display = lisa_device_get("display");
lisa_device_t *rgb_ctrl = lisa_device_get("rgb0");      // RGB 控制器设备
lisa_device_t *cmd_bus = lisa_device_get("sw_spi_cmd"); // 独立命令通道（软件 SPI）
lisa_device_t *gpio = lisa_device_get("gpioa");

// 配置 RGB 总线（双通道模式）
lisa_display_config_t config = {
    .panel_name = "st7701s",  // ST7701S 使用 RGB 接口
    .bus_type = LISA_DISPLAY_BUS_RGB,
    .bus_instance = 0,
    .bus_config.rgb = {
        // RGB 数据通道配置
        .rgb_dev = rgb_ctrl,
        .bounce_buffer_size = 480 * 10,  // 10 行像素（驱动自动计算字节数）
        .input_format = LISA_RGB_INPUT_FORMAT_RGB565,
        .output_format = LISA_RGB_OUTPUT_FORMAT_RGB565,

        // 时序配置
        .timings = {
            .h_res = 480,
            .v_res = 480,
            .h_pulse_width = 10,
            .h_front_blanking = 20,
            .h_back_blanking = 10,
            .v_pulse_width = 10,
            .v_front_blanking = 10,
            .v_back_blanking = 1,
        },
        .pclk_hz = 6000000,  // 6MHz 像素时钟

        // 独立命令通道配置（用于 Panel 初始化）
        .cmd_bus_dev = cmd_bus,
    },
    .rst_gpio = gpio,
    .rst_pin = 1,
};
lisa_display_attach_bus(display, &config);

// 说明：RGB 模式使用双通道架构
// - 数据通道：RGB 并行接口，用于高速像素传输（使用 Bounce Buffer）
// - 命令通道：独立的软件 SPI，用于 Panel 初始化命令（3-Wire SPI）
```

### 使用 ST7701S 面板（RGB 接口）

ST7701S 是一款 480x480 分辨率的 TFT-LCD 显示控制器，适合使用 RGB 并行总线。

**Kconfig 配置**:
```kconfig
# 启用 Display 驱动
CONFIG_LISA_DISPLAY_DEVICE=y

# 选择 ST7701S 面板驱动
CONFIG_LISA_DISPLAY_PANEL_ST7701S=y

# 配置面板分辨率（在 Kconfig.st7701s 中）
CONFIG_PANEL_ST7701S_WIDTH=480
CONFIG_PANEL_ST7701S_HEIGHT=480

# 启用 RGB 总线和命令通道
CONFIG_LISA_DISPLAY_BUS_RGB=y
CONFIG_LISA_DISPLAY_CMD_BUS_SW_SPI=y

# 可选：启用 TE 同步
CONFIG_LISA_DISPLAY_TE_SYNC=y
```

**代码示例**:
```c
#include "lisa_display.h"

// 1. 获取设备
lisa_device_t *display = lisa_device_get("display");
lisa_device_t *rgb0 = lisa_device_get("rgb0");
lisa_device_t *sw_spi_cmd = lisa_device_get("sw_spi_cmd");
lisa_device_t *gpio = lisa_device_get("gpioa");

// 2. 配置 RGB + 软件 SPI 命令通道
lisa_display_config_t config = {
    .panel_name = "st7701s",
    .bus_type = LISA_DISPLAY_BUS_RGB,
    .bus_config.rgb = {
        .rgb_dev = rgb0,
        .bounce_buffer_size = 480 * 10,  // 10 行缓冲
        .input_format = LISA_RGB_INPUT_FORMAT_RGB565,
        .output_format = LISA_RGB_OUTPUT_FORMAT_RGB565,
        .timings = {
            .h_res = 480,
            .v_res = 480,
            .h_pulse_width = 10,
            .h_front_blanking = 20,
            .h_back_blanking = 10,
            .v_pulse_width = 10,
            .v_front_blanking = 10,
            .v_back_blanking = 1,
        },
        .pclk_hz = 6000000,  // 6MHz
        .cmd_bus_dev = sw_spi_cmd,  // 命令通道
    },
    .rst_gpio = gpio,
    .rst_pin = 1,
};

int ret = lisa_display_attach_bus(display, &config);
if (ret != LISA_DEVICE_OK) {
    // 错误处理
    return ret;
}

// 3. 开启显示
lisa_display_blanking_off(display);
lisa_display_set_brightness(display, 80);

// 4. 写入像素数据
lisa_display_capabilities_t caps;
lisa_display_get_capabilities(display, &caps);

lisa_display_buffer_desc_t desc = {
    .width = caps.width,
    .height = caps.height,
    .buf_size = caps.width * caps.height * 2,  // RGB565
};

// 从 PSRAM 分配帧缓冲
uint16_t *framebuffer = lisa_mem_alloc(desc.buf_size);

// 填充数据
// ...

// 写入显示
lisa_display_write(display, 0, 0, &desc, framebuffer);
```

**注意事项**:
- ST7701S 需要同时配置 RGB 数据总线和软件 SPI 命令总线
- 命令总线仅在 Panel 初始化时使用，运行时仅使用 RGB 数据通道
- RGB 模式下建议使用 PSRAM 分配大容量帧缓冲
- 根据实际硬件调整 RGB 时序参数（h_pulse_width、blanking 等）

## 自定义屏幕初始化参数

所有 Panel 驱动均支持在 `lisa_display_attach_bus()` 时传入自定义的初始化序列，覆盖驱动内置的默认序列。这在以下场景中非常有用：

- 同一款 Panel IC 搭配不同厂商的屏幕模组，需要不同的初始化参数
- 需要在运行时动态调整 Panel 初始化参数（如 Gamma 校正、刷新率等）
- 快速验证新的初始化序列，无需修改驱动代码

### 序列格式

`custom_init_params` 是一个 `uint8_t` 数组，按照 **寄存器地址 → 参数长度 → 参数数据** 的顺序逐条排列，多条寄存器配置依次拼接：

```
[寄存器地址] [参数长度 N] [参数1] [参数2] ... [参数N] [下一个寄存器地址] [参数长度] ...
```

示例：

```c
static const uint8_t custom_init_params[] = {
    // 寄存器地址, 参数长度, 参数数据...
    0x36, 1, 0x00,                          // 寄存器 0x36 (MADCTL)，1 个参数
    0x3A, 1, 0x05,                          // 寄存器 0x3A (COLMOD)，1 个参数，RGB565
    0xB2, 5, 0x0C, 0x0C, 0x00, 0x33, 0x33, // 寄存器 0xB2 (Porch Setting)，5 个参数
    0xC6, 1, 0x0F,                          // 寄存器 0xC6 (Frame Rate)，1 个参数，60Hz
};
```

### 使用方法

在 `lisa_display_config_t` 中设置 `panel_init_params` 和 `panel_init_params_len` 字段：

```c
lisa_display_config_t config = {
    .bus_type = LISA_DISPLAY_BUS_SPI_4WIRE,
    .bus_config.spi_4wire = {
        .spi_dev = spi,
        .cs_gpio = gpiob,
        .cs_pin = 1,
        .dc_gpio = gpiob,
        .dc_pin = 0,
        .spi_freq = 50000000,
    },
    .rst_gpio = gpioa,
    .rst_pin = 1,
    // 传入自定义初始化参数
    .panel_init_params = custom_init_params,
    .panel_init_params_len = sizeof(custom_init_params),
};
lisa_display_attach_bus(display, &config);
```

### 注意事项

- 如果不设置 `panel_init_params`（或设为 `NULL`/长度为 0），驱动将使用内置的默认初始化序列
- 序列中的寄存器地址为原始的 1 字节值（如 `0x36`），驱动会自动处理不同总线的命令格式转换
- 驱动在解析序列时会进行边界检查，如果数据截断会输出警告日志

## 硬件配置

### 引脚复用配置

Display 驱动依赖底层总线驱动（SPI、QSPI、GPIO、PWM）的引脚配置。需要在板型目录中配置相应的 pinmux 函数。

**配置位置**:
- **定义**: `boards/<板型名>/pinmux.c` 中实现各驱动的 pinmux 函数
- **声明**: `boards/<板型名>/pinmux.h` 中声明函数
- **调用时机**: 各底层设备初始化时自动调用

**示例** (参考 `boards/arcs_evb/pinmux.c`):

```c
// SPI1 引脚配置（用于 Display 数据传输）
void lisa_spi1_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 5, 6);  // SPI1 CLK
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 3, 6);  // SPI1 MOSI
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 1, 6);  // SPI1 CS/MISO
}

// GPIO 引脚配置（用于 RST、DC、CS 控制）
void lisa_gpioa_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 1, 1);  // LCD RST
}

void lisa_gpiob_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 0, 0);  // LCD DC
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 8, 0);  // LCD TE
}

// PWM 引脚配置（用于背光控制）
void lisa_pwm_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 0, 12); // LCD 背光 PWM
}
```

**注意**:
- 功能码需根据芯片手册确定
- 不同板型的引脚配置可能不同
- 只需配置实际使用的引脚

## 参数说明

### 总线类型

| 枚举值 | 说明 | 架构特点 | 适用场景 |
|-------|------|---------|---------|
| `LISA_DISPLAY_BUS_SPI_3WIRE` | 3线 SPI（SCLK, MOSI, CS） | 单通道，命令和数据复用 | 小屏幕、低成本方案 |
| `LISA_DISPLAY_BUS_SPI_4WIRE` | 4线 SPI（SCLK, MOSI, CS, D/C） | 单通道，D/C 区分命令和数据 | 通用方案 |
| `LISA_DISPLAY_BUS_QSPI` | QSPI 接口 | 单通道，四线并行传输 | 高速传输需求 |
| `LISA_DISPLAY_BUS_RGB` | RGB 并行接口 | **双通道架构**：数据通道（RGB）+ 命令通道（软件 SPI） | 高分辨率、高帧率场景 |

**RGB 总线特殊说明**：
- RGB 模式下，像素数据通过 RGB 并行接口高速传输（配置 `CONFIG_LISA_DISPLAY_BUS_RGB`）
- Panel 初始化命令通过独立的软件 SPI 通道发送（配置 `CONFIG_LISA_DISPLAY_CMD_BUS_SW_SPI`）
- 需要同时配置 `rgb_dev`（RGB 控制器）和 `cmd_bus_dev`（命令总线）两个设备
- 适用于 480x480 及以上分辨率的屏幕，如 ST7701S 等 Panel

### 像素格式

| 枚举值 | 说明 |
|-------|------|
| `LISA_DISPLAY_PIXEL_FORMAT_RGB_565` | RGB565，每像素 2 字节 |
| `LISA_DISPLAY_PIXEL_FORMAT_RGB_888` | RGB888，每像素 3 字节 |
| `LISA_DISPLAY_PIXEL_FORMAT_BGR_565` | BGR565，每像素 2 字节 |
| `LISA_DISPLAY_PIXEL_FORMAT_ARGB_8888` | ARGB8888，每像素 4 字节 |
| `LISA_DISPLAY_PIXEL_FORMAT_MONO_1` | 单色，每像素 1 位 |

### 显示方向

| 枚举值 | 说明 |
|-------|------|
| `LISA_DISPLAY_ORIENTATION_0` | 0° 正常方向 |
| `LISA_DISPLAY_ORIENTATION_90` | 90° 顺时针旋转 |
| `LISA_DISPLAY_ORIENTATION_180` | 180° 旋转 |
| `LISA_DISPLAY_ORIENTATION_270` | 270° 顺时针旋转 |

### 常用颜色宏（RGB565）

| 宏定义 | 值 | 说明 |
|-------|-----|------|
| `LISA_DISPLAY_COLOR_BLACK` | `0x0000` | 黑色 |
| `LISA_DISPLAY_COLOR_WHITE` | `0xFFFF` | 白色 |
| `LISA_DISPLAY_COLOR_RED` | `0xF800` | 红色 |
| `LISA_DISPLAY_COLOR_GREEN` | `0x07E0` | 绿色 |
| `LISA_DISPLAY_COLOR_BLUE` | `0x001F` | 蓝色 |

## 返回值说明

| 返回值 | 说明 |
|-------|------|
| `LISA_DEVICE_OK (0)` | 操作成功 |
| `LISA_DEVICE_ERR_INVALID` | 参数无效 |
| `LISA_DEVICE_ERR_NOT_SUPPORT` | 不支持该操作 |
| `LISA_DEVICE_ERR_NOT_READY` | 设备未就绪 |
| `LISA_DEVICE_ERR_IO` | IO 错误 |

## 线程安全与锁机制

Display 驱动采用**双层锁机制**确保多线程和多屏并发访问的安全性：

### 1. 总线级锁（Bus Mutex）

**作用**: 保护共享硬件总线的访问，防止多个 Panel 同时操作同一 SPI/QSPI 总线。

**特性**:
- **智能共享**: 在 [`arcs_attach_bus()`](lisa_display_arcs.c:63) 初始化时自动判断
  - 如果多个 Display 使用**相同的 `bus_dev` 指针**（即共享总线），则**复用同一个锁**
  - 如果使用**独立总线**，则**创建独立的锁**
- **零配置**: 用户无需手动配置，驱动自动识别并处理
- **精细粒度**: 仅在实际总线操作时加锁，最小化阻塞时间

**代码位置**:
- 锁定义: [`lisa_display_panel_t.bus_mutex`](lisa_display_panel.h:37)
- 锁创建/共享: [`arcs_attach_bus()`](lisa_display_arcs.c:164-192)
- 锁使用: [`panel_write_cmd_data()`](lisa_display_panel.c:42-70), [`panel_draw_pixels()`](lisa_display_panel.c:72-130)

**示例场景**:
```c
// 场景1: 两个屏幕共享 SPI1 总线（只是 CS 引脚不同）
// Display0 和 Display1 都使用 spi1 设备
// → 驱动自动共享同一个 bus_mutex

// 场景2: 两个屏幕使用独立总线
// Display0 使用 SPI1, Display1 使用 QSPI
// → 驱动为每个总线创建独立的 bus_mutex
```

### 2. 资源级锁（Rotate Mutex）

**作用**: 保护全局旋转缓冲区 [`g_rotate_ctx`](lisa_display_panel.c:280)，防止多个屏幕同时使用旋转资源。

**特性**:
- **全局唯一**: 所有 Display 实例共享同一个旋转锁
- **跨总线保护**: 即使使用不同总线的屏幕也需要互斥访问
- **防止重复初始化**: 使用 [`g_rotate_ctx_initialized`](lisa_display_panel.c:281) 标志位防止内存泄漏

**代码位置**:
- 锁定义: [`panel_rotate_ctx_t.rotate_mutex`](lisa_display_panel.c:275)
- 锁初始化: [`panel_rotate_init()`](lisa_display_panel.c:296-324)
- 锁使用: [`panel_send_data_with_sram_rotate()`](lisa_display_panel.c:410-483)

**重要**: 旋转缓冲区是**全局静态资源**，任何使用 90°/270° 旋转的屏幕都会竞争此资源，因此必须加锁保护。

### 3. 设备级锁（Device Mutex）

**作用**: 保护 Display 设备的上层 API 调用，防止同一设备的并发操作。

**代码位置**: [`arcs_display_priv_t.mutex`](lisa_display_arcs.c:25)

### 锁的层次关系

```
应用层
  ↓ lisa_display_write() 调用
[Device Mutex] ← 设备级锁（每个 Display 独立）
  ↓ panel_draw_pixels() 调用
[Bus Mutex] ← 总线级锁（共享总线时多个 Panel 共享）
  ↓ 如果需要旋转
[Rotate Mutex] ← 资源级锁（全局唯一，所有 Display 共享）
  ↓ 硬件操作
```

### 性能优化

- **无锁开销**: 独立总线的屏幕不会因总线锁产生性能损失
- **CPU/DMA 并行**: 旋转操作使用 Ping-Pong 缓冲，CPU 旋转下一块数据时 DMA 传输当前数据
- **CPDMA 加速**: 启用 [`CONFIG_LISA_DISPLAY_CPDMA_ROTATE`](Kconfig) 可用硬件 DMA 加速旋转

## 注意事项

### 通用注意事项

1. **配置顺序**: 必须先调用 `lisa_display_attach_bus()` 配置总线，才能使用其他显示 API
2. **帧缓冲区**: 建议从 PSRAM 分配帧缓冲区，确保 `CONFIG_PSRAM_HEAP_SIZE` 足够大
3. **亮度范围**: `lisa_display_set_brightness()` 参数范围为 0-100，表示百分比
4. **TE 同步**: 启用 `CONFIG_LISA_DISPLAY_TE_SYNC` 可防止画面撕裂，但需要连接 TE 引脚
5. **旋转性能**: 启用 `CONFIG_LISA_DISPLAY_CPDMA_ROTATE` 可使用硬件加速旋转，提升性能
6. **SPI 频率**: 实际 SPI 频率需根据屏幕规格和 PCB 走线质量调整
7. **线程安全**: 驱动采用双层锁机制（总线锁+资源锁），完全线程安全，可在多线程环境中安全使用
8. **依赖设备**: 使用前需确保底层设备（GPIO、SPI/QSPI、PWM）已正确初始化

### 多屏异显注意事项

9. **Panel 设备配置**: 使用 `config.panel_name` 指定每个显示屏使用的 Panel 驱动设备名称
   - 每个显示屏可以使用不同的 Panel（如 display 用 "st7789p3"，display1 用 "axs15231b"）
   - Panel 设备名称必须与驱动注册时使用的名称一致（见各 Panel 驱动的 `LISA_DEVICE_REGISTER`）
   
10. **总线实例配置**: 使用 `config.bus_instance` 区分不同的总线实例
    - SPI 4-Wire 支持多实例（`panel_bus_spi_4wire_0`, `panel_bus_spi_4wire_1`）
    - QSPI 和 RGB 目前为单实例
    
11. **独立控制**: 两个显示屏完全独立控制
    - 可以使用不同的分辨率、像素格式、方向
    - 可以独立设置亮度、开关显示
    
12. **资源隔离**: 确保两个显示屏的硬件资源不冲突
    - GPIO 引脚（RST、DC、CS、TE）不能重复使用
    - 背光 PWM 通道需要分配不同通道
    - SPI 总线需要不同的实例或通过 CS 引脚区分

13. **内存需求**: 双屏配置需要更多内存
    - 每个屏幕需要独立的帧缓冲区
    - 建议确保 PSRAM 容量足够（至少 2MB+）

14. **Kconfig 配置**:
    - 启用 `CONFIG_LISA_DUAL_DISPLAY=y`
    - 同时启用需要的多个 Panel 驱动（不再是单选）

15. **线程安全保证**:
    - **SPI 一主多从场景**: 共享总线的多个屏幕会自动共享总线锁，确保不会同时访问
    - **旋转缓冲区保护**: 全局旋转资源受全局锁保护，避免数据竞争
    - **错误处理**: 所有错误路径都正确释放锁，不会导致死锁
16. **RGB 双通道架构**（重要）:
   - RGB 总线采用双通道设计：**数据通道**（RGB 并行）+ **命令通道**（软件 SPI）
   - 数据通道使用 Bounce Buffer 模式，自动管理双缓冲和 DMA 传输
   - 命令通道用于 Panel 初始化，独立于数据传输
   - 配置时需同时提供 `rgb_dev`（RGB 控制器）和 `cmd_bus_dev`（命令总线）
   - RGB 总线层会自动路由像素数据到正确的控制器设备（`priv->rgb_dev`）
   - 命令通道实现为软件 SPI（3-Wire），仅在初始化阶段使用，运行时不影响性能
17. **Bounce Buffer 配置**（RGB 模式）:
    - `bounce_buffer_size` 单位为**像素数**，驱动会根据 `bpp` 自动计算字节数
    - 实际分配 = `bounce_buffer_size * bpp / 8` 字节
    - 推荐配置：10-20 行像素（如 `480 * 10`，RGB565 下占 9600 字节）
    - Bounce buffer 字节大小必须是帧缓冲字节大小的整数因子

## 架构说明

LISA Display 驱动采用分层架构设计，将应用接口、Panel 通用逻辑、Panel 专用驱动和总线专用驱动分离开来，以提供灵活性和可移植性。

### 标准总线架构（SPI/QSPI）

```
+-------------------------------------------------------------------------+
|                                应用层                                   |
|                    +---------------------------+                        |
|                    |   应用程序 (e.g. main.c)  |                        |
|                    +---------------------------+                        |
+--------------------------------|----------------------------------------+
                                 | (调用)
                                 v
+--------------------------------+----------------------------------------+
|                      LISA Display 驱动框架                              |
|                                                                         |
|   +-------------------------------------------------------------------+ |
|   | lisa_display_api_t (lisa_display.h)                               | |
|   +--------------------------------|----------------------------------+ |
|                                    | (由...实现)                        |
|                                    v                                    |
|   +-------------------------------------------------------------------+ |
|   | Display 设备层 (lisa_display_arcs.c)                              | |
|   | (设备注册, 互斥锁, TE同步)                                        | |
|   +--------------------------------|----------------------------------+ |
|                                    | (通过Kconfig选择并转发至)          |
|                                    v                                    |
|   +-------------------------------------------------------------------+ |
|   | Panel 驱动 (e.g. panel_st7789p3.c)                                | |
|   | (IC专用初始化和命令)                                              | |
|   +--------------------------------|----------------------------------+ |
|                                    | (实现)                             |
|                                    v                                    |
|   +-------------------------------------------------------------------+ |
|   | Panel 抽象层 (lisa_display_panel.c)                               | |
|   | (Panel通用逻辑, 旋转算法)                                         | |
|   +--------------------------------|----------------------------------+ |
|                                    | (调用)                             |
|                                    v                                    |
|   +-------------------------------------------------------------------+ |
|   | 总线抽象层 (lisa_display_bus.h)                                   | |
|   | (定义标准总线API)                                                 | |
|   +--------------------------------|----------------------------------+ |
|                                    | (由...实现)                        |
|                                    v                                    |
|   +-------------------------------------------------------------------+ |
|   | 总线驱动 (e.g. bus_spi_4wire.c)                                   | |
|   | (实现总线专用数据传输)                                            | |
|   +-------------------------------------------------------------------+ |
|                                                                         |
+--------------------------------|----------------------------------------+
                                 | (通过...访问硬件)
                                 v
+--------------------------------+----------------------------------------+
|                      硬件抽象层 (HAL)                                   |
|   +-------------------------------------------------------------------+ |
|   | LISA 设备驱动 (SPI, GPIO, PWM, DMA)                               | |
|   +-------------------------------------------------------------------+ |
+-------------------------------------------------------------------------+
```

### RGB 双通道架构

RGB 模式采用特殊的双通道设计，将命令传输和数据传输分离：

```
+-------------------------------------------------------------------------+
|                          应用层 (LVGL / Panel)                          |
+--------------------------------|----------------------------------------+
                                 |
                                 v
+--------------------------------+----------------------------------------+
|                      Display Bus API                                    |
|   (lisa_display_bus_write_pixels / trans_cmd_data)                     |
+-----------|--------------------------|----------------------------------+
            |                          |
            | 像素数据                 | 命令/参数
            v                          v
+------------------------+    +----------------------------------------+  |
| RGB 总线层             |    | 命令总线层 (cmd_bus_dev)               |  |
| (lisa_display_bus_rgb) |    | (lisa_display_cmd_bus_sw_spi)          |  |
| - 转发像素数据         |    | - 发送初始化命令                       |  |
| - 持有 rgb_dev 引用    |    | - 软件 SPI 3-Wire                      |  |
+-----------|------------+    +----------------------------------------+  |
            |
            | lisa_rgb_update_framebuffer(priv->rgb_dev)
            v
+------------------------+                                                 |
| RGB 控制器层           |                                                 |
| (lisa_rgb_arcs.c)      |                                                 |
| - 双缓冲管理           |                                                 |
| - Bounce Buffer        |                                                 |
| - GPDMA Ping-Pong      |                                                 |
+-----------|------------+                                                 |
            |
            v
+------------------------+                                                 |
| HAL (RGB + GPDMA)      |                                                 |
+-------------------------------------------------------------------------+
```

**关键设计**：
- **数据通道**：RGB 并行接口，使用 Bounce Buffer 和 Ping-Pong DMA，帧缓冲在 PSRAM
- **命令通道**：独立的软件 SPI（3-Wire），仅用于 Panel 初始化阶段
- **设备引用传递**：总线层通过 `priv->rgb_dev` 保存控制器设备引用
- **自动路由**：`rgb_bus_write_pixels()` 自动路由到正确的控制器设备
- **双缓冲机制**：应用层写入非显示缓冲区，DMA 中断自动切换缓冲区

### 各层描述

- **Display 设备层 (`lisa_display_arcs.c`)**: 实现 `lisa_display_api_t`，处理设备注册、互斥锁保护、TE 同步，通过 Kconfig 链接到特定 Panel 驱动
- **Panel 驱动层 (`panel_*.c`)**: 针对特定显示控制器（如 ST7789P3）的初始化序列和专用命令
- **Panel 抽象层 (`lisa_display_panel.c`)**: 提供独立于显示控制器的通用逻辑，如旋转算法、辅助函数
- **总线抽象层 (`lisa_display_bus.h`)**: 定义用于不同硬件总线通信的标准 `lisa_display_bus_api_t` 接口
- **总线驱动层 (`bus_*.c`)**: 实现特定总线协议（如 4 线 SPI、QSPI）的数据传输

```{toctree}
:maxdepth: 1
:hidden:

panels/README.md
```

## Panel 移植适配

如需添加新的显示面板驱动，请参考 [Panel 移植适配指南](panels/README.md)。

## 文件说明

### 核心文件
- `lisa_display.h` - 驱动头文件，包含所有 API 和类型定义
- `lisa_display_arcs.c` - ARCS 平台 Display 设备实现
- `lisa_display_panel.c` - Panel 抽象层实现
- `lisa_display_panel.h` - Panel 驱动接口定义
- `lisa_display_bus.h` - 总线抽象层接口定义
- `mipi_dcs.h` - MIPI DCS 命令定义

### 总线驱动（bus/ 目录）
- `bus/lisa_display_bus_spi_4wire.c` - 4线 SPI 总线驱动
- `bus/lisa_display_bus_spi_3wire.c` - 3线 SPI 总线驱动
- `bus/lisa_display_bus_qspi.c` - QSPI 总线驱动
- `bus/lisa_display_bus_rgb.c` - RGB 并行总线驱动
- `bus/lisa_display_cmd_bus_sw_spi.c` - 软件 SPI 命令通道驱动

### Panel 驱动（panels/ 目录）
- `panels/panel_st7789p3.c` - ST7789P3 面板驱动
- `panels/panel_axs15231b.c` - AXS15231B 面板驱动
- `panels/panel_st7701s.c` - ST7701S 面板驱动（480x480 RGB 接口）
- `panels/panel_nv3030b.c` - NV3030B 面板驱动（170x320 SPI 4-Wire 接口）

### 配置文件
- `Kconfig` - 顶层配置选项
- `bus/Kconfig` - 总线配置选项
- `panels/Kconfig` - Panel 选择配置
- `panels/Kconfig.st7789p3` - ST7789P3 配置
- `panels/Kconfig.axs15231b` - AXS15231B 配置
- `panels/Kconfig.st7701s` - ST7701S 配置
- `panels/Kconfig.nv3030b` - NV3030B 配置
- `CMakeLists.txt` - 构建配置
