# DVP 驱动

基于 lisa_device 框架的 DVP（Digital Video Port）设备驱动，为 ARCS 平台提供统一的数字视频接口，主要用于连接摄像头等图像采集设备。

## 功能特性

- **设备支持**: 支持 DVP0 设备
- **传输模式**: 支持普通模式和 Ping-Pong 双缓冲模式
- **数据格式**: 支持 YUV422 和 8 位亮度（Lumina）输入格式
- **DMA 传输**: 集成 GPDMA 实现高效的数据传输
- **时钟输出**: 支持可配置的主时钟输出（MCLK），用于驱动摄像头
- **缓冲区重载**: 支持运行时动态重载缓冲区，实现连续捕获
- **线程安全**: 内部使用互斥锁保护，可在多线程环境中使用

## 配置选项

在 `prj.conf` 中启用驱动：

```kconfig
CONFIG_LISA_DVP_DEVICE=y
```

## API 接口

### 配置接口

```c
int lisa_dvp_setup(lisa_device_t *dev, const lisa_dvp_config_t *config,
                   lisa_dvp_callback_t callback, void *user_data);
```

**重要**: 必须先调用 `lisa_dvp_setup()` 配置设备后，才能使用下面的功能 API。

### 控制接口

```c
int lisa_dvp_start(lisa_device_t *dev, void *buf, uint32_t len);
int lisa_dvp_start_pingpong(lisa_device_t *dev, void *ping_buf, void *pong_buf, uint32_t len);
int lisa_dvp_stop(lisa_device_t *dev);
```

### 缓冲区重载接口

```c
int lisa_dvp_reload(lisa_device_t *dev, void *buf, uint32_t len);
int lisa_dvp_reload_pingpong(lisa_device_t *dev, void *buf);
```

在回调函数中调用，用于重载下一帧的缓冲区地址。

### 时钟控制接口

```c
int lisa_dvp_enable_clockout(lisa_device_t *dev, uint32_t clock);
```

启用 DVP 时钟输出，为摄像头提供主时钟。

## 使用示例

### 普通模式捕获单帧

```c
#include "lisa_device.h"
#include "lisa_dvp.h"

// 定义接收缓冲区（需要足够大以容纳一帧数据）
static uint8_t frame_buffer[640 * 480 * 2] __attribute__((aligned(4)));

// DVP 事件回调函数
void dvp_callback(lisa_dvp_event_t event, void *user_data)
{
    if (event == LISA_DVP_EVENT_DONE) {
        // 一帧数据接收完成
        // 在此处理图像数据 frame_buffer
        // ...

        // 如需继续捕获下一帧，重载缓冲区
        lisa_device_t *dvp_dev = (lisa_device_t *)user_data;
        lisa_dvp_reload(dvp_dev, frame_buffer, sizeof(frame_buffer));
    }
}

int main(void)
{
    // 1. 获取 DVP 设备
    lisa_device_t *dvp_dev = lisa_device_get_by_name(LISA_DVP0_NAME);
    if (!dvp_dev) {
        return -1;
    }

    // 2. 配置 DVP 参数
    lisa_dvp_config_t config = {
        .dvp_hal_config = {
            .frame_width    = 640,
            .frame_height   = 480,
            .pixel_offset   = 0,
            .line_offset    = 0,
            .input_format   = LISA_DVP_INPUT_FORM_YUV422_Y0CBY1CR,
            .data_align     = LISA_DVP_DATA_ALIGN_LEFT,
            .vsync_polarity = LISA_DVP_POL_RISING,
            .hsync_polarity = LISA_DVP_POL_RISING,
            .pclk_polarity  = LISA_DVP_POL_RISING,
        },
        .gpdma_ch = 2,  // 使用 GPDMA 通道 2
    };

    // 3. 初始化 DVP 设备
    int ret = lisa_dvp_setup(dvp_dev, &config, dvp_callback, dvp_dev);
    if (ret != LISA_DEVICE_OK) {
        return -1;
    }

    // 4. 启用时钟输出（为摄像头提供 25MHz 时钟）
    ret = lisa_dvp_enable_clockout(dvp_dev, 25000000);
    if (ret != LISA_DEVICE_OK) {
        return -1;
    }

    // 5. 启动 DVP 捕获
    ret = lisa_dvp_start(dvp_dev, frame_buffer, sizeof(frame_buffer));
    if (ret != LISA_DEVICE_OK) {
        return -1;
    }

    // 等待捕获完成（在回调函数中处理）
    // ...

    return 0;
}
```

### Ping-Pong 模式连续捕获

```c
#include "lisa_device.h"
#include "lisa_dvp.h"

// 定义两个缓冲区用于 Ping-Pong 模式
static uint8_t ping_buffer[640 * 480 * 2] __attribute__((aligned(4)));
static uint8_t pong_buffer[640 * 480 * 2] __attribute__((aligned(4)));

// DVP 事件回调函数
void dvp_pingpong_callback(lisa_dvp_event_t event, void *user_data)
{
    lisa_device_t *dvp_dev = (lisa_device_t *)user_data;

    if (event == LISA_DVP_EVENT_PING_DONE) {
        // Ping 缓冲区接收完成，处理 ping_buffer 数据
        // ...

        // 重载 Ping 缓冲区用于下一次捕获
        lisa_dvp_reload_pingpong(dvp_dev, ping_buffer);

    } else if (event == LISA_DVP_EVENT_PONG_DONE) {
        // Pong 缓冲区接收完成，处理 pong_buffer 数据
        // ...

        // 重载 Pong 缓冲区用于下一次捕获
        lisa_dvp_reload_pingpong(dvp_dev, pong_buffer);
    }
}

int main(void)
{
    // 1. 获取 DVP 设备
    lisa_device_t *dvp_dev = lisa_device_get_by_name(LISA_DVP0_NAME);
    if (!dvp_dev) {
        return -1;
    }

    // 2. 配置 DVP 参数（同上）
    lisa_dvp_config_t config = {
        .dvp_hal_config = {
            .frame_width    = 640,
            .frame_height   = 480,
            .pixel_offset   = 0,
            .line_offset    = 0,
            .input_format   = LISA_DVP_INPUT_FORM_YUV422_Y0CBY1CR,
            .data_align     = LISA_DVP_DATA_ALIGN_LEFT,
            .vsync_polarity = LISA_DVP_POL_RISING,
            .hsync_polarity = LISA_DVP_POL_RISING,
            .pclk_polarity  = LISA_DVP_POL_RISING,
        },
        .gpdma_ch = 2,
    };

    // 3. 初始化 DVP 设备
    int ret = lisa_dvp_setup(dvp_dev, &config, dvp_pingpong_callback, dvp_dev);
    if (ret != LISA_DEVICE_OK) {
        return -1;
    }

    // 4. 启用时钟输出
    ret = lisa_dvp_enable_clockout(dvp_dev, 25000000);
    if (ret != LISA_DEVICE_OK) {
        return -1;
    }

    // 5. 启动 Ping-Pong 模式捕获
    ret = lisa_dvp_start_pingpong(dvp_dev, ping_buffer, pong_buffer, sizeof(ping_buffer));
    if (ret != LISA_DEVICE_OK) {
        return -1;
    }

    // Ping-Pong 模式会自动交替使用两个缓冲区
    // 在回调函数中处理并重载缓冲区即可实现连续捕获
    // ...

    return 0;
}
```

### 8 位灰度图像捕获

```c
#include "lisa_device.h"
#include "lisa_dvp.h"

// 灰度图像缓冲区（每像素 1 字节）
static uint8_t gray_buffer[640 * 480] __attribute__((aligned(4)));

void dvp_gray_callback(lisa_dvp_event_t event, void *user_data)
{
    if (event == LISA_DVP_EVENT_DONE) {
        // 灰度图像接收完成，处理 gray_buffer
        // ...
    }
}

int main(void)
{
    lisa_device_t *dvp_dev = lisa_device_get_by_name(LISA_DVP0_NAME);
    if (!dvp_dev) {
        return -1;
    }

    // 配置为 8 位亮度格式
    lisa_dvp_config_t config = {
        .dvp_hal_config = {
            .frame_width    = 640,
            .frame_height   = 480,
            .pixel_offset   = 0,
            .line_offset    = 0,
            .input_format   = LISA_DVP_INPUT_FORM_LUMINA_8BIT,  // 8 位灰度
            .data_align     = LISA_DVP_DATA_ALIGN_RIGHT,
            .vsync_polarity = LISA_DVP_POL_RISING,
            .hsync_polarity = LISA_DVP_POL_RISING,
            .pclk_polarity  = LISA_DVP_POL_FALLING,
        },
        .gpdma_ch = 2,
    };

    int ret = lisa_dvp_setup(dvp_dev, &config, dvp_gray_callback, dvp_dev);
    if (ret != LISA_DEVICE_OK) {
        return -1;
    }

    ret = lisa_dvp_enable_clockout(dvp_dev, 25000000);
    if (ret != LISA_DEVICE_OK) {
        return -1;
    }

    ret = lisa_dvp_start(dvp_dev, gray_buffer, sizeof(gray_buffer));
    if (ret != LISA_DEVICE_OK) {
        return -1;
    }

    return 0;
}
```

## 硬件配置

### 引脚复用配置

DVP 驱动在初始化时会自动调用板型目录中定义的 `lisa_dvp_pinmux()` 函数，用于配置引脚复用。

**配置位置**:
- **定义**: `boards/<板型名>/pinmux.c` 中实现函数
- **声明**: `boards/<板型名>/pinmux.h` 中声明函数
- **调用时机**: 设备初始化时自动调用

**示例** (参考 [boards/arcs_evb/pinmux.c:32-46](boards/arcs_evb/pinmux.c#L32-L46)):

```c
__attribute__((weak)) void lisa_dvp_pinmux()
{
    // HSYNC: PAD_A 10
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 10, 16);
    // VSYNC: PAD_A 11
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 11, 16);
    // PCLK: PAD_A 12
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 12, 16);
    // D0-D7: PAD_A 13-20
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 13, 16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 14, 16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 15, 16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 16, 16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 17, 16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 18, 16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 19, 16);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 20, 16);
    // MCLK: PAD_A 26
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 26, 16);
}
```

**引脚说明**:

| 信号 | PAD | PIN | 功能编号 | 说明 |
|-----|-----|-----|---------|------|
| HSYNC | PAD_A | 10 | 16 | 水平同步信号 |
| VSYNC | PAD_A | 11 | 16 | 垂直同步信号 |
| PCLK | PAD_A | 12 | 16 | 像素时钟 |
| D0-D7 | PAD_A | 13-20 | 16 | 8 位数据线 |
| MCLK | PAD_A | 26 | 16 | 主时钟输出 |

**注意**:
- 该函数由板型相关代码实现，不同板型的引脚配置可能不同
- 使用 `__attribute__((weak))` 修饰，允许在应用代码中覆盖默认配置
- DVP 接口使用 8 位并行数据线，支持 YUV422 和灰度格式

## 配置参数说明

### 输入数据格式

| 格式 | 说明 | 每像素字节数 |
|-----|------|-------------|
| `LISA_DVP_INPUT_FORM_YUV422_Y0CBY1CR` | YUV422 格式，字节序为 Y0CbY1Cr | 2 |
| `LISA_DVP_INPUT_FORM_LUMINA_8BIT` | 8 位亮度（灰度）格式 | 1 |

### 信号极性

| 参数 | 值 | 说明 |
|-----|---|------|
| `LISA_DVP_POL_FALLING` | 0 | 下降沿有效 |
| `LISA_DVP_POL_RISING` | 1 | 上升沿有效 |

用于配置 VSYNC、HSYNC 和 PCLK 的有效边沿。

### 数据对齐方式

| 对齐方式 | 说明 |
|---------|------|
| `LISA_DVP_DATA_ALIGN_RIGHT` | 右对齐（低位对齐） |
| `LISA_DVP_DATA_ALIGN_LEFT` | 左对齐（高位对齐） |

当数据位宽小于总线宽度时，指定数据的对齐方式。

### 事件类型

| 事件 | 触发时机 | 使用场景 |
|-----|---------|---------|
| `LISA_DVP_EVENT_DONE` | 普通模式下一帧接收完成 | 单缓冲区模式 |
| `LISA_DVP_EVENT_PING_DONE` | Ping 缓冲区接收完成 | Ping-Pong 模式 |
| `LISA_DVP_EVENT_PONG_DONE` | Pong 缓冲区接收完成 | Ping-Pong 模式 |

## 注意事项

1. **必须先配置**: 设备获取后必须先调用 `lisa_dvp_setup()` 配置设备，才能使用其他功能 API
2. **缓冲区对齐**: 接收缓冲区必须 4 字节对齐，建议使用 `__attribute__((aligned(4)))` 修饰
3. **缓冲区大小**: 缓冲区大小必须足够容纳一帧数据，计算公式：
   - YUV422 格式：`width × height × 2` 字节
   - 8 位灰度：`width × height` 字节
4. **GPDMA 通道**: 需指定一个可用的 GPDMA 通道，不同设备使用的通道不能冲突
5. **时钟频率**: `lisa_dvp_enable_clockout()` 的时钟频率参数需根据摄像头规格配置，常见值为 12MHz、24MHz、25MHz
6. **Ping-Pong 模式**: Ping-Pong 模式下，两个缓冲区会交替接收数据，需在回调中及时处理并重载缓冲区
7. **重载时机**: `lisa_dvp_reload()` 和 `lisa_dvp_reload_pingpong()` 应在回调函数中调用，确保连续捕获不丢帧
8. **停止捕获**: 调用 `lisa_dvp_stop()` 后需重新调用 `lisa_dvp_start()` 才能继续捕获
9. **线程安全**: 驱动内部使用互斥锁保护，但建议在同一线程中操作同一设备
10. **信号极性**: VSYNC、HSYNC、PCLK 的极性需根据摄像头数据手册配置，配置错误会导致图像错乱或无法捕获
11. **像素/行偏移**: `pixel_offset` 和 `line_offset` 用于裁剪图像，通常设置为 0
12. **回调函数**: 回调函数在中断上下文中执行，应尽量简短，避免阻塞操作
13. **缓冲区长度**: 传递给 `lisa_dvp_start()` 和相关函数的 `len` 参数单位为字节

## 文件说明

- [lisa_dvp.h](lisa_dvp.h) - DVP 驱动头文件，包含所有对外接口定义
- [lisa_dvp_arcs.c](lisa_dvp_arcs.c) - ARCS 平台的 DVP 驱动实现
- [Kconfig](Kconfig) - DVP 驱动的 Kconfig 配置文件
- [CMakeLists.txt](CMakeLists.txt) - DVP 驱动的 CMake 构建配置

## 相关示例

完整的使用示例请参考：[samples/drivers/devices/lisa_camera/src/main.c](../../samples/drivers/devices/lisa_camera/src/main.c)
