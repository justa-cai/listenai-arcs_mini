# LISA Camera 摄像头驱动示例

## 功能说明

本示例演示如何使用 LISA Camera 驱动接口，实现连续捕获图像并通过串口高速发送的功能。主要包括：

1. **设备初始化**：初始化摄像头、DVP、I2C、UART等设备。
2. **参数配置**：配置摄像头的分辨率、像素格式等参数。
3. **连续捕获**：在主循环中连续捕获图像帧。
4. **串口发送**：将捕获到的图像数据通过串口（DMA模式，3Mbps）发送出去，可用于上位机显示或分析。

本示例展示了摄像头驱动在实际应用场景中的数据流处理，需要连接摄像头硬件才能正常运行。

## 硬件连接

### 摄像头连接

摄像头通过 DVP (Digital Video Port) 接口连接到主控芯片：

| 信号 | 说明 | 连接 |
|-----|------|------|
| XCLK | 主时钟输出（24MHz）| 主控→摄像头 |
| PCLK | 像素时钟输入 | 摄像头→主控 |
| VSYNC | 垂直同步信号 | 摄像头→主控 |
| HSYNC | 水平同步信号 | 摄像头→主控 |
| D0-D7 | 8位数据总线 | 摄像头→主控 |
| SDA | I2C数据线（传感器配置）| 双向 |
| SCL | I2C时钟线（传感器配置）| 主控→摄像头 |
| VCC | 电源（通常3.3V或2.8V）| - |
| GND | 地 | - |

### 支持的摄像头传感器

本驱动支持以下摄像头传感器（通过I2C自动探测）：

| 传感器 | 最大分辨率 | 像素格式 | I2C地址 |
|--------|-----------|---------|---------|
| OV2640 | UXGA (1600x1200) | RGB565/YUV422/JPEG | 0x30 |
| OV3660 | QXGA (2048x1536) | RGB565/YUV422/JPEG | 0x3C |
| GC0328 | VGA (640x480) | RGB565/YUV422 | 0x21 |
| GC032A | VGA (640x480) | RGB565/YUV422 | 0x21 |
| SC030IOT | VGA (640x480) | RGB565/YUV422 | 0x68 |

## 示例步骤

1. 初始化 `uart1` 设备，配置为 3Mbps 波特率，DMA 传输模式。
2. 获取 `camera` 设备句柄并检查设备是否就绪。
3. 配置摄像头硬件参数，包括 MCLK、PWDN、I2C 等。
4. 查询并打印摄像头的能力信息（最大分辨率、支持格式等）。
5. 配置并挂载 DVP 总线接口。
6. 启动摄像头。
7. 进入主循环，连续调用 `lisa_camera_capture()` 捕获图像帧。
8. 将捕获到的图像帧数据通过 `uart1` 异步发送。
9. 释放帧缓冲区，准备下一次捕获。

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 预期输出

```
=== LISA Camera Driver Sample ===
Serial initialized (3Mbps)
camera device ready
Setting up camera...
Camera capabilities: max_width=1600, max_height=1200, supported_formats=0x... 
Starting camera...
Captured frame: 640x480, len=614400, ts=123
Send frame: .. .. .. ... .. .. .., len=614400
Frame 1 sending...
Captured frame: 640x480, len=614400, ts=156
Send frame: .. .. .. ... .. .. .., len=614400
Frame 2 sending...
...
```

## 核心 API

### 设备管理

| API | 说明 |
|-----|------|
| `lisa_device_get()` | 获取设备对象 |
| `lisa_device_ready()` | 检查设备是否就绪 |

### 总线配置

| API | 说明 |
|-----|------|
| `lisa_camera_attach_bus()` | 附加DVP/SPI总线接口 |

### 摄像头配置

| API | 说明 |
|-----|------|
| `lisa_camera_setup()` | 配置摄像头参数（分辨率、格式等）|
| `lisa_camera_get_capabilities()` | 查询摄像头能力 |

### 图像捕获

| API | 说明 |
|-----|------|
| `lisa_camera_start()` | 启动摄像头 |
| `lisa_camera_stop()` | 停止摄像头 |
| `lisa_camera_capture()` | 捕获一帧图像 |
| `lisa_camera_release_fb()` | 释放帧缓冲区 |


## 关键代码

### 1. 配置DVP总线

```c
lisa_camera_bus_config_t bus_config = {
    .bus_type = LISA_CAMERA_BUS_DVP,
    .config.dvp = {
        .dvp_dev = dvp_dev,
        .pixel_offset = 0,
        .line_offset = 0,
        .pclk_polarity = 1,   /* 上升沿采样 */
        .vsync_polarity = 0,  /* 低电平有效 */
        .hsync_polarity = 0,  /* 低电平有效 */
        .data_width = 8,      /* 8位数据总线 */
        .dma_dev = dma_dev,
        .dma_channel = 0,
    }
};

lisa_camera_attach_bus(camera_dev, &bus_config);
```

### 2. 配置并启动摄像头

```c
lisa_camera_config_t config = {
    .pixel_format = LISA_CAMERA_PIXFMT_RGB565,
    .framesize = LISA_CAMERA_FRAMESIZE_QVGA,  /* 320x240 */
    .xclk_freq_hz = 24000000,  /* 24MHz */
    .fb_count = 2,             /* 2个帧缓冲区 */
    .enable_hmirror = false,
    .enable_vflip = false,
};

lisa_camera_setup(camera_dev, &config);
lisa_camera_start(camera_dev);
```

### 3. 捕获并发送图像

```c
/* 主循环：捕获图像并通过串口发送 */
int send_cnt = 0;
while (1) {
    lisa_camera_fb_t *fb = NULL;

    /* 捕获一帧图像 */
    ret = lisa_camera_capture(camera_dev, &fb);
    if (ret != LISA_DEVICE_OK || fb == NULL) {
        LOGE("Capture failed: %d", ret);
        continue;
    }

    /* 通过串口发送图像数据 */
    if (serial_tx_ready()) {
        memcpy(uart_buf, fb->buf, fb->len);
        serial_send(uart_buf, fb->len);
        send_cnt++;
    }

    /* 立即释放帧缓冲区 */
    lisa_camera_release_fb(camera_dev, fb);
}
```

## 配置说明

示例需要在 `prj.conf` 中配置：

```ini
CONFIG_LISA_DEVICE=y      # 使能 LISA 设备框架
CONFIG_LISA_I2C=y         # 使能 I2C 驱动（传感器配置）
CONFIG_LISA_I2C0=y        # 使能 I2C0 控制器
CONFIG_LISA_CAMERA=y      # 使能 Camera 驱动
CONFIG_LOG=y              # 使能日志功能
```

## 像素格式说明

| 格式 | 说明 | 字节/像素 | 用途 |
|-----|------|----------|------|
| RGB565 | 16位RGB | 2 | 显示、预览 |
| RGB888 | 24位RGB | 3 | 高质量图像 |
| YUV422 | YUV 4:2:2 | 2 | 视频编码 |
| YUV420 | YUV 4:2:0 | 1.5 | 视频压缩 |
| GRAY | 灰度 | 1 | 图像处理 |
| JPEG | JPEG压缩 | 可变 | 存储、传输 |

## 分辨率说明

| 名称 | 分辨率 | 像素数 | 用途 |
|-----|--------|--------|------|
| QQVGA | 160x120 | 19K | 低功耗预览 |
| QVGA | 320x240 | 77K | 预览、识别 |
| VGA | 640x480 | 307K | 标准图像 |
| SVGA | 800x600 | 480K | 高清预览 |
| HD | 1280x720 | 922K | 高清视频 |
| UXGA | 1600x1200 | 1.92M | 高分辨率照片 |

## 注意事项

1. **内存需求**：
   - QVGA RGB565: ~154KB/帧
   - VGA RGB565: ~614KB/帧
   - 需要预留足够的内存用于帧缓冲区

2. **帧率**：
   - 帧率取决于分辨率、像素格式和系统负载
   - 典型值：QVGA 30fps, VGA 15fps

3. **时钟配置**：
   - 摄像头通常需要24MHz外部时钟（XCLK）
   - 确保时钟配置正确

4. **I2C通信**：
   - 传感器通过I2C配置寄存器
   - 确保I2C引脚和上拉电阻正确

5. **DMA传输**：
   - 建议使用DMA提高传输效率
   - 注意DMA缓冲区对齐要求

6. **回调函数**：
   - 回调在中断上下文执行
   - 不要在回调中执行耗时操作
   - 不要在回调中阻塞

## 故障排查

### 1. 设备初始化失败

**现象**：`lisa_device_get()` 返回 NULL 或不就绪

**可能原因**：
- 设备未在设备树中配置
- 驱动未使能
- 引脚配置错误

**解决方法**：
- 检查 `prj.conf` 中的配置项
- 确认设备树配置正确
- 使用示波器检查XCLK信号

### 2. 捕获失败

**现象**：`lisa_camera_capture()` 返回错误

**可能原因**：
- 摄像头未正确初始化
- DVP信号连接错误
- DMA配置问题

**解决方法**：
- 检查PCLK、VSYNC、HSYNC信号
- 确认数据线D0-D7连接正确
- 验证DMA通道配置

### 3. 图像异常

**现象**：图像花屏、偏色、条纹

**可能原因**：
- 时钟极性配置错误
- 数据对齐问题
- 内存不足

**解决方法**：
- 调整 `pclk_polarity`、`vsync_polarity`、`hsync_polarity`
- 检查帧缓冲区大小
- 确保DMA缓冲区地址对齐

## 参考资料

- [LISA Device 框架文档](../../drivers/lisa_device/README.md)
- [LISA Camera 驱动文档](../../drivers/lisa_camera/README.md)
- [DVP接口规范](https://en.wikipedia.org/wiki/Digital_video_port)
