# LISA Display 组件

LISA Display 组件是 ARCS SDK 中的核心显示驱动组件，提供统一的显示设备抽象接口，支持多种 LCD 和 EPD 显示控制器。

## 📖 组件概述

### 主要功能
- **统一的显示设备抽象**：提供标准化的显示操作接口
- **多显示控制器支持**：支持 8 种常见的 LCD/EPD 控制器
- **灵活的传输接口**：支持 SPI 4-line 和 QSPI 传输模式
- **显示同步机制**：支持 TE 同步和 BUSY 同步
- **屏幕亮度控制**：支持 PWM 和 Wire Dimming 两种亮度调节方式
- **屏幕方向控制**：支持 4 种屏幕方向旋转

### 支持的显示控制器
| 控制器型号 | 类型 | 特殊功能 |
|-----------|------|----------|
| AXS15231B | LCD | - |
| ST77926   | LCD | 默认选择 |
| ST7789P3  | LCD | - |
| ST7789V   | LCD | - |
| GC9309NA  | LCD | - |
| NV3030B   | LCD | - |
| GC9A01    | LCD | - |
| UC8253C   | EPD | 需要 BUSY 同步 |

## 🚀 快速开始

### 1. 配置项启用

在项目配置中启用 LISA Display：

```kconfig
CONFIG_LISA_DISPLAY=y
CONFIG_LISA_DISPLAY_ST77926=y  # 选择控制器类型
```

### 2. 硬件配置

配置显示硬件参数：

```c
display_hw_config_t display_config = {
    .trans_config = {
        .spi_4line = {
            .spi_dev = spi_device_handle,
            .spi_tx_dma_ch = 1,
            .spi_sck_freq = 50000000,  // 50MHz
            .spi_pins = {
                .cs  = {pad, pin, func},
                .clk = {pad, pin, func},
                .sda = {pad, pin, func},
                .dc  = {pad, pin, func}
            }
        }
    },
    .blacklight = {
        .pin = {pad, pin, func},
        .dev = pwm_device_handle,
        .freq = 1000,  // 1KHz PWM
        .channel = 0
    },
    .reset = {pad, pin, func},
    .te = {pad, pin, func}
};
```

### 3. 设备初始化

```c
// 创建显示设备
const struct display_device *display_dev = lisa_display_create(&display_config);
if (!display_dev) {
    // 初始化失败处理
    return -1;
}

// 获取显示能力
struct display_capabilities caps;
lisa_display_get_capabilities(display_dev, &caps);
```

### 4. 基本显示操作

```c
// 开启显示
lisa_display_blanking_off(display_dev);

// 设置亮度 (0-100)
lisa_display_set_brightness(display_dev, 80);

// 写入图像数据
struct display_buffer_descriptor desc = {
    .buf_size = width * height * 2,  // RGB565
    .width = width,
    .height = height,
    .pitch = width
};
lisa_display_write(display_dev, x, y, &desc, image_buffer);

// 设置屏幕方向
lisa_display_set_orientation(display_dev, DISPLAY_ORIENTATION_ROTATED_90);
```

## 🔧 配置选项

### 显示控制器选择
```kconfig
# 从以下控制器中选择一个
CONFIG_LISA_DISPLAY_AXS15231B=y
CONFIG_LISA_DISPLAY_ST77926=y      # 默认
CONFIG_LISA_DISPLAY_ST7789P3=y
CONFIG_LISA_DISPLAY_ST7789V=y
CONFIG_LISA_DISPLAY_GC9309NA=y
CONFIG_LISA_DISPLAY_NV3030B=y
CONFIG_LISA_DISPLAY_GC9A01=y
CONFIG_LISA_DISPLAY_UC8253C=y      # EPD控制器
```

### 同步机制配置
```kconfig
# TE同步 - 防止撕裂效应
CONFIG_LISA_DISPLAY_TE_SYNC=y

# BUSY同步 - EPD显示器专用
CONFIG_LISA_DISPLAY_BUSY_SYNC=y
```

### 亮度控制方式
```kconfig
# PWM亮度控制（默认）
CONFIG_LISA_DISPLAY_BRIGHTNESS_TYPE_PWM=y

# Wire Dimming亮度控制
CONFIG_LISA_DISPLAY_BRIGHTNESS_TYPE_WIRE_DIMMING=y
```

### 硬件加速选项
```kconfig
# CPDMA旋转加速
CONFIG_LISA_DISPLAY_CPDMA_ROTATE=y
CONFIG_LISA_DISPLAY_CPDMA_CH=2
```

## 📋 API 参考

### 核心数据结构

#### `display_hw_config_t`
显示硬件配置结构体，包含传输接口、背光、复位、TE等引脚配置。

#### `struct display_capabilities`
显示能力结构体，包含分辨率、像素格式、当前方向等信息。

#### `struct display_buffer_descriptor` 
显示缓冲区描述符，定义图像数据的格式和尺寸。

### 主要API函数

#### 设备管理
- `lisa_display_create(config)` - 创建显示设备实例
- `lisa_display_get()` - 获取当前显示设备

#### 显示控制
- `lisa_display_blanking_on(dev)` - 关闭显示（保留缓冲区）
- `lisa_display_blanking_off(dev)` - 开启显示
- `lisa_display_sleep(dev, onoff)` - 显示睡眠控制

#### 显示操作
- `lisa_display_write(dev, x, y, desc, buf)` - 写入图像数据
- `lisa_display_set_orientation(dev, orientation)` - 设置屏幕方向
- `lisa_display_set_brightness(dev, brightness)` - 设置亮度

#### 信息查询
- `lisa_display_get_capabilities(dev, caps)` - 获取显示能力

### 像素格式支持
- `PIXEL_FORMAT_RGB_888` - 24位RGB
- `PIXEL_FORMAT_ARGB_8888` - 32位ARGB
- `PIXEL_FORMAT_RGB_565` - 16位RGB（常用）
- `PIXEL_FORMAT_BGR_565` - 16位BGR
- `PIXEL_FORMAT_MONO_1` - 1位单色

### 显示方向
- `DISPLAY_ORIENTATION_NORMAL` - 正常方向
- `DISPLAY_ORIENTATION_ROTATED_90` - 顺时针90度
- `DISPLAY_ORIENTATION_ROTATED_180` - 180度
- `DISPLAY_ORIENTATION_ROTATED_270` - 顺时针270度

## 🔍 传输接口

### SPI 4-line 接口
标准的 4 线 SPI 接口，包含：
- **CS** (Chip Select) - 片选信号
- **CLK** (Clock) - 时钟信号  
- **SDA** (Serial Data) - 数据信号
- **DC** (Data/Command) - 数据/命令选择

### QSPI 接口  
四线 SPI 接口，支持更高传输速度：
- **CS** (Chip Select) - 片选信号
- **CLK** (Clock) - 时钟信号
- **DIO[0:3]** - 4条数据线

## ⚠️ 注意事项

1. **控制器互斥**：同一时间只能启用一个显示控制器
2. **引脚配置**：确保引脚配置与硬件设计匹配
3. **DMA通道**：避免DMA通道冲突
4. **时钟频率**：根据控制器规格设置合适的SPI时钟频率
5. **TE同步**：使用TE同步时确保TE引脚连接正确
6. **EPD控制器**：UC8253C等EPD控制器需要BUSY同步机制

