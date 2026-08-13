# Display 双屏异显示例

## 功能说明

演示如何使用 LISA Display 驱动同时控制两个独立的显示屏，实现多屏异显功能。每个显示屏可以使用不同的 Panel 驱动和总线接口，独立显示不同的内容。

### 示例配置

- **Display0**: ST7789P3 面板 + SPI 4-Wire 总线（240x240）
- **Display1**: AXS15231B 面板 + QSPI 总线（176x560）

### 核心特性

- **多 Panel 驱动**: 同时启用多个 Panel 驱动（ST7789P3 + AXS15231B）
- **多总线类型**: 支持不同总线接口（SPI 4-Wire + QSPI）
- **独立控制**: 每个显示屏独立配置亮度、颜色、刷新频率
- **异步显示**: 两个屏幕显示不同内容，互不干扰
- **线程安全**: 驱动内部使用双层锁机制，支持多线程并发访问
- **智能锁管理**: 自动识别共享总线并管理锁资源，无需用户干预

## 硬件连接

### Display0: ST7789P3 (SPI 4-Wire)

| 信号 | ARCS-D 引脚 | 说明 |
|------|-------------|------|
| VCC | 3.3V | 电源 |
| GND | GND | 地 |
| CS | GPIOB_5 | 片选 |
| DC | GPIOB_0 | 数据/命令选择 |
| RESET | GPIOA_1 | 复位 |
| SCK | GPIOB_3 (SPI1_CLK) | SPI 时钟 |
| MOSI | GPIOB_1 (SPI1_MOSI) | SPI 数据 |
| BL | PWM0_CH0 (GPIOA_0) | 背光 PWM |

**注意**: 此配置基于 ARCS EVB 板型，对应代码中的宏定义：
- `LCD0_RST_PIN = 1` (GPIOA)
- `LCD0_CS_PIN = 5` (GPIOB)
- `LCD0_DC_PIN = 0` (GPIOB)
- `LCD0_SPI_CLK_PIN = 3` (GPIOB, 复用为 SPI1_CLK)
- `LCD0_SPI_DATA_PIN = 1` (GPIOB, 复用为 SPI1_MOSI)
- `LCD0_BL_PWM_CHANNEL = 0`

### Display1: AXS15231B (QSPI)

| 信号 | ARCS-D 引脚 | 说明 |
|------|-------------|------|
| VCC | 3.3V | 电源 |
| GND | GND | 地 |
| CS | GPIOA_19 | 片选 |
| RESET | GPIOA_2 | 复位 |
| QSPI_CLK | QSPI_CLK | QSPI 时钟 |
| QSPI_D0 | QSPI_D0 | QSPI 数据 0 |
| QSPI_D1 | QSPI_D1 | QSPI 数据 1 |
| QSPI_D2 | QSPI_D2 | QSPI 数据 2 |
| QSPI_D3 | QSPI_D3 | QSPI 数据 3 |
| BL | PWM0_CH1 (GPIOA_x) | 背光 PWM |

**注意**: 此配置基于 ARCS EVB 板型，对应代码中的宏定义：
- `LCD1_RST_PIN = 2` (GPIOA)
- `LCD1_CS_PIN = 19` (GPIOA)
- `LCD1_BL_PWM_CHANNEL = 1`

## Kconfig 配置

在 `prj.conf` 中的关键配置：

```kconfig
# 启用双显示屏支持
CONFIG_LISA_DUAL_DISPLAY=y

# Display 驱动
CONFIG_LISA_DISPLAY_DEVICE=y

# Panel 驱动（支持多选）
CONFIG_LISA_DISPLAY_PANEL_ST7789P3=y
CONFIG_PANEL_ST7789P3_WIDTH=240
CONFIG_PANEL_ST7789P3_HEIGHT=320

CONFIG_LISA_DISPLAY_PANEL_AXS15231B=y
CONFIG_PANEL_AXS15231B_WIDTH=176
CONFIG_PANEL_AXS15231B_HEIGHT=560

# 总线驱动
CONFIG_LISA_DISPLAY_BUS_SPI_4WIRE=y
CONFIG_LISA_DISPLAY_BUS_QSPI=y

# 背光控制
CONFIG_LISA_DISPLAY_BACKLIGHT_PWM=y

# PSRAM 配置（双屏需要更大内存）
CONFIG_PSRAM_HEAP_SIZE=0x200000  # 2MB
```

## 示例流程

1. **初始化 Display0 (ST7789P3)**
   - 获取 display 设备和相关外设（GPIO、SPI、PWM）
   - 配置 SPI 4-Wire 总线参数
   - 设置 Panel 设备名称为 "st7789p3"
   - 分配帧缓冲区
   - 设置亮度为 80%

2. **初始化 Display1 (AXS15231B)**
   - 获取 display1 设备和相关外设（GPIO、QSPI、PWM）
   - 配置 QSPI 总线参数
   - 设置 Panel 设备名称为 "axs15231b"
   - 分配帧缓冲区
   - 设置亮度为 60%

3. **循环显示**
   - Display0 和 Display1 显示不同颜色
   - 每 2 秒切换到下一个颜色
   - 颜色序列：红 → 绿 → 蓝 → 黄 → 青 → 品红 → 白

## 编译

```bash
# 使用提供的脚本
./build.sh

# 或者手动编译
lisa zep build -b csk6011a_nano -B build
```

```{eval-rst}
.. include:: /sample_build.rst
```

## 预期输出

**终端输出：**

```
Dual Display sample started
Initializing Display0 (ST7789P3)...
Display0 initialized: 240x240
Initializing Display1 (AXS15231B)...
Display1 initialized: 176x560
Both displays initialized successfully
Display0: color index 0
Display1: color index 3
Display0: color index 1
Display1: color index 4
...
```

**显示效果：**

- **Display0 (240x240)**: 以 80% 亮度显示，颜色从红色开始循环
- **Display1 (176x560)**: 以 60% 亮度显示，颜色从黄色开始循环
- 两个屏幕同步切换，但显示不同颜色

## 核心代码

### Display0 配置

```c
lisa_display_config_t config0 = {
    .panel_name = "st7789p3",  // Panel 设备名称
    .bus_type = LISA_DISPLAY_BUS_SPI_4WIRE,
    .bus_instance = 0,
    .bus_config = {
        .spi_4wire = {
            .spi_dev = spi1,
            .cs_gpio = gpiob,
            .cs_pin = LCD0_CS_PIN,
            .dc_gpio = gpiob,
            .dc_pin = LCD0_DC_PIN,
            .spi_freq = 50 * 1000 * 1000,  // 50MHz
        }
    },
    .backlight = {
        .type = LISA_DISPLAY_BACKLIGHT_TYPE_PWM,
        .config.pwm = {
            .dev = pwm0,
            .channel = 0,  // PWM 通道 0
            .freq = 2000,
        }
    },
    .rst_gpio = gpioa,
    .rst_pin = LCD0_RST_PIN,
};
```

### Display1 配置

```c
lisa_display_config_t config1 = {
    .panel_name = "axs15231b",  // Panel 设备名称
    .bus_type = LISA_DISPLAY_BUS_QSPI,
    .bus_instance = 0,
    .bus_config = {
        .qspi = {
            .qspi_dev = qspi,
            .qspi_freq = 50 * 1000 * 1000,  // 50MHz
            .cs_gpio = gpioa,
            .cs_pin = LCD1_CS_PIN,
        }
    },
    .backlight = {
        .type = LISA_DISPLAY_BACKLIGHT_TYPE_PWM,
        .config.pwm = {
            .dev = pwm0,
            .channel = 1,  // PWM 通道 1（不同于 Display0）
            .freq = 2000,
        }
    },
    .rst_gpio = gpioa,
    .rst_pin = LCD1_RST_PIN,
};
```

### 关键配置差异

| 配置项 | Display0 | Display1 |
|--------|----------|----------|
| Panel 设备名称 | "st7789p3" | "axs15231b" |
| 总线类型 | SPI 4-Wire | QSPI |
| 设备名称 | display | display1 |
| PWM 通道 | 0 | 1 |
| 复位引脚 | GPIOA_1 | GPIOA_2 |

## 核心 API

| API | 说明 |
|-----|------|
| `lisa_device_get("display")` | 获取 Display0 设备实例 |
| `lisa_device_get("display1")` | 获取 Display1 设备实例 |
| `lisa_display_attach_bus()` | 关联总线配置到显示设备 |
| `lisa_display_get_capabilities()` | 获取显示屏分辨率等参数 |
| `lisa_display_blanking_off()` | 开启显示 |
| `lisa_display_write()` | 写入帧数据到显示屏 |
| `lisa_display_set_brightness()` | 设置背光亮度（0-100） |

## 故障排查

### 问题 1: Display0 或 Display1 初始化失败

**可能原因：**
- Panel 驱动未启用（检查 Kconfig）
- 设备名称配置错误（panel_name 与驱动注册名称不一致）
- 总线设备未找到（检查设备注册）

**解决方法：**
```c
// 检查设备是否存在
lisa_device_t *dev = lisa_device_get("display");
if (!dev) {
    LISA_LOGE(LOG_TAG, "display not found!");
}

// 检查 Panel 设备是否存在
lisa_device_t *panel = lisa_device_get("st7789p3");
if (!panel) {
    LISA_LOGE(LOG_TAG, "st7789p3 panel not found!");
}
```

### 问题 2: 屏幕无显示

**可能原因：**
- 硬件连接错误
- 引脚配置不正确（检查 pinmux）
- 背光未开启或 PWM 通道冲突

**解决方法：**
- 检查硬件连线，确保电源、地、信号线正确
- 确认 `lisa_display_blanking_off()` 被调用
- 检查两个显示屏使用不同的 PWM 通道
- 验证背光 PWM 配置正确

### 问题 3: 内存分配失败

**可能原因：**
- PSRAM 配置不足（双屏需要更多内存）

**解决方法：**
- 增加 `CONFIG_PSRAM_HEAP_SIZE`（建议至少 2MB）
- 检查帧缓冲区大小计算是否正确

### 问题 4: 颜色显示异常

**可能原因：**
- 需要启用颜色反转
- 像素格式配置错误

**解决方法：**
- 尝试开启/关闭 `CONFIG_LISA_DISPLAY_COLOR_INVERT`
- 检查 Panel 配置中的像素格式设置

## 线程安全特性

本示例演示的多屏异显功能具备完整的线程安全保护：

### 1. **双层锁机制**

驱动内部使用两层互斥锁保护：

- **总线级锁（Bus Mutex）**: 保护硬件总线访问
  - 如果两个 Display 使用相同的总线设备（如共享 SPI），会自动共享同一个锁
  - 如果使用独立总线（如本示例：Display0 用 SPI，Display1 用 QSPI），则各自使用独立的锁
  - **零配置**：驱动自动判断并管理，无需用户干预

- **资源级锁（Rotate Mutex）**: 保护全局旋转缓冲区
  - 所有使用 90°/270° 旋转的 Display 共享全局旋转资源
  - 自动互斥访问，防止数据竞争

### 2. **SPI 一主多从场景**

如果两个显示屏共享同一个 SPI 总线（只是 CS 引脚不同）：

```c
// 两个屏幕都使用 spi1，但 CS 引脚不同
lisa_display_config_t config0 = {
    .panel_name = "st7789p3",
    .bus_type = LISA_DISPLAY_BUS_SPI_4WIRE,
    .bus_config.spi_4wire = {
        .spi_dev = spi1,  // 相同的 SPI 设备
        .cs_pin = 1,      // 不同的 CS 引脚
        // ...
    },
};

lisa_display_config_t config1 = {
    .panel_name = "axs15231b",
    .bus_type = LISA_DISPLAY_BUS_SPI_4WIRE,
    .bus_config.spi_4wire = {
        .spi_dev = spi1,  // 相同的 SPI 设备
        .cs_pin = 2,      // 不同的 CS 引脚
        // ...
    },
};
```

**驱动会自动**：
- 检测到两个 Display 使用相同的 `bus_dev` 指针
- 共享同一个总线锁，确保不会同时访问
- 无需用户添加任何同步代码

### 3. **多线程使用示例**

```c
// 线程 1：更新 Display0
void thread1(void *arg)
{
    while (1) {
        // 安全调用，驱动内部有锁保护
        lisa_display_write(display0, 0, 0, &desc0, buffer0);
        lisa_thread_mdelay(100);
    }
}

// 线程 2：更新 Display1
void thread2(void *arg)
{
    while (1) {
        // 安全调用，即使与线程 1 并发执行也不会冲突
        lisa_display_write(display1, 0, 0, &desc1, buffer1);
        lisa_thread_mdelay(100);
    }
}
```

**线程安全保证**：
- 每个 Display 设备有独立的设备级锁
- 共享总线的 Display 会互斥访问总线
- 使用旋转功能时会互斥访问全局旋转缓冲区
- 所有错误路径都正确释放锁，不会死锁

## 注意事项

1. **设备名称**: `panel_name` 必须与 Panel 驱动注册时使用的设备名称一致
   - ST7789P3 驱动注册为 "st7789p3"
   - AXS15231B 驱动注册为 "axs15231b"

2. **资源独立性**: 确保两个显示屏使用不同的资源
   - 不同的 PWM 通道（避免冲突）
   - 不同的 GPIO 引脚（RST、CS、DC）
   - 独立的帧缓冲区

3. **内存管理**: 双屏需要分配两个帧缓冲区，确保 PSRAM 足够大
   - Display0: 240×240×2 = 115,200 字节
   - Display1: 176×560×2 = 197,120 字节
   - 总计约 312KB，建议配置 2MB PSRAM

4. **总线频率**: 根据实际硬件调整 SPI/QSPI 频率，过高可能导致显示异常

5. **引脚复用**: 示例中重写了 pinmux 函数，确保引脚配置符合硬件设计

6. **线程安全**: 驱动完全线程安全，可以在多个线程中同时操作不同的 Display
   - 共享总线时会自动互斥，保证总线访问安全
   - 使用旋转功能时会自动互斥全局旋转缓冲区
   - 所有同步机制对用户透明，无需额外代码

## 扩展应用

### 显示不同内容

```c
// Display0 显示图像
void display0_show_image(const uint16_t *image_data, uint16_t width, uint16_t height)
{
    lisa_display_buffer_desc_t desc = {
        .width = width,
        .height = height,
        .buf_size = width * height * sizeof(uint16_t),
    };
    lisa_display_write(display0_ctx.device, 0, 0, &desc, image_data);
}

// Display1 显示文本（需配合 GUI 库）
void display1_show_text(const char *text)
{
    // 使用 LVGL 或其他 GUI 库渲染文本到 framebuffer
    // ...
    
    lisa_display_buffer_desc_t desc = {
        .width = display1_ctx.width,
        .height = display1_ctx.height,
        .buf_size = display1_ctx.width * display1_ctx.height * sizeof(uint16_t),
    };
    lisa_display_write(display1_ctx.device, 0, 0, &desc, display1_ctx.framebuffer);
}
```

### 动态调整亮度

```c
// 根据环境光调整亮度
void adjust_brightness(uint8_t brightness0, uint8_t brightness1)
{
    lisa_display_set_brightness(display0_ctx.device, brightness0);
    lisa_display_set_brightness(display1_ctx.device, brightness1);
}
```

## 文件说明

- `src/main.c` - 示例主程序
- `prj.conf` - Kconfig 配置
- `CMakeLists.txt` - 构建配置
- `sample.yaml` - 测试配置
- `build.sh` - 构建脚本
- `README.md` - 本文档

## 性能优化

### 1. **无锁开销**
- 本示例中 Display0 和 Display1 使用不同总线（SPI vs QSPI）
- 两个 Display 各自使用独立的总线锁，互不影响
- 无锁竞争，无性能损失

### 2. **CPU/DMA 并行**
- 旋转操作使用 Ping-Pong 缓冲
- CPU 旋转下一块数据时，DMA 传输当前数据
- 最大化硬件利用率

### 3. **CPDMA 加速**（可选）
```kconfig
# 启用硬件 DMA 加速旋转
CONFIG_LISA_DISPLAY_CPDMA_ROTATE=y
CONFIG_LISA_DISPLAY_CPDMA_CH=2
```

## 相关文档

- [LISA Display 驱动文档](../../../../drivers/lisa_display/README.md)
- [线程安全与锁机制说明](../../../../drivers/lisa_display/README.md#线程安全与锁机制)