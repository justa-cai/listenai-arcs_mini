# LISA DVP 驱动示例 - Ping-Pong 模式

## 功能说明

本示例演示如何使用 LISA DVP 驱动的 Ping-Pong 双缓冲模式实现高速连续图像捕获。

## 主要功能

- 初始化 DVP 设备并配置采集参数
- 使用 Ping-Pong 双缓冲模式实现零拷贝连续捕获
- 通过回调函数交替处理 Ping/Pong 缓冲区数据
- 在回调中重载对应缓冲区保持连续采集
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
- **缓冲区大小**: 每个 640 × 480 × 2 = 614,400 字节
- **缓冲区数量**: 2 个（Ping + Pong）

### DVP 配置

- **DMA 通道**: 2
- **数据对齐**: 左对齐
- **VSYNC 极性**: 上升沿
- **HSYNC 极性**: 上升沿
- **PCLK 极性**: 上升沿
- **MCLK 频率**: 25MHz

### 捕获设置

- **捕获帧数**: 20 帧
- **模式**: Ping-Pong 模式（双缓冲区交替）

## Ping-Pong 模式优势

1. **零拷贝**: 硬件 DMA 直接写入用户缓冲区，无需额外拷贝
2. **连续捕获**: 两个缓冲区交替使用，一个接收数据时另一个可以处理
3. **无丢帧**: 避免了单缓冲区模式下的帧间间隔
4. **高吞吐**: 适合高帧率连续采集场景

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

## 代码说明

### 主要流程

1. **初始化阶段**
   - 获取 DVP 设备
   - 配置 DVP 参数（分辨率、格式、信号极性等）
   - 启用时钟输出（为摄像头提供 MCLK）

2. **捕获阶段**
   - 清空两个缓冲区
   - 启动 Ping-Pong 模式捕获
   - 等待帧数据接收完成

3. **回调处理**
   - 接收 `LISA_DVP_EVENT_PING_DONE` 或 `LISA_DVP_EVENT_PONG_DONE` 事件
   - 处理对应缓冲区的图像数据
   - 调用 `lisa_dvp_reload_pingpong()` 重载对应缓冲区
   - 达到指定帧数后停止捕获

### Ping-Pong 工作原理

```
时间线:
  DMA → Ping 缓冲区 ━━━━━━━┓
                         ┃ (回调: PING_DONE)
                         ┗━━→ 处理 Ping 数据
                              ┗━━→ 重载 Ping 缓冲区

  DMA → Pong 缓冲区 ━━━━━━━┓
                         ┃ (回调: PONG_DONE)
                         ┗━━→ 处理 Pong 数据
                              ┗━━→ 重载 Pong 缓冲区

  交替循环 ↻
```

### 关键函数

- `lisa_dvp_setup()` - 初始化并配置 DVP 设备
- `lisa_dvp_enable_clockout()` - 启用时钟输出
- `lisa_dvp_start_pingpong()` - 启动 Ping-Pong 模式捕获
- `lisa_dvp_reload_pingpong()` - 重载缓冲区（在回调中调用）
- `lisa_dvp_stop()` - 停止捕获

## 注意事项

1. **双缓冲区**: 需要准备两个大小相同的缓冲区
2. **缓冲区对齐**: 两个缓冲区都必须 4 字节对齐
3. **缓冲区位置**: 缓冲区放在 PSRAM 中以节省 SRAM 空间
4. **内存需求**: Ping-Pong 模式需要双倍内存（2 个缓冲区）
5. **重载时机**: 必须在对应的回调中及时重载缓冲区
6. **处理时间**: 处理单帧的时间必须小于采集一帧的时间，否则会造成丢帧
7. **回调上下文**: 回调函数在中断上下文中执行，复杂处理应放到任务中
8. **事件区分**: 注意区分 `PING_DONE` 和 `PONG_DONE` 事件，重载对应的缓冲区

## 适用场景

- 高帧率连续捕获
- 实时视频流处理
- 图像序列采集
- 零拷贝要求的应用
- 需要最小化帧间延迟的场景

## 性能对比

| 对比项 | 普通模式 | Ping-Pong 模式 |
|-------|---------|---------------|
| 缓冲区数量 | 1 个 | 2 个 |
| 内存占用 | 较小 | 较大（2倍） |
| 帧间延迟 | 有间隔 | 无间隔 |
| 连续性 | 中断式 | 连续式 |
| 适用场景 | 低速/单帧 | 高速/连续 |
| 处理时间要求 | 宽松 | 严格 |

## 相关文档

- [LISA DVP 驱动 README](../../../../drivers/lisa_dvp/README.md)
- [普通模式示例](../normal_mode/README.md)
