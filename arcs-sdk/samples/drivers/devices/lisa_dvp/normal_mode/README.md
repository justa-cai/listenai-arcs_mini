# LISA DVP 驱动示例 - 普通模式

## 功能说明

本示例演示如何使用 LISA DVP 驱动的普通模式捕获图像数据。

## 主要功能

- 初始化 DVP 设备并配置采集参数
- 使用单缓冲区普通模式捕获图像帧
- 通过回调函数处理接收到的图像数据
- 在回调中重载缓冲区实现连续捕获
- 捕获指定帧数后自动停止

## 硬件要求

- ARCS EVB 开发板
- 支持 DVP 接口的摄像头模块（如 GC032A）
- 正确连接 DVP 信号线：
  - HSYNC (PAD_A 10)
  - VSYNC (PAD_A 11)
  - PCLK (PAD_A 12)
  - D0-D7 (PAD_A 13-20)
  - MCLK (PAD_A 26)

## 配置说明

### 图像参数

- **分辨率**: 640x480
- **格式**: YUV422 (Y0CbY1Cr)
- **缓冲区大小**: 640 × 480 × 2 = 614,400 字节

### DVP 配置

- **DMA 通道**: 2
- **数据对齐**: 左对齐
- **VSYNC 极性**: 上升沿
- **HSYNC 极性**: 上升沿
- **PCLK 极性**: 上升沿
- **MCLK 频率**: 25MHz

### 捕获设置

- **捕获帧数**: 10 帧
- **模式**: 普通模式（单缓冲区）

## 使用方法

### 1. 编译

```{eval-rst}
.. include:: /sample_build.rst
```

### 2. 烧录

```bash
lisa zep flash
```

### 3. 查看输出

通过串口工具（如 lisa zep monitor）查看日志输出：

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

## 代码说明

### 主要流程

1. **初始化阶段**
   - 获取 DVP 设备
   - 配置 DVP 参数（分辨率、格式、信号极性等）
   - 启用时钟输出（为摄像头提供 MCLK）

2. **捕获阶段**
   - 清空缓冲区
   - 启动 DVP 捕获
   - 等待帧数据接收完成

3. **回调处理**
   - 接收 `LISA_DVP_EVENT_DONE` 事件
   - 处理接收到的图像数据
   - 调用 `lisa_dvp_reload()` 重载缓冲区继续捕获
   - 达到指定帧数后停止捕获

### 关键函数

- `lisa_dvp_setup()` - 初始化并配置 DVP 设备
- `lisa_dvp_enable_clockout()` - 启用时钟输出
- `lisa_dvp_start()` - 启动普通模式捕获
- `lisa_dvp_reload()` - 重载缓冲区（在回调中调用）
- `lisa_dvp_stop()` - 停止捕获

## 注意事项

1. **缓冲区对齐**: 帧缓冲区必须 4 字节对齐
2. **缓冲区位置**: 帧缓冲区放在 PSRAM 中以节省 SRAM 空间
3. **缓冲区大小**: 必须足够容纳一帧完整数据
4. **重载时机**: 必须在回调函数中及时重载缓冲区，否则会停止捕获
5. **回调上下文**: 回调函数在中断上下文中执行，应避免耗时操作
6. **信号极性**: DVP 信号极性需根据摄像头规格配置

## 适用场景

- 单帧捕获
- 低速连续捕获
- 帧间需要较长处理时间的应用
- 内存资源受限的场景（只需要一个缓冲区）

## 相关文档

- [LISA DVP 驱动 README](../../../../drivers/lisa_dvp/README.md)
- [Ping-Pong 模式示例](../pingpong_mode/README.md)
