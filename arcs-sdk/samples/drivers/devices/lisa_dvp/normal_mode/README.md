# DVP 普通模式捕获示例

## 功能说明

本示例演示如何使用 LISA DVP 驱动的普通模式捕获图像数据，使用单缓冲区进行连续帧捕获。

## 硬件连接

- **PAD_A 10**: HSYNC
- **PAD_A 11**: VSYNC
- **PAD_A 12**: PCLK
- **PAD_A 13-20**: D0-D7（8位数据总线）
- **PAD_A 26**: MCLK

连接支持 DVP 接口的摄像头模块（如 GC032A）。

## 示例步骤

1. 获取 DVP 设备
2. 配置 DVP 参数（分辨率 640x480、YUV422 格式、信号极性等）
3. 启用时钟输出（为摄像头提供 25MHz MCLK）
4. 清空缓冲区，启动 DVP 普通模式捕获
5. 通过回调函数处理接收到的图像数据，重载缓冲区实现连续捕获
6. 捕获 10 帧后自动停止

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 烧录

```{eval-rst}
.. include:: /sample_flash.rst
```

## 预期输出

```
========================================
  LISA DVP Driver Sample - Normal Mode
========================================
Frame format: YUV422
Resolution: 640x480
Max frames: 10

DVP device obtained
DVP setup completed (DMA channel: 2)
DVP clockout enabled (25MHz)
DVP capture started (normal mode)
Frame size: 640x480, buffer size: 614400 bytes
Waiting for frames...
Frame 1 captured, size=614400 bytes
Frame data: XX XX XX XX XX XX XX XX
...
Frame 10 captured, size=614400 bytes
Reached maximum capture frames, stopping...
DVP stopped successfully

========================================
Capture completed, total frames: 10
========================================
```

## 核心 API

| API | 说明 |
|-----|------|
| `lisa_dvp_setup()` | 初始化并配置 DVP 设备 |
| `lisa_dvp_enable_clockout()` | 启用时钟输出 |
| `lisa_dvp_start()` | 启动普通模式捕获 |
| `lisa_dvp_reload()` | 重载缓冲区（在回调中调用） |
| `lisa_dvp_stop()` | 停止捕获 |

## 关键代码

```c
/* 配置 DVP 参数 */
lisa_dvp_config_t config = {
    .width = 640,
    .height = 480,
    .format = LISA_DVP_FORMAT_YUV422,
    .dma_channel = 2,
    .vsync_polarity = LISA_DVP_POLARITY_RISING,
    .hsync_polarity = LISA_DVP_POLARITY_RISING,
    .pclk_polarity = LISA_DVP_POLARITY_RISING,
};
lisa_dvp_setup(dvp_dev, &config, dvp_callback, NULL);

/* 启动普通模式捕获 */
lisa_dvp_start(dvp_dev, frame_buffer, sizeof(frame_buffer));

/* 回调中重载缓冲区 */
static void dvp_callback(lisa_dvp_event_t event, void *user_data)
{
    if (event == LISA_DVP_EVENT_DONE) {
        /* 处理帧数据 */
        lisa_dvp_reload(dvp_dev, frame_buffer, sizeof(frame_buffer));
    }
}
```

## 注意事项

1. **缓冲区对齐**：帧缓冲区必须 4 字节对齐
2. **缓冲区位置**：帧缓冲区放在 PSRAM 中以节省 SRAM 空间
3. **缓冲区大小**：必须足够容纳一帧完整数据（640x480x2 = 614400 字节）
4. **重载时机**：必须在回调函数中及时重载缓冲区，否则会停止捕获
5. **回调上下文**：回调函数在中断上下文中执行，应避免耗时操作
