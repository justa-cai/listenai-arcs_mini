# LISA DVP Ping-Pong 模式捕获示例

## 功能说明

本示例演示如何使用 LISA DVP 驱动的 Ping-Pong 双缓冲模式实现高速连续图像捕获，两个缓冲区交替使用实现零拷贝连续采集。

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
4. 清空两个缓冲区，启动 Ping-Pong 模式捕获
5. 通过回调函数交替处理 Ping/Pong 缓冲区数据，重载对应缓冲区
6. 捕获 20 帧后自动停止

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
==========================================
  LISA DVP Driver Sample - Ping-Pong Mode
==========================================
Frame format: YUV422
Resolution: 640x480
Max frames: 20
Mode: Ping-Pong (zero-copy continuous capture)

DVP device obtained
DVP setup completed (DMA channel: 2)
DVP clockout enabled (25MHz)
DVP Ping-Pong capture started
Frame size: 640x480, buffer size: 614400 bytes
Ping buffer: 0x60xxxxxx
Pong buffer: 0x60xxxxxx
Waiting for frames (Ping-Pong mode)...
[PING] Frame 1 captured, size=614400 bytes
[PING] Frame data: XX XX XX XX XX XX XX XX
[PONG] Frame 1 captured, size=614400 bytes
[PONG] Frame data: XX XX XX XX XX XX XX XX
[PING] Frame 2 captured, size=614400 bytes
...
[PONG] Frame 10 captured, size=614400 bytes
Reached maximum capture frames, stopping...
DVP stopped successfully

==========================================
Capture completed
Ping frames: 10
Pong frames: 10
Total frames: 20
==========================================
```

## 核心 API

| API | 说明 |
|-----|------|
| `lisa_dvp_setup()` | 初始化并配置 DVP 设备 |
| `lisa_dvp_enable_clockout()` | 启用时钟输出 |
| `lisa_dvp_start_pingpong()` | 启动 Ping-Pong 模式捕获 |
| `lisa_dvp_reload_pingpong()` | 重载缓冲区（在回调中调用） |
| `lisa_dvp_stop()` | 停止捕获 |

## 关键代码

```c
/* 启动 Ping-Pong 模式捕获 */
lisa_dvp_start_pingpong(dvp_dev, ping_buffer, pong_buffer, buffer_size);

/* 回调中处理并重载对应缓冲区 */
static void dvp_callback(lisa_dvp_event_t event, void *user_data)
{
    if (event == LISA_DVP_EVENT_PING_DONE) {
        /* 处理 Ping 缓冲区数据 */
        lisa_dvp_reload_pingpong(dvp_dev, 0, ping_buffer, buffer_size);
    } else if (event == LISA_DVP_EVENT_PONG_DONE) {
        /* 处理 Pong 缓冲区数据 */
        lisa_dvp_reload_pingpong(dvp_dev, 1, pong_buffer, buffer_size);
    }
}
```

## 注意事项

1. **双缓冲区**：需要准备两个大小相同的缓冲区，都必须 4 字节对齐
2. **内存需求**：Ping-Pong 模式需要双倍内存（2 个缓冲区 x 614400 字节）
3. **缓冲区位置**：缓冲区放在 PSRAM 中以节省 SRAM 空间
4. **重载时机**：必须在对应的回调中及时重载缓冲区
5. **处理时间**：处理单帧的时间必须小于采集一帧的时间，否则会造成丢帧
