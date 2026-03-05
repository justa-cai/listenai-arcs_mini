# Camera 驱动

基于 lisa_device 框架的摄像头设备驱动，为 ARCS 平台提供统一的图像采集接口。

## 功能特性

- **传感器支持**: GC032A、GC0328、BF3901 等多种图像传感器自动探测
- **总线接口**: 支持 DVP (并行) 和 SPI (串行) 两种数据总线
- **像素格式**: 支持 RGB565、RGB888、YUV422、YUV420、灰度、JPEG、RAW 格式
- **帧缓冲管理**: 多缓冲队列机制，支持异步捕获和回调通知
- **图像控制**: 支持水平镜像、垂直翻转、裁剪窗口设置
- **线程安全**: 使用队列机制管理帧缓冲区，可在多线程环境中使用

## 配置选项

在 `prj.conf` 中启用驱动：

```kconfig
CONFIG_LISA_CAMERA_DEVICE=y          # 启用 Camera 驱动

# 选择总线类型 (二选一)
CONFIG_LISA_CAMERA_BUS_DVP=y         # 使用 DVP 并行总线
# CONFIG_LISA_CAMERA_BUS_SPI=y       # 使用 SPI 串行总线

# 选择传感器 (可多选)
CONFIG_LISA_CAMERA_SENSOR_GC032A=y   # 启用 GC032A 传感器
CONFIG_LISA_CAMERA_SENSOR_GC0328=y   # 启用 GC0328 传感器
CONFIG_LISA_CAMERA_SENSOR_BF3901=y   # 启用 BF3901 传感器
```

## API 接口

### 初始化与配置接口

```c
int lisa_camera_setup(lisa_device_t *dev, const lisa_camera_config_t *config);
int lisa_camera_attach_bus(lisa_device_t *dev, const lisa_camera_bus_config_t *bus_config);
int lisa_camera_get_capabilities(lisa_device_t *dev, lisa_camera_capabilities_t *caps);
```

**重要**: 必须按顺序调用 `lisa_camera_setup()` 和 `lisa_camera_attach_bus()` 后，才能启动摄像头。

### 捕获控制接口

```c
int lisa_camera_start(lisa_device_t *dev);
int lisa_camera_stop(lisa_device_t *dev);
int lisa_camera_capture(lisa_device_t *dev, lisa_camera_fb_t **fb);
int lisa_camera_release_fb(lisa_device_t *dev, lisa_camera_fb_t *fb);
```

### 参数设置接口

```c
int lisa_camera_set_pixformat(lisa_device_t *dev, lisa_camera_pixel_format_t format);
int lisa_camera_set_hmirror(lisa_device_t *dev, bool enable);
int lisa_camera_set_vflip(lisa_device_t *dev, bool enable);
int lisa_camera_set_crop(lisa_device_t *dev, const lisa_camera_crop_t *crop);
int lisa_camera_set_callback(lisa_device_t *dev, lisa_camera_frame_callback_t callback, void *user_data);
```

### 状态查询接口

```c
int lisa_camera_get_framesize(lisa_device_t *dev, uint16_t *width, uint16_t *height);
lisa_camera_pixel_format_t lisa_camera_get_pixformat(lisa_device_t *dev);
```

## 使用示例

### 基础图像捕获

```c
#include "lisa_device.h"
#include "lisa_camera.h"

int camera_example(void)
{
    int ret;

    // 1. 获取设备
    lisa_device_t *camera_dev = lisa_device_get("camera");
    if (!lisa_device_ready(camera_dev)) {
        return -1;
    }

    lisa_device_t *i2c_dev = lisa_device_get("i2c1");

    // 2. 配置摄像头
    lisa_camera_config_t config = {
        .hw_config = {
            .mclk_pad = CSK_IOMUX_PAD_A,
            .mclk_pin = 26,
            .pwdn_gpio_dev = lisa_device_get("gpiob"),
            .pwdn_pin = 9,
            .pwdn_delay_us = 1000,
            .xclk_delay_us = 1000,
            .i2c_dev = i2c_dev,
        },
        .xclk_freq_hz = 18000000,  // 18MHz 外部时钟
        .fb_count = 3,             // 3 个帧缓冲区
        .enable_hmirror = false,
        .enable_vflip = false,
    };

    ret = lisa_camera_setup(camera_dev, &config);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    // 3. 配置 SPI 总线
    lisa_camera_bus_config_t bus_config = {
        .bus_type = LISA_CAMERA_BUS_SPI,
        .dma_channel = 2,
        .config.spi = {
            .spi_dev = lisa_device_get("spi0"),
            .cs_gpio = lisa_device_get("gpioa"),
            .cs_pin = 22,
            .spi_bit_order = 1,  // LSB first
            .spi_mode = 1,       // CPOL=0, CPHA=1
        }
    };
    lisa_camera_get_framesize(camera_dev, &bus_config.width, &bus_config.height);
    bus_config.pixel_format = lisa_camera_get_pixformat(camera_dev);

    ret = lisa_camera_attach_bus(camera_dev, &bus_config);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    // 4. 启动摄像头
    ret = lisa_camera_start(camera_dev);
    if (ret != LISA_DEVICE_OK) {
        return ret;
    }

    // 5. 捕获图像
    lisa_camera_fb_t *fb = NULL;
    ret = lisa_camera_capture(camera_dev, &fb);
    if (ret == LISA_DEVICE_OK && fb != NULL) {
        // 处理图像数据
        // fb->buf: 图像数据指针
        // fb->len: 数据长度
        // fb->width, fb->height: 分辨率

        // 6. 释放帧缓冲区
        lisa_camera_release_fb(camera_dev, fb);
    }

    // 7. 停止摄像头
    lisa_camera_stop(camera_dev);

    return 0;
}
```

### DVP 总线配置

```c
// DVP 并行总线配置
lisa_camera_bus_config_t bus_config = {
    .bus_type = LISA_CAMERA_BUS_DVP,
    .dma_channel = 2,
    .config.dvp = {
        .dvp_dev = lisa_device_get("dvp0"),
        .data_align = 1,        // 左对齐 [bit11~4]
        .line_offset = 0,
        .pixel_offset = 0,
        .pclk_polarity = 0,     // 下降沿采样
        .vsync_polarity = 1,    // 高电平有效
        .hsync_polarity = 1,    // 高电平有效
    }
};

lisa_camera_get_framesize(camera_dev, &bus_config.width, &bus_config.height);
bus_config.pixel_format = lisa_camera_get_pixformat(camera_dev);

lisa_camera_attach_bus(camera_dev, &bus_config);
```

### 连续捕获回调模式

```c
// 帧完成回调函数
void frame_callback(const lisa_camera_fb_t *fb, void *user_data)
{
    lisa_device_t *dev = (lisa_device_t *)user_data;

    // 处理帧数据 (在 ISR 上下文中，应尽快返回)
    process_frame(fb);
}

// 设置回调
lisa_camera_set_callback(camera_dev, frame_callback, camera_dev);

// 启动后，每帧完成时自动调用回调
lisa_camera_start(camera_dev);
```

## 关键数据结构

### lisa_camera_config_t

摄像头配置结构体：

| 字段 | 类型 | 说明 |
|------|------|------|
| `hw_config` | `lisa_camera_hw_config_t` | 硬件配置 (引脚、I2C 等) |
| `xclk_freq_hz` | `uint32_t` | 外部时钟频率 (Hz) |
| `fb_count` | `uint8_t` | 帧缓冲区数量 (建议 2-3) |
| `enable_hmirror` | `bool` | 水平镜像开关 |
| `enable_vflip` | `bool` | 垂直翻转开关 |

### lisa_camera_fb_t

帧缓冲区结构体：

| 字段 | 类型 | 说明 |
|------|------|------|
| `buf` | `uint8_t *` | 图像数据指针 |
| `len` | `uint32_t` | 数据长度 (字节) |
| `width` | `uint16_t` | 图像宽度 |
| `height` | `uint16_t` | 图像高度 |
| `format` | `lisa_camera_pixel_format_t` | 像素格式 |
| `timestamp` | `uint32_t` | 时间戳 (毫秒) |

## 像素格式说明

| 枚举值 | 格式 | 字节/像素 | 说明 |
|-------|------|----------|------|
| `LISA_CAMERA_PIXFMT_RGB565` | RGB565 | 2 | 常用 LCD 显示格式 |
| `LISA_CAMERA_PIXFMT_RGB888` | RGB888 | 3 | 真彩色 |
| `LISA_CAMERA_PIXFMT_YUV422` | YUV422 | 2 | 视频编码常用 |
| `LISA_CAMERA_PIXFMT_YUV420` | YUV420 | 1.5 | 视频编码常用 |
| `LISA_CAMERA_PIXFMT_GRAY` | 灰度 | 1 | 单通道灰度图 |
| `LISA_CAMERA_PIXFMT_JPEG` | JPEG | 可变 | 硬件压缩格式 |
| `LISA_CAMERA_PIXFMT_RAW` | RAW | 可变 | Bayer 原始数据 |

## 硬件配置

### 引脚复用配置

Camera 驱动需要在板型目录中配置相关引脚复用函数。

**配置位置**:
- **定义**: `boards/<板型名>/pinmux.c` 中实现函数
- **声明**: `boards/<板型名>/pinmux.h` 中声明函数
- **调用时机**: 设备初始化时自动调用

**SPI 总线引脚示例** (参考 `boards/arcs_evb/pinmux.c`):

```c
#define CAM_SPI_CS_PIN   22
#define CAM_SPI_MOSI_PIN 24
#define CAM_SPI_SCK_PIN  25

void lisa_spi0_pinmux(void)
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_SPI_SCK_PIN, CSK_IOMUX_FUNC_ALTER5);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_SPI_MOSI_PIN, CSK_IOMUX_FUNC_ALTER5);
}

void lisa_gpioa_pinmux(void)
{
    // CS 引脚配置为 GPIO
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_SPI_CS_PIN, CSK_IOMUX_FUNC_DEFAULT);
}
```

**DVP 总线引脚示例**:

```c
#define CAM_HSYNC_PIN   10
#define CAM_VSYNC_PIN   11
#define CAM_PCLK_PIN    12
#define CAM_D0_PIN      13
// ... D1-D7

void lisa_dvp_pinmux(void)
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_HSYNC_PIN, CSK_IOMUX_FUNC_ALTER16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_VSYNC_PIN, CSK_IOMUX_FUNC_ALTER16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_PCLK_PIN, CSK_IOMUX_FUNC_ALTER16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CAM_D0_PIN, CSK_IOMUX_FUNC_ALTER16);
    // ... 配置其他数据引脚
}
```

### I2C 配置

传感器通过 I2C (SCCB 协议) 进行寄存器配置：

```c
void lisa_i2c1_pinmux(void)
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 0, 8);  // SDA
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 1, 8);  // SCL
}
```

### 控制引脚

| 引脚 | 功能 | 说明 |
|-----|------|------|
| PWDN | Power Down | 低电平工作，高电平休眠 |
| MCLK | 外部时钟 | DVP 控制器输出，驱动传感器 |

## 注意事项

1. **初始化顺序**: 必须按 `setup` → `attach_bus` → `start` 顺序调用
2. **帧缓冲区释放**: 每次 `lisa_camera_capture()` 成功后，必须调用 `lisa_camera_release_fb()` 释放帧缓冲区
3. **缓冲区数量**: `fb_count` 建议设置为 2-3，过多会占用大量内存
4. **内存对齐**: 帧缓冲区内部按 32 字节对齐分配，支持 DMA 传输
5. **Cache 一致性**: 驱动内部会调用 `HAL_InvalidateDCache_by_Addr()` 刷新 Cache
6. **回调上下文**: `lisa_camera_set_callback()` 设置的回调函数在 ISR 上下文中执行，应尽快返回
7. **捕获超时**: `lisa_camera_capture()` 阻塞等待最多 1000ms，超时返回 `LISA_DEVICE_ERR_TIMEOUT`
8. **总线选择**: DVP 适合高分辨率/高帧率 (占用 10+ GPIO)，SPI 适合低分辨率 (占用 4 GPIO)
9. **传感器探测**: 驱动会自动遍历所有已启用的传感器进行探测，确保 I2C 配置正确
10. **时钟频率**: `xclk_freq_hz` 需根据传感器规格设置，常用值为 18MHz 或 24MHz

## 传感器移植适配

### 移植步骤

移植新传感器需要完成以下 5 个步骤：

#### 1. 添加传感器信息定义

在 `sensors/sensor.h` 中添加传感器枚举和地址定义：

```c
// 在 camera_model_t 枚举中添加
typedef enum {
    // ... 已有传感器
    CAMERA_XXXX,       // 新增传感器型号
    CAMERA_MODEL_MAX,
} camera_model_t;

// 在 camera_sccb_addr_t 枚举中添加 I2C 地址
typedef enum {
    // ... 已有地址
    XXXX_SCCB_ADDR = 0x21,  // 新传感器 I2C 地址 (7位)
} camera_sccb_addr_t;

// 在 camera_pid_t 枚举中添加 PID
typedef enum {
    // ... 已有 PID
    XXXX_PID = 0x1234,      // 新传感器 Product ID
} camera_pid_t;
```

#### 2. 注册传感器信息表

在 `sensors/sensor.c` 的 `camera_sensor[]` 数组中添加传感器信息：

```c
const camera_sensor_info_t camera_sensor[CAMERA_MODEL_MAX] = {
    // ... 已有传感器
    {CAMERA_XXXX, "XXXX", XXXX_SCCB_ADDR, XXXX_PID, 640, 480, 
     (PIXFORMAT_MASK_RGB565 | PIXFORMAT_MASK_YUV422)},
};
```

#### 3. 实现传感器驱动

创建 `sensors/src/xxxx.c` 和 `sensors/inc/xxxx.h` 文件：

**头文件** (`sensors/inc/xxxx.h`):

```c
#ifndef __XXXX_H__
#define __XXXX_H__

#include "sensor.h"

/**
 * @brief 探测传感器
 * @param slv_addr I2C 地址
 * @param id 输出传感器 ID
 * @return 非零表示探测成功，0 表示失败
 */
int xxxx_detect(int slv_addr, sensor_id_t *id);

/**
 * @brief 初始化传感器函数指针
 * @param sensor 传感器结构体指针
 * @return 0
 */
int xxxx_init(sensor_t *sensor);

#endif
```

**源文件** (`sensors/src/xxxx.c`):

```c
#include "sensor.h"
#include "xxxx.h"
#include "xxxx_regs.h"      // 寄存器定义
#include "xxxx_settings.h"  // 初始化寄存器表

// 探测函数：读取 PID 寄存器验证传感器
int xxxx_detect(int slv_addr, sensor_id_t *id)
{
    if (XXXX_SCCB_ADDR == slv_addr) {
        uint8_t pid_h = CAMERA_READ_REG8(slv_addr, PID_REG_H);
        uint8_t pid_l = CAMERA_READ_REG8(slv_addr, PID_REG_L);
        uint16_t pid = (pid_h << 8) | pid_l;
        
        if (XXXX_PID == pid) {
            id->PID = pid;
            return pid;  // 返回非零表示成功
        }
    }
    return 0;  // 探测失败
}

// 复位函数：软复位并写入初始化寄存器
static int reset(sensor_t *sensor)
{
    // 1. 软复位
    CAMERA_WRITE_REG8(sensor->slv_addr, RESET_REG, 0x01);
    CAMERA_DELAY_MS(100);
    
    // 2. 写入初始化寄存器表
    // write_regs(sensor->slv_addr, xxxx_init_regs);
    
    return 0;
}

// 设置像素格式
static int set_pixformat(sensor_t *sensor, pixformat_t pixformat)
{
    switch (pixformat) {
    case PIXFORMAT_RGB565:
        // 配置 RGB565 输出
        break;
    case PIXFORMAT_YUV422:
        // 配置 YUV422 输出
        break;
    default:
        return -1;
    }
    sensor->pixformat = pixformat;
    return 0;
}

// 获取当前像素格式
static pixformat_t get_pixformat(sensor_t *sensor)
{
    return sensor->pixformat;
}

// 设置窗口 (分辨率/裁剪)
static int set_window(sensor_t *sensor, int16_t x, int16_t y, uint16_t w, uint16_t h)
{
    // 配置输出窗口寄存器
    return 0;
}

// 获取当前窗口大小
static int get_window(sensor_t *sensor, uint16_t *w, uint16_t *h)
{
    *w = 640;  // 读取或返回默认值
    *h = 480;
    return 0;
}

// 设置水平镜像
static int set_hmirror(sensor_t *sensor, int enable)
{
    // 配置镜像寄存器
    return 0;
}

// 设置垂直翻转
static int set_vflip(sensor_t *sensor, int enable)
{
    // 配置翻转寄存器
    return 0;
}

// 初始化函数：填充传感器函数指针表
int xxxx_init(sensor_t *sensor)
{
    // 必须实现的函数
    sensor->reset = reset;
    sensor->set_pixformat = set_pixformat;
    sensor->get_pixformat = get_pixformat;
    sensor->set_window = set_window;
    sensor->get_window = get_window;
    sensor->set_hmirror = set_hmirror;
    sensor->set_vflip = set_vflip;
    
    // 可选函数 (不支持则设为 NULL 或 dummy)
    sensor->set_brightness = NULL;
    sensor->set_contrast = NULL;
    sensor->set_saturation = NULL;
    // ... 其他可选函数
    
    return 0;
}
```

#### 4. 添加 Kconfig 配置

在 `Kconfig` 中添加传感器选项：

```kconfig
config LISA_CAMERA_SENSOR_XXXX
    bool "XXXX"
    default n
    help
        Enable XXXX camera sensor support.
```

#### 5. 注册到驱动

在 `lisa_camera_arcs.c` 中添加传感器引用：

```c
// 头文件引用
#if CONFIG_LISA_CAMERA_SENSOR_XXXX
#include "xxxx.h"
#endif

// camera_sensors 数组中添加
static const sensor_func_t camera_sensors[] = {
    // ... 已有传感器
#if CONFIG_LISA_CAMERA_SENSOR_XXXX
    {CAMERA_XXXX, xxxx_detect, xxxx_init},
#endif
};
```

### sensor_t 函数指针说明

| 函数指针 | 必需 | 说明 |
|---------|------|------|
| `reset` | **是** | 软复位并加载初始化配置 |
| `set_pixformat` | **是** | 设置像素格式 (RGB565/YUV422 等) |
| `get_pixformat` | **是** | 获取当前像素格式 |
| `set_window` | **是** | 设置输出窗口大小 |
| `get_window` | **是** | 获取当前窗口大小 |
| `set_hmirror` | 推荐 | 水平镜像控制 |
| `set_vflip` | 推荐 | 垂直翻转控制 |
| `set_colorbar` | 可选 | 测试模式 (彩条) |
| `set_brightness` | 可选 | 亮度调节 |
| `set_contrast` | 可选 | 对比度调节 |
| `set_saturation` | 可选 | 饱和度调节 |
| `set_gainceiling` | 可选 | 增益上限设置 |

### I2C 寄存器读写宏

驱动提供统一的寄存器操作宏：

```c
// 8 位寄存器地址
CAMERA_READ_REG8(addr, reg)           // 读取寄存器
CAMERA_WRITE_REG8(addr, reg, value)   // 写入寄存器

// 16 位寄存器地址 (如 OV5640)
CAMERA_READ_REG16(addr, reg)
CAMERA_WRITE_REG16(addr, reg, value)

// 延时
CAMERA_DELAY_MS(ms)

// 日志
CAMERA_LOGI("info message");
CAMERA_LOGE("error message");
```

### 移植验证

1. 在 `prj.conf` 中启用新传感器：`CONFIG_LISA_CAMERA_SENSOR_XXXX=y`
2. 编译并烧录固件
3. 检查日志输出 `Sensor detected: PID=0xXXXX`
4. 测试图像捕获功能

## 文件说明

- `lisa_camera.h` - 公共 API 头文件
- `lisa_camera_arcs.c` - ARCS 平台驱动实现
- `Kconfig` - 配置选项
- `bus/lisa_camera_bus.h` - 总线接口定义
- `bus/lisa_camera_bus_dvp.c` - DVP 总线实现
- `bus/lisa_camera_bus_spi.c` - SPI 总线实现
- `sensors/` - 传感器驱动目录 (gc032a.c, gc0328.c, bf3901.c 等)
