# RGB LCD 驱动

## 概述

`lisa_rgb` 是针对 RGB (并口) LCD 显示屏的设备驱动模型，封装了对 HAL 层 `Driver_RGB` 和 `Driver_GPDMA` 的调用，提供统一的设备接口。

## 功能特性

- ✅ 支持 RGB565/RGB888 输入格式
- ✅ 支持 RGB565/RGB666/RGB888 输出格式
- ✅ 支持 8/16/24 位并口输出
- ✅ 支持 SYNC+DE / SYNC-ONLY 同步模式
- ✅ 支持单次传输 / 连续刷新模式
- ✅ 使用 GPDMA 高效传输帧缓冲数据
- ✅ 提供互斥锁保护多线程访问
- ✅ 信号量同步传输完成事件

## 架构设计

```
┌────────────────────────────────────────────────────────────┐
│            应用层 (LVGL / Panel 驱动)                       │
├────────────────────────────────────────────────────────────┤
│       Display Bus API (lisa_display_bus_write_pixels)      │
├────────────────────────────────────────────────────────────┤
│  RGB 总线层 (lisa_display_bus_rgb.c)                       │
│  - rgb_bus_write_pixels() 转发像素数据                     │
│  - 持有 RGB 控制器设备引用 (priv->rgb_dev)                 │
├────────────────────────────────────────────────────────────┤
│  RGB 控制器层 (lisa_rgb_arcs.c)                            │
│  - arcs_rgb_update_framebuffer() 管理双缓冲                │
│  - bounce_buffer_fill() 分块传输                           │
│  - gpdma_event_callback() 自动切换缓冲区                   │
├────────────────────────────────────────────────────────────┤
│       HAL Driver_RGB + Driver_GPDMA                        │
├────────────────────────────────────────────────────────────┤
│            硬件 RGB 控制器 + GPDMA                          │
└────────────────────────────────────────────────────────────┘
```

**关键设计**：
- **双层架构**：总线层（bus）作为抽象接口，控制器层（controller）负责硬件实现
- **设备引用传递**：总线层在 `rgb_bus_attach()` 时保存控制器设备引用 `priv->rgb_dev`
- **API 调用链**：`rgb_bus_write_pixels()` → `lisa_rgb_update_framebuffer(priv->rgb_dev)` → `arcs_rgb_update_framebuffer()`
- **独立命令通道**：Panel 初始化命令通过独立的软件 SPI/I2C 通道发送，不经过 RGB 数据总线

## 快速开始

### 典型使用流程

```c
#include "lisa_device.h"
#include "lisa_rgb.h"
#include "lisa_display.h"

// 1. 获取设备（由设备框架自动注册）
lisa_device_t *rgb = lisa_device_get("rgb0");
lisa_device_t *display = lisa_device_get("display");

// 2. 通过 Display Bus API 配置 RGB（推荐方式）
lisa_display_config_t config = {
    .bus_type = LISA_DISPLAY_BUS_RGB,
    .bus_config.rgb = {
        .rgb_dev = rgb,
        .bounce_buffer_size = 480 * 10,  // 10 行像素
        .input_format = LISA_RGB_INPUT_FORMAT_RGB565,
        .output_format = LISA_RGB_OUTPUT_FORMAT_RGB565,
        .timings = {
            .h_res = 480, .v_res = 480,
            .h_pulse_width = 10, .h_front_blanking = 20,
            .h_back_blanking = 10, .v_pulse_width = 10,
            .v_front_blanking = 10, .v_back_blanking = 1,
        },
        .pclk_hz = 6000000,
        .cmd_bus_dev = cmd_bus,  // 独立命令通道
    },
};
lisa_display_attach_bus(display, &config);

// 3. RGB 传输已自动启动，直接更新帧缓冲即可
uint16_t *framebuffer = allocate_framebuffer();
memset(framebuffer, 0xFFFF, 480 * 480 * 2);  // 填充白色

// 4. 更新显示内容（通过 Display Bus 层）
lisa_display_write(display, 0, 0, &desc, framebuffer);

// 或者直接调用 RGB API（高级用法）
lisa_rgb_update_framebuffer(rgb, framebuffer, 480 * 480 * 2);
```

**重要提示**：
- ✅ **推荐**：通过 `lisa_display_attach_bus()` 使用 RGB，由框架自动管理
- ⚠️ **高级**：直接调用 `lisa_rgb_*()` API 需要理解双层架构
- ❌ **错误**：不要混用 Display Bus 层和 RGB 控制器层的 API

## 配置选项

在 `menuconfig` 中配置驱动参数：

```
Device Drivers
  └─ LISA Device Drivers
      └─ [*] Enable LISA RGB LCD driver
          ├─ (1) GPDMA channel for RGB data transfer
          ├─ (6000000) RGB pixel clock frequency (Hz)
          ├─ (480) Horizontal resolution (pixels)
          ├─ (480) Vertical resolution (pixels)
          ├─ RGB output bus width (24-bit RGB)
          ├─ RGB input data format (RGB565)
          ├─ [*] Enable continuous DE signal output
          └─ [*] Enable continuous frame mode
```

## 使用示例

### 1. 获取设备实例

```c
#include "lisa_device.h"
#include "lisa_rgb.h"

lisa_device_t *rgb = lisa_device_get(LISA_RGB0_NAME);
if (!rgb) {
    // 设备未注册或初始化失败
    return;
}
```

### 2. 启动 RGB 传输

```c
// RGB 传输由 Display Bus 层在 attach 时自动启动
// 如果直接使用 RGB API，只需调用：
int ret = lisa_rgb_start(rgb);
if (ret != LISA_DEVICE_OK) {
    // 启动失败
}

// 注意：
// - RGB 控制器启动后会持续运行，自动循环显示当前帧缓冲
// - 使用 Bounce Buffer 模式，无需手动管理帧缓冲切换
// - 推荐通过 Display Bus 层使用，由框架自动管理启动
```

### 3. 等待传输完成（单次模式）

```c
// 等待传输完成（最长 1000ms）
ret = lisa_rgb_wait_done(rgb, 1000);
if (ret == LISA_DEVICE_ERR_TIMEOUT) {
    // 传输超时
}
```

### 4. 停止传输

```c
ret = lisa_rgb_stop(rgb);
if (ret != LISA_DEVICE_OK) {
    // 停止失败
}
```

### 5. 更新帧缓冲区（Bounce Buffer 模式）

```c
uint16_t *framebuffer = get_current_framebuffer();
// ... LVGL 或应用层绘制新内容到 framebuffer ...

// 更新显示内容（拷贝到非显示缓冲区并标记待切换）
int ret = lisa_rgb_update_framebuffer(rgb, framebuffer, 480 * 480 * 2);
if (ret != LISA_DEVICE_OK) {
    // 更新失败（可能是信号量超时）
}

// DMA 中断会在下一帧完成时自动切换显示缓冲区
```

### 6. 完整示例流程

```c
#include "lisa_device.h"
#include "lisa_display.h"

// 1. 通过 Display Bus 层配置（推荐）
lisa_device_t *display = lisa_device_get("display");
lisa_display_config_t config = {
    .bus_type = LISA_DISPLAY_BUS_RGB,
    .bus_config.rgb = {
        .rgb_dev = lisa_device_get("rgb0"),
        .bounce_buffer_size = 480 * 10,
        .input_format = LISA_RGB_INPUT_FORMAT_RGB565,
        // ... 其他配置 ...
    },
};
lisa_display_attach_bus(display, &config);  // 自动调用 setup 和 start

// 2. 更新显示内容
uint16_t *fb = allocate_framebuffer();
draw_content(fb);  // 绘制内容
lisa_display_write(display, 0, 0, &desc, fb);  // 通过 Display Bus 更新
```

### 7. Bounce Buffer 模式

Bounce Buffer 模式通过小的内部 RAM 缓冲区分块传输大的 PSRAM 帧缓冲，实现高效的显示刷新。这是唯一支持的工作模式。

#### 7.1 配置

```c
lisa_display_bus_rgb_config_t config = {
    // ... 其他配置 ...
    .bounce_buffer_size = 480 * 10,  // 10 行像素数（驱动会根据 bpp 自动计算字节数）
    .input_format = LISA_RGB_INPUT_FORMAT_RGB565,  // RGB565 格式，bpp=16
};

// 实际 bounce buffer 内存分配大小 = bounce_buffer_size * bpp / 8
// 例如：480 * 10 * 16 / 8 = 9600 字节
```

#### 7.2 使用示例

```c
lisa_device_t *rgb = lisa_device_get(LISA_RGB0_NAME);

// 启动 RGB 传输（Bounce Buffer 模式会自动初始化）
lisa_rgb_start(rgb);

// 更新图像数据
uint16_t *new_frame = get_new_frame_data();
lisa_rgb_update_framebuffer(rgb, new_frame, 480 * 480 * 2);

// DMA 会自动：
// 1. 等待上一帧显示完成
// 2. 拷贝新帧数据到非显示缓冲区
// 3. 在下一帧传输完成时切换缓冲区

// 说明：
// - lisa_rgb_start() 会自动初始化 Bounce Buffer 和 Ping-Pong DMA
// - 双帧缓冲已在 lisa_rgb_setup() 中分配（PSRAM）
// - Bounce Buffer 已在 lisa_rgb_setup() 中分配（内部 SRAM）
```

#### 7.3 工作原理

```
┌─────────────────────────────────────────────────────┐
│ PSRAM 帧缓冲 (480x480x2 = 460KB)                    │
│  [显示缓冲 fb_buf[0]]  [写入缓冲 fb_buf[1]]         │
└──────────────┬──────────────────────────────────────┘
               │ 分块拷贝
               ▼
┌─────────────────────────────────────────────────────┐
│ 内部 RAM Bounce Buffer (480x10x2 = 9.6KB)           │
│  [Ping Buffer]  ⇄  [Pong Buffer]                    │
└──────────────┬──────────────────────────────────────┘
               │ GPDMA Ping-Pong 传输
               ▼
┌─────────────────────────────────────────────────────┐
│ RGB 控制器 FIFO                                      │
└─────────────────────────────────────────────────────┘
```

#### 7.4 优势

- **内存效率**：Bounce Buffer 只需几KB内部RAM，帧缓冲使用PSRAM
- **无闪烁**：双缓冲机制确保显示和写入分离
- **自动管理**：DMA 中断自动填充和切换，应用层无需手动干预
- **高性能**：内部 RAM 作为 DMA 源，传输速度快

## API 参考

### 数据结构

#### `lisa_display_bus_rgb_config_t`
RGB 总线配置结构体，用于配置 RGB 控制器和 Bounce Buffer 模式。

**关键字段**：
```c
typedef struct {
    lisa_device_t *rgb_dev;              // RGB 控制器设备（必需）
    lisa_device_t *cmd_bus_dev;          // 独立命令总线设备（可选，用于 Panel 初始化）

    uint32_t bounce_buffer_size;         // Bounce Buffer 大小（像素数，非字节数）

    lisa_rgb_input_format_t input_format;   // 输入格式（RGB565/RGB888）
    lisa_rgb_output_format_t output_format; // 输出格式

    lisa_rgb_timings_t timings;          // 时序参数
    uint32_t pclk_hz;                    // 像素时钟频率

    lisa_rgb_polarity_t hsync_polarity;  // 水平同步极性
    lisa_rgb_polarity_t vsync_polarity;  // 垂直同步极性
    lisa_rgb_polarity_t de_polarity;     // 数据使能极性
    lisa_rgb_polarity_t pclk_polarity;   // 像素时钟极性

    bool output_lsb_first;               // 是否 LSB 优先输出
} lisa_display_bus_rgb_config_t;
```

#### `lisa_rgb_timings_t`
RGB 时序参数结构体。

**字段说明**：
```c
typedef struct {
    uint16_t h_res;              // 水平分辨率（像素）
    uint16_t v_res;              // 垂直分辨率（像素）
    uint16_t h_pulse_width;      // 水平同步脉冲宽度（PCLK）
    uint16_t h_front_blanking;   // 水平前肩消隐（PCLK）
    uint16_t h_back_blanking;    // 水平后肩消隐（PCLK）
    uint16_t v_pulse_width;      // 垂直同步脉冲宽度（行）
    uint16_t v_front_blanking;   // 垂直前肩消隐（行）
    uint16_t v_back_blanking;    // 垂直后肩消隐（行）
} lisa_rgb_timings_t;
```

### 函数接口

#### `lisa_rgb_setup()`
配置 RGB 控制器和 Bounce Buffer 模式。

**函数签名**：
```c
int lisa_rgb_setup(lisa_device_t *dev, const lisa_display_bus_rgb_config_t *config);
```

**功能**：
- 初始化 RGB 控制器硬件参数（时序、格式、极性等）
- 启用 RGB 像素时钟输出
- 初始化 GPDMA 通道配置
- 分配双帧缓冲区（PSRAM，每个 `fb_size` 字节）
- 分配 Bounce Buffer（内部 SRAM，每个 `bb_size` 字节）
- 初始化信号量和互斥锁

**参数**：
- `config->bounce_buffer_size`: Bounce Buffer 大小（单位：像素数）
- `config->input_format`: 输入格式（RGB565/RGB888）
- `config->output_format`: 输出格式
- `config->timings`: 时序参数（分辨率、消隐期等）
- `config->pclk_hz`: 像素时钟频率

**注意**：
- 此函数通常由 Display Bus 层在 `attach` 时自动调用
- 应用层一般不需要直接调用此函数
- `bounce_buffer_size` 必须使得 `fb_size % (bounce_buffer_size * bpp / 8) == 0`

#### `lisa_rgb_start()`
启动 RGB 传输，初始化 Bounce Buffer 并启动 GPDMA Ping-Pong 传输。

**函数签名**：
```c
int lisa_rgb_start(lisa_device_t *dev);
```

**功能**：
- 预填充两个 Bounce Buffer（Ping 和 Pong）
- 启动 GPDMA Ping-Pong DMA 传输
- 启动 RGB 控制器连续输出

**注意**：
- 此函数只需在初始化时调用一次
- 调用前必须已通过 `lisa_rgb_setup()` 完成配置
- 启动后 RGB 控制器会持续输出，自动循环显示当前帧缓冲内容

#### `lisa_rgb_stop()`
停止 RGB 传输。

**函数签名**：
```c
int lisa_rgb_stop(lisa_device_t *dev);
```

**功能**：
- 停止 RGB 控制器输出
- 不会停止 GPDMA（GPDMA 会自动在缓冲区耗尽后停止）

**注意**：正常使用时通常不需要停止 RGB 传输

#### `lisa_rgb_wait_done()`
等待传输完成（仅在单次传输模式下需要）。

**注意**：Bounce Buffer 模式下通常不需要调用此函数，因为双缓冲机制会自动管理同步。

#### `lisa_rgb_enable_clock()`
启用/禁用像素时钟输出。

#### `lisa_rgb_update_framebuffer()`
更新帧缓冲区数据，将新帧数据拷贝到非显示缓冲区并标记待切换。这是 Bounce Buffer 模式下更新显示内容的主要接口。

**函数签名**：
```c
int lisa_rgb_update_framebuffer(lisa_device_t *dev, const void *buf, uint32_t size);
```

**参数**：
- `dev`: RGB 控制器设备（注意：必须是 `rgb0` 设备，而非 `panel_bus_rgb` 总线设备）
- `buf`: 新的帧缓冲区数据指针
- `size`: 数据大小（字节），建议为完整帧大小（`width * height * bpp / 8`）

**工作流程**：
1. 等待写入信号量（确保上一帧已完成切换）
2. 拷贝新数据到非显示缓冲区（`fb_buf[1 - display_idx]`）
3. 设置 `swap_pending = 1`，标记有新帧待显示
4. DMA 中断会在下一帧传输完成时自动切换 `display_idx`


**注意**：
- 此函数在 Bounce Buffer 模式下自动管理双缓冲，应用层无需关心缓冲区切换逻辑
- 如果传入的 `size` 超过帧缓冲大小，会被截断并输出警告
- 函数内部会阻塞等待信号量（最长 1000ms），确保不与上一次写入冲突

## 注意事项

1. **GPDMA 通道冲突**：确保配置的 GPDMA 通道不与其他外设冲突
2. **帧缓冲区对齐**：帧缓冲区地址建议 32 字节对齐以获得最佳性能
3. **时钟频率**：像素时钟频率需要根据 LCD 面板规格配置
4. **连续模式**：在连续刷新模式下，无需调用 `wait_done()`，RGB 控制器会自动循环发送数据
5. **D-Cache**：如果启用 D-Cache，需要在更新帧缓冲区后刷新 Cache
6. **Bounce Buffer 模式**（必需）：
   - `bounce_buffer_size` 配置必须设置为 > 0（单位：像素数）
   - 实际分配大小 = `bounce_buffer_size * bpp / 8` 字节
   - Bounce buffer 字节大小必须是帧缓冲字节大小的整数因子
   - 推荐配置：10-20 行像素（如 `480 * 10`，RGB565 下占 9600 字节）
   - 使用 `lisa_rgb_update_framebuffer()` 更新帧数据
   - 自动双缓冲机制，无需手动管理缓冲区切换
   - 帧缓冲分配在 PSRAM，Bounce buffer 分配在内部 SRAM
   - 内部自动使用 Ping-Pong DMA 进行高效传输
   - RGB 总线层会自动路由到正确的 RGB 控制器设备

## 硬件接口

### RGB 信号引脚

| 信号 | 描述 | 方向 |
|------|------|------|
| PCLK | 像素时钟 | 输出 |
| HSYNC | 水平同步 | 输出 |
| VSYNC | 垂直同步 | 输出 |
| DE | 数据使能 | 输出 |
| R[7:0] | 红色数据 | 输出 |
| G[7:0] | 绿色数据 | 输出 |
| B[7:0] | 蓝色数据 | 输出 |

### 时序参数示例

以 480x480 LCD 为例：

```
水平时序:
  h_res = 480
  h_pulse_width = 10
  h_front_blanking = 20
  h_back_blanking = 10
  总水平周期 = 480 + 10 + 20 + 10 = 520 PCLK

垂直时序:
  v_res = 480
  v_pulse_width = 10
  v_front_blanking = 10
  v_back_blanking = 1
  总垂直周期 = 480 + 10 + 10 + 1 = 501 行

帧率计算:
  PCLK = 6MHz
  帧率 = 6,000,000 / (520 * 501) ≈ 23 fps
```

## 故障排查

### 屏幕无显示
1. 检查 PCLK 频率是否正确
2. 检查时序参数是否匹配 LCD 规格
3. 检查信号极性配置
4. 确认帧缓冲区数据正确

### 屏幕闪烁
1. 检查帧率是否太低
2. 检查 GPDMA 传输是否及时
3. 确认时序参数稳定
4. 验证 Bounce Buffer 大小是否为帧缓冲大小的整数因子

### 颜色错误
1. 检查输入/输出格式配置
2. 检查 RGB 数据线接线
3. 确认 LSB/MSB 配置正确
4. 验证 `bpp` 计算与实际格式匹配（RGB565=16, RGB888=24）
