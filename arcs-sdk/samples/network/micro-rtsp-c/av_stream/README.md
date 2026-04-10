# RTSP 音视频流服务器示例

## 功能说明

演示如何使用 micro-rtsp-c 库搭建支持音视频同步传输的 RTSP 流媒体服务器，通过 WiFi 网络实时传输摄像头采集的 JPEG 视频和麦克风采集的音频数据，支持标准 RTSP 客户端接入播放。

本示例实现了完整的音视频 RTSP 服务器功能，包括：
- 使用 JPEG 硬件编码器实时编码摄像头图像
- 使用 PCMU (μ-law) 编码压缩音频数据
- 音视频时间戳同步，确保播放流畅
- 支持 TCP/UDP 双模式传输
- 自动处理 JPEG 量化表传输

### 新特性

- **硬件 JPEG 编码**：使用芯片内置 JPEG 编码器，CPU 占用低，编码速度快
- **实时音视频流**：同时传输摄像头视频和麦克风音频，支持双轨道
- **音视频同步**：使用统一的时间戳基准，确保音画同步
- **PCMU 音频编码**：标准 μ-law 编码，兼容性好，CPU 占用低
- **灵活的分辨率配置**：支持多种摄像头分辨率（320x240 / 640x480）

## 硬件连接

**摄像头接口（DVP）：**
- 使用板载 DVP 摄像头接口
- 支持 YUV422 输出格式
- 分辨率可配置

**麦克风接口（I2S）：**
- 使用板载麦克风
- 采样率：8000 Hz
- 位宽：16-bit

**串口输出：**
- 串口 TX: PA3
- 波特率：921600

## 使用场景

本示例适用于以下场景：

- **视频监控**：实时查看摄像头画面，支持网络远程查看
- **物联网设备**：智能门铃、婴儿监护器等需要音视频传输的设备
- **开发调试**：验证摄像头和音频采集功能，调试编码参数

## 示例步骤

1. 初始化 WiFi 并连接到指定 AP
2. 初始化 JPEG 硬件编码器（YUV422 格式，800x600 分辨率）
3. 初始化音频采集（I2S 麦克风，8000Hz 采样率）
4. 配置音频编码器（PCMU，μ-law 编码）
5. 在 8554 端口启动 RTSP 服务器
6. 等待客户端连接
7. 并行处理：
   - 摄像头线程：采集图像 → JPEG 编码 → 发送视频 RTP 包
   - 音频线程：采集音频 → μ-law 编码 → 发送音频 RTP 包
8. 维持音视频时间戳同步

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 烧录

```{eval-rst}
.. include:: /sample_flash.rst
```

## 预期输出

**串口输出：**

```
[rtsp-test] RTSP Server Starting...
[rtsp-test] WiFi connected and IP obtained
[rtsp-test] DHCP Success: IP=192.168.137.166

[camera] Camera initialized: 800x600, YUV422
[camera] JPEG encoder initialized successfully
[audio] Audio initialized: 8000Hz, 16-bit
[audio] PCMU encoder initialized (μ-law)

[rtsp-test] Starting RTSP server on port 8554
+ UDP socket created: rtp_port=6970 rtcp_port=6971
+ Audio UDP transport: rtp_port=6972 rtcp_port=6973

[rtsp-test] client connected: 192.168.137.1

+ RTSP command: DESCRIBE
+ SDP: Video track=track1, Audio track=track2
+ RTSP command: SETUP (track1)
+ Video SETUP: client_port=54224-54225 server_port=6970-6971
+ RTSP command: SETUP (track2)
+ Audio SETUP: client_port=54226-54227 server_port=6972-6973
+ RTSP command: PLAY
+ PLAY: streaming enabled (video+audio)

[camera] Frame captured: 37150 bytes, encoded in 45ms
[audio] Audio frame: 640 bytes -> 320 bytes (μ-law)
+ UDP send VIDEO RTP: size=1082 port=54224 ret=1082
+ UDP send AUDIO RTP: size=332 port=54226 ret=332
```

**客户端播放：**

使用 ffplay 播放（UDP 模式，推荐）：
```bash
ffplay -rtsp_transport udp rtsp://192.168.137.166:8554/mjpeg/1 -fflags nobuffer -flags low_delayffplay
```

使用 ffplay 播放（TCP 模式）：
```bash
ffplay -rtsp_transport tcp rtsp://192.168.137.166:8554/mjpeg/1 -fflags nobuffer -flags low_delayffplay
```

预期效果：
- 实时显示摄像头画面（约 10-15 fps）
- 同步播放麦克风采集的音频

## 核心 API

### RTSP 服务器 API

| API | 说明 |
|-----|------|
| `rtsp_streamer_init()` | 初始化 RTSP 流媒体服务 |
| `rtsp_streamer_init_audio()` | 初始化音频配置 |
| `rtsp_streamer_stream_frame()` | 发送视频帧 |
| `rtsp_streamer_stream_audio_frame()` | 发送音频帧 |

### JPEG 编码 API

| API | 说明 |
|-----|------|
| `jpeg_encoder_init()` | 初始化 JPEG 编码器 |
| `jpeg_encoder_encode()` | 编码一帧图像 |
| `jpeg_encoder_stop()` | 停止编码器 |

### 音频编码 API

| API | 说明 |
|-----|------|
| `pcm16_to_ulaw()` | PCM 16-bit 转 μ-law 编码 |
| `audio_capture_frame()` | 采集并编码音频帧 |

## 功能详解

### 音视频同步机制

示例使用统一的时间基准确保音视频同步：

```c
// 视频时间戳（90000 Hz 时钟）
streamer->start_ms = ms;  // 记录起始时间
uint32_t elapsed_ms = ms - streamer->start_ms;
streamer->timestamp = elapsed_ms * 90;  // 90000/1000 = 90

// 音频时间戳（8000 Hz 采样率）
streamer->audio_timestamp = elapsed_ms * 8;  // 8000/1000 = 8
```

两者使用相同的 `start_ms` 基准，确保时间轴对齐。

### JPEG 编码流程

1. 摄像头采集 YUV422 格式图像
2. CPU 将 YUYV 交叉格式转换为 JPEG MCU 块格式
3. DMA 将 MCU 数据传输到 JPEG 编码器
4. JPEG 硬件编码器输出压缩的 ECS 数据
5. CPU 构建完整的 JPEG 文件（添加头部、量化表、Huffman 表）

### PCMU (μ-law) 音频编码

- **编码格式**：ITU-T G.711 μ-law
- **采样率**：8000 Hz
- **位宽**：16-bit PCM → 8-bit μ-law
- **帧大小**：320 样本（40ms @ 8kHz）
- **压缩比**：2:1（640 字节 → 320 字节）
- **特点**：标准电话音质，CPU 占用极低，兼容性好

## 关键代码

**初始化 JPEG 编码器：**

```c
jpeg_enc_cfg_t jpeg_cfg = {
    .width = 320,
    .height = 240,
    .input_format = 0x00  // YUV422
};

jpeg_encoder_init(&jpeg_enc_ctx, &jpeg_cfg);
```

**编码并发送视频帧：**

```c
// 采集摄像头图像
camera_capture(yuv_buffer, &yuv_len);

// JPEG 硬件编码
jpeg_encoder_encode(&jpeg_enc_ctx, yuv_buffer, jpeg_buffer, &jpeg_len);

// 发送 RTSP 视频流
rtsp_streamer_stream_frame(&streamer, jpeg_buffer, jpeg_len, current_time_ms);
```

**音频编码并发送：**

```c
// 从队列获取 PCM 音频数据并自动编码为 μ-law
uint8_t *audio_frame = NULL;
uint32_t audio_len = 0;

while (audio_capture_frame(&audio_frame, &audio_len) == 0) {
    // 发送 RTSP 音频流（已编码为 μ-law）
    rtsp_streamer_stream_audio_frame(&streamer, audio_frame, audio_len, current_time_ms);
}
```

**PCM 转 μ-law 编码实现：**

```c
static uint8_t pcm16_to_ulaw(int16_t pcm_val)
{
    const int16_t CLIP = 32635;
    const int16_t BIAS = 0x84;

    uint8_t sign = (pcm_val < 0) ? 0x80 : 0;
    if (sign) pcm_val = (int16_t)(-pcm_val);
    if (pcm_val > CLIP) pcm_val = CLIP;

    pcm_val = (int16_t)(pcm_val + BIAS);
    int exponent = 7;
    for (int exp_mask = 0x4000; (pcm_val & exp_mask) == 0 && exponent > 0;
         exp_mask >>= 1, exponent--);

    int mantissa = (pcm_val >> (exponent + 3)) & 0x0F;
    uint8_t ulaw = (uint8_t)(~(sign | (exponent << 4) | mantissa));

    return ulaw;
}
```

**配置音视频参数：**

```c
// 配置音频编码参数
rtsp_audio_config_t audio_cfg = {
    .payload_type = 0,            // PCMU 标准负载类型
    .sample_rate = 8000,          // 8kHz 采样率
    .channels = 1,                // 单声道
    .codec_name = "PCMU"
};

rtsp_streamer_init_audio(&streamer, &audio_cfg);
```

## 配置说明

**WiFi 配置（main.c）：**

```c
#define TARGET_WIFI_SSID   "Your_SSID"
#define TARGET_WIFI_PWD     "Your_Password"
```

**摄像头配置（camera.h）：**

```c
#define CAMERA_WIDTH   800
#define CAMERA_HEIGHT  600
#define CAMERA_FORMAT  YUV422  // YUYV 格式
```

**音频配置（audio.h）：**

```c
#define AUDIO_SAMPLE_RATE  8000   // 8kHz 采样率
#define AUDIO_FRAME_SIZE   320    // 40ms 帧长（320样本）
#define AUDIO_CHANNELS     1      // 单声道
```

**传输优化参数：**

```c
#define MAX_FRAG_SIZE      950    // RTP 包大小（字节）
#define FRAGMENT_DELAY_US  200    // 片段间延迟（微秒）
```

## 注意事项

1. **WiFi 配置**：需修改 WiFi SSID 和密码为实际网络信息
2. **摄像头分辨率**：
   - 支持 320x240、640x480
   - 高分辨率需要更多带宽和编码时间
   - 建议先使用 640x480 测试
3. **JPEG 量化表**：
   - 编码器使用自定义高质量量化表（Quality ≈ 85-90）
   - 自动在第一个 RTP 包中传输量化表
   - 确保客户端颜色还原准确
4. **音频编码格式**：
   - PCMU (μ-law) 是标准的电话音质编码
   - 固定 8kHz 采样率，8-bit 编码
   - CPU 占用极低，兼容性好，所有 RTSP 客户端都支持
5. **网络带宽**：
   - 800x600 @ 10fps + PCMU 音频：约 850-1250 Kbps
   - PCMU 音频固定码率：64 Kbps
   - 确保 WiFi 信号强度足够
6. **UDP 丢包问题**：
   - 默认使用 UDP，网络不稳定时可能丢包导致花屏
   - 客户端可切换到 TCP 模式：`ffplay -rtsp_transport tcp rtsp://...`
   - 已优化片段大小和延迟，减少丢包概率
7. **音视频同步**：
   - 示例已实现基于统一时间基准的同步机制
   - 如发现音画不同步，检查帧率设置和网络延迟
8. **内存占用**：
   - JPEG 编码缓冲区：约 100KB
   - 音频缓冲区：约 10KB
   - 每个客户端连接：约 5KB
   - 总计约需 120KB 可用内存
9. **客户端兼容性**：
    - VLC：完全支持，推荐使用
    - ffplay：完全支持，可选择 TCP/UDP
    - 所有 RTSP 客户端：PCMU 是标准编码，兼容性极好
    - 浏览器：需要插件或转码服务

## 验证方法

1. **视频质量检查**：
   - 观察图像是否清晰，无明显压缩伪影
   - 检查颜色是否准确（不偏色、不过曝）
   - 帧率是否稳定（通过 VLC 统计信息查看）

2. **音频质量检查**：
   - 语音是否清晰可辨
   - 有无明显杂音或失真
   - 音量是否合适

3. **同步检查**：
   - 拍手或敲击，观察画面与声音是否同步
   - 延迟应小于 200ms

4. **稳定性测试**：
   - 长时间运行（1小时以上），检查是否有内存泄漏
   - 频繁连接断开，测试服务器稳定性

## 相关文档

- [micro-rtsp-c 库源码](../../../../modules/micro-rtsp-c/)
- [JPEG 编码器驱动文档](../../../../drivers/jpeg/)
- [I2S 音频驱动文档](../../../../drivers/i2s/)
- [ITU-T G.711 μ-law 规范](https://www.itu.int/rec/T-REC-G.711/)
- [RTSP 协议规范 RFC 2326](https://www.rfc-editor.org/rfc/rfc2326)
- [RTP Payload Format for JPEG RFC 2435](https://www.rfc-editor.org/rfc/rfc2435)
- [RTP Profile for Audio/Video RFC 3551](https://www.rfc-editor.org/rfc/rfc3551)
