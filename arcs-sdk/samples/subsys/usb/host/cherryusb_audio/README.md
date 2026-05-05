# USB Host Audio（UAC）示例

## 功能说明

演示如何使用 CherryUSB 在 ARCS 芯片上实现 USB Host Audio（UAC 1.0）功能，自动识别外接 USB 音频设备的播放流与录音流，并通过 ISO 传输完成音频数据的收发。

本示例支持三种运行模式，设备接入后自动判断：

- **仅播放**：向 speaker/headphones 流输出 1kHz 正弦波
- **仅录音**：从 mic 流持续接收 PCM 数据并周期性打印统计
- **播放 + 录音**：同时启动两路 ISO 传输 （理论支持，未实际验证）

### 特性

- **自适应格式协商**：优先选择 48kHz，找不到时退回设备首个支持的采样率；自动适配任意声道数与位深
- **复合设备支持**：支持同时含 Mixer Unit、Selector Unit 的复合描述符拓扑（无 IAD 的设备自动 fallback）
- **热插拔**：设备断开时安全停止 ISO 传输，可重新插入

## 硬件连接

### 开发板

- ARCS EVB 开发板（Host 端）× 1

### USB 音频设备

- 标准 USB Audio Class 1.0 设备，例如：
  - 带mic的 USB 音频适配器
  - 带speaker的 USB 音频适配器
  - USB 声卡（耳机 + 麦克风一体）（理论支持，未实际验证）

### USB 连接

将 USB 音频设备插入开发板 `USB_ARCS` 接口（支持通过 USB Hub 连接）。

### USB Host 硬件修改（必须）

开发板 `USB_ARCS` Type-C 接口默认工作在 Device 模式，作为 Host 使用须完成以下硬件修改：

- **VBUS**：直接对 Type-C 接口供电 5V
- **CC1 / CC2**：分别接 22K 上拉电阻到 VBUS

> **注意**：未做硬件修改时无法识别外接 USB 设备。

## 示例内容

1. USB PHY 初始化，配置为 Host 模式
2. 初始化 CherryUSB Host 协议栈
3. 等待 USB 音频设备热插拔
4. 设备接入后解析音频控制描述符，打印 speaker/mic 流的格式信息
5. 自动选择最佳格式（优先 48kHz，兼容任意位深和声道数）
6. 两阶段启动：先对所有可用流执行 `SET_INTERFACE`，再依次启动 ISO 传输
7. 播放流：每次回调填充 1kHz 正弦波 PCM 数据（256 点正弦表 + 16.16 定点相位累加器）
8. 录音流：每 500 次回调打印一次数据统计和十六进制片段
9. 设备断开时停止 ISO 传输，释放任务资源，等待重新插入

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 烧录

```{eval-rst}
.. include:: /sample_flash.rst
```

## 预期输出

**设备端日志（GeneralPlus USB 声卡，2ch/16bit/48kHz speaker + 1ch/16bit/48kHz mic）：**

```
========================================
  CherryUSB Host Audio Example
  Auto: playback / recording / both
========================================

[INFO] USB Host initialized, connect USB audio device

[I/usbh_hub] New full-speed device on Bus 0, Hub 2, Port 3 connected
[I/usbh_core] New device found,idVendor:1b3f,idProduct:2008,bcdDevice:0100
[I/usbh_core] Manufacturer: GeneralPlus
[I/usbh_core] Product: USB Audio Device
[I/usbh_audio] AC units: 2 IT, 2 OT, 9 total units, 2 streams
[I/usbh_audio] ============= Audio module information ===================
bcdADC :0100
Num of audio stream :2
    stream name :speaker
    stream intf :1
    Num of altsetting :2
        Altsetting :1
            bNrChannels :2
            bBitResolution :16
            bSamFreqType :2
                SampleFreq :44100
                SampleFreq :48000
    stream name :mic
    stream intf :2
    Num of altsetting :2
        Altsetting :1
            bNrChannels :1
            bBitResolution :16
            bSamFreqType :2
                SampleFreq :44100
                SampleFreq :48000
[I/usbh_audio] ============= Audio module information ===================

[AUDIO] Audio device connected
[AUDIO] Speaker: speaker 2ch/16bit/48000Hz
[I/usbh_audio] Ep=05 Attr=01 Mps=192 Interval=01 Mult=00
[I/usbh_audio] Open audio stream :speaker, altsetting: 1
[AUDIO] speaker unmute: 9
[AUDIO] speaker volume 0dB: 10
[AUDIO] Mic: mic 1ch/16bit/48000Hz
[I/usbh_audio] Ep=86 Attr=01 Mps=100 Interval=01 Mult=00
[I/usbh_audio] Open audio stream :mic, altsetting: 1
[AUDIO] Starting 1kHz sine wave playback...
[I/usbh_audio] Audio ISO playback started (mps=192, packets=32)
[AUDIO] Starting recording...
[I/usbh_audio] Audio ISO streaming started (mps=100, packets=32)
[AUDIO] Active: playback recording
[REC] total: 96 bytes, this packet: 96 bytes
[REC] data[0..63]: 0000000000000000000000000000000000000000000000000000000000000000...
[REC] total: 48096 bytes, this packet: 96 bytes
[REC] data[0..63]: 110105011101110110010B01160125013301230122013301370125012C01...
```


## 核心 API

### USB Host 初始化

| API | 说明 |
|-----|------|
| `usbh_initialize()` | 初始化 USB Host 协议栈 |
| `usb_host_init()` | 初始化 USB PHY（通过 `SYS_INIT` 自动调用） |

### Audio Host API

| API | 说明 |
|-----|------|
| `usbh_audio_open()` | 打开指定音频流，执行 `SET_INTERFACE` 并配置采样率 |
| `usbh_audio_set_mute()` | 设置指定流的静音状态 |
| `usbh_audio_set_volume()` | 设置指定流的音量（单位 dB） |
| `usbh_audio_start_playback()` | 启动播放 ISO OUT 传输，注册填充回调 |
| `usbh_audio_start_streaming()` | 启动录音 ISO IN 传输，注册数据回调 |
| `usbh_audio_stop_streaming()` | 停止 ISO 传输（播放与录音均适用） |
| `usbh_audio_list_module()` | 打印设备音频流能力信息 |

### 用户回调（弱符号覆盖）

| 回调函数 | 说明 |
|---------|------|
| `usbh_audio_run()` | 音频设备接入后由框架调用，创建业务任务并启动 ISO 传输 |
| `usbh_audio_stop()` | 音频设备断开时由框架调用，停止 ISO 传输并通知任务退出 |

## 关键代码

### 自动选择音频流格式

```c
/* 按名称查找流，优先 48kHz，找不到则退回首个支持的采样率 */
find_stream_format(audio_class, "speaker", "headphones", &ctx->speaker);
find_stream_format(audio_class, "mic", NULL, &ctx->mic);
```

### 两阶段启动（先 SET_INTERFACE，再启动 ISO）

```c
/* Phase 1: 打开所有流（SET_INTERFACE） */
usbh_audio_open(audio_class, ctx->speaker.name,
                ctx->speaker.samp_freq, ctx->speaker.bitresolution);
usbh_audio_set_mute(audio_class, ctx->speaker.name, 0, false);
usbh_audio_set_volume(audio_class, ctx->speaker.name, 0, 0);  /* 0dB */

/* Phase 2: 启动 ISO 传输 */
usbh_audio_start_playback(audio_class, on_playback_fill, ctx);
usbh_audio_start_streaming(audio_class, on_audio_data, ctx);
```

### 播放侧 1kHz 正弦波填充回调

```c
static void on_playback_fill(struct usbh_audio *audio_class,
                             uint8_t *buf, uint32_t len, void *arg)
{
    struct audio_ctx *ctx = (struct audio_ctx *)arg;
    int16_t *out = (int16_t *)buf;
    uint32_t num_frames = len / (ctx->speaker.channels * 2);

    for (uint32_t i = 0; i < num_frames; i++) {
        uint8_t idx = (ctx->sine_phase >> 16) & 0xFF;
        int16_t sample = sine_table[idx];
        ctx->sine_phase += ctx->sine_phase_inc;

        for (uint8_t c = 0; c < ctx->speaker.channels; c++) {
            out[i * ctx->speaker.channels + c] = sample;
        }
    }
}
```

## 配置说明

### usb_config.h 关键配置项

| 配置项 | 默认值 | 说明 |
|--------|--------|------|
| `CONFIG_USBHOST_MAX_INTERFACES` | `16` | 最大接口数（复合设备需适当增大） |
| `CONFIG_USBHOST_MAX_INTF_ALTSETTINGS` | `4` | 每个接口最大 altsetting 数 |
| `CONFIG_USBHOST_MAX_ENDPOINTS` | `6` | 最大端点数 |
| `CONFIG_USBHOST_MAX_AUDIO_CLASS` | `1` | 最多同时支持的 UAC 设备数 |
| `CONFIG_USBH_AUDIO_ISO_PACKETS_PER_URB` | `32` | 每个 URB 包含的 ISO 包数量（影响延迟和 CPU 占用） |
| `CONFIG_USB_ALIGN_SIZE` | `32` | DMA 缓冲区对齐字节数（dcache 开启时须 32B） |

### CMakeLists.txt

```cmake
# 将 CherryUSB 库代码重定位到 PSRAM，避免 SRAM 溢出
listenai_code_relocate(LIBRARY chryusb LOCATION PSRAM)
```

## 注意事项

1. **硬件修改必须**：开发板 USB 口默认为 Device 模式，作为 Host 使用必须完成 VBUS 5V 供电和 CC1/CC2 22K 上拉电阻修改。

2. **两路 ISO 同时运行的限制**：FS 音频设备经 HS Hub 连接时，同时启动 ISO IN（录音）+ ISO OUT（播放）可能导致播放无声。原因是 ARCS MUSB 的 Split Transaction 调度尚不支持同一 FS 设备的双向并发 ISO 传输。直连（不经 Hub）或仅使用单向流可规避此问题。

3. **SRAM 占用**：CherryUSB 库静态数据较大，建议在 `CMakeLists.txt` 中通过 `listenai_code_relocate` 将其重定位到 PSRAM。

4. **采样率适配**：示例优先选择 48kHz，播放侧正弦波频率和 ISO MPS 均随采样率自动计算，无需手动修改。

5. **复合设备**：mic + speaker的复合设备，理论支持，未实际验证。
