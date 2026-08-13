# USB CherryUSB UAC 设备示例

## 功能说明

演示如何使用 CherryUSB UAC（USB Audio Class）设备功能将开发板作为标准 USB 声卡接入 PC，实现 PC 端录音和播放。

本示例基于 CherryUSB 协议栈实现 UAC1.0 设备功能，并通过 `lisa_audio` 对接板端真实音频链路。PC 端可枚举出标准 USB Audio 设备，无需安装私有驱动即可使用系统录音、播放工具或 Audacity 等音频软件。

默认音频格式如下：

| 方向 | 功能 | 默认配置 | 数据来源/去向 |
|------|------|----------|---------------|
| MIC | USB IN，设备到 PC | 16 kHz, 2 ch, 16 bit | `lisa_audio_record` 采集真实 MIC 数据 |
| Speaker | USB OUT，PC 到设备 | 16 kHz, 1 ch, 16 bit | `lisa_audio_play` 播放 PC 下发的数据 |

MIC/Speaker 的采样率、通道数、采样深度、录音增益、播放增益和缓冲参数可通过 Kconfig 调整。UAC 配置项使用中性的 `CONFIG_UAC_*` 命名，不绑定具体 USB 协议栈；CherryUSB 相关开关仅用于本示例选择底层 USB device 栈。

## 硬件连接

### 开发板

- Arcs-EVB 开发板
- Arcs-Mini 开发板
- Venusa RD EVB 开发板

### 音频连接

- **MIC 输入**：使用开发板板载或外接麦克风，具体硬件连接由对应开发板和 `lisa_audio` 驱动配置决定。
- **Speaker 输出**：使用开发板板载或外接喇叭，具体硬件连接由对应开发板和 `lisa_audio` 驱动配置决定。
- **Venusa RD EVB**：喇叭功放 PA_EN 接在 GPIOA0，本示例提供 `venusa_rd_evb_overlay.conf` 用于显式使能 PA 控制。

### USB 连接

- 使用开发板 USB device 口通过 USB 线缆连接到 PC。
- 本示例默认使用 USB High-Speed，并在描述符配置中检查 isochronous endpoint 包大小。

## 示例内容

1. 初始化 `lisa_audio`，配置真实 MIC 录音和 speaker 播放参数。
2. 初始化 CherryUSB device controller，注册 UAC1.0 描述符、AudioControl 和 AudioStreaming 接口。
3. 将 `lisa_audio_record` 采集到的 MIC 数据缓存后通过 USB IN endpoint 发送给 PC。
4. 将 PC 通过 USB OUT endpoint 下发的 speaker 数据缓存后写入 `lisa_audio_play` 播放。
5. 处理 UAC mute、volume、sampling frequency 等 class control 请求。
6. PC 端识别为标准 USB Audio 录音和播放设备。

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

### Arcs-EVB

```bash
./build.sh -S samples/subsys/usb/device/cherryusb_uac -DBOARD=arcs_evb
```

### Arcs-Mini

```bash
./build.sh -S samples/subsys/usb/device/cherryusb_uac -DBOARD=arcs_mini
```

### Venusa RD EVB

Venusa RD EVB 建议叠加 PA_EN overlay：

```bash
./build.sh -S samples/subsys/usb/device/cherryusb_uac \
  -DBOARD=venusa_rd_evb \
  -DCONFIG_FILES=venusa_rd_evb_overlay.conf
```

## 烧录

```{eval-rst}
.. include:: /sample_flash.rst
```

Venusa RD EVB 示例烧录命令：

```bash
./tools/burn/cskburn -C venusa -s /dev/ttyACM0 -b 3000000 0x0 build/arcs.bin
```

Arcs-EVB 示例烧录命令：

```bash
./tools/burn/cskburn -C arcs -s /dev/ttyACM0 -b 3000000 0x0 build/arcs.bin
```

## 预期输出

**设备端日志:**

```text
********SDK 0.1.7 @ v0.0.1-1810-g555f61e57ca1********
Running on cpu-id: 1
I/elog            [00:00:00.009 1 elog_async] EasyLogger V2.2.99 is initialize success.
I/main            [00:00:00.010 1 main] CherryUSB UAC lisa_audio sample starting
I/uac             [00:00:00.013 1 main] CherryUSB UAC registered: mic 16000Hz/16bit/2ch packet=4 interval=1, speaker 16000Hz/16bit/1ch packet=2
I/main            [00:00:00.013 1 main] UAC sample ready
I/main            [00:00:05.013 1 main] UAC sample running
```

主机配置音频流后，可看到类似日志：

```text
I/main            [00:00:12.000 1 isr] USB configured
I/uac             [00:00:12.010 1 isr] UAC mic opened
I/uac             [00:00:13.010 1 isr] UAC speaker opened
```

## PC 端测试方法

### Windows

> 正常情况下设备管理器中可看到 USB Audio 设备，系统声音设置或 Audacity 中可选择对应的录音和播放设备。

1. 在“声音设置”中选择 `ListenAI CherryUSB UAC` 对应的输入设备进行录音。
2. 在“声音设置”中选择 `ListenAI CherryUSB UAC` 对应的输出设备进行播放。
3. 也可以使用 Audacity 选择该设备进行录音、播放和波形检查。

### Linux

```bash
# 查看 USB 枚举信息
lsusb
lsusb -t

# 查看 ALSA 声卡列表
arecord -l
aplay -l
```

录制 MIC 原始 PCM 数据：

```bash
# <card> 替换为 arecord -l 中枚举出的声卡编号
arecord -D hw:<card>,0 -f S16_LE -c 2 -r 16000 -d 5 mic_16k_2ch.raw
```

录制 WAV 文件：

```bash
arecord -D hw:<card>,0 -f S16_LE -c 2 -r 16000 -d 5 mic_16k_2ch.wav
```

播放 WAV 文件到开发板 speaker：

```bash
aplay -D hw:<card>,0 -f S16_LE -c 1 -r 16000 test_16k_mono.wav
```

也可以使用 Audacity 选择 `ListenAI CherryUSB UAC` 进行录音和播放验证。

## 核心 API

### UAC Device API

| API | 说明 |
|-----|------|
| `uac_device_register()` | 注册 UAC descriptor、class callback、stream 和 endpoint |
| `uac_device_handle_usb_event()` | 处理 USB disconnect 等事件并复位 UAC stream 状态 |
| `uac_stream_t` | UAC 数据流抽象，用于对接 MIC capture 和 speaker playback |
| `uac_stream_ops_t::read()` | 从 capture stream 读取 MIC 数据并发送给 USB host |
| `uac_stream_ops_t::write()` | 将 USB host 下发的 speaker 数据写入 playback stream |
| `uac_stream_ops_t::set_mute()` | 响应 host mute 控制 |
| `uac_stream_ops_t::set_volume()` | 响应 host volume 控制 |

### USB Device API

| API | 说明 |
|-----|------|
| `usb_device_start()` | 初始化 CherryUSB device controller 并启动 USB 枚举 |

### MIC API

| API | 说明 |
|-----|------|
| `mic_init()` | 初始化 `lisa_audio_record` 并注册录音回调 |
| `mic_stream()` | 获取 MIC capture stream，用于注册到 UAC device |

### Speaker API

| API | 说明 |
|-----|------|
| `speaker_init()` | 初始化 `lisa_audio_play` 并配置播放参数 |
| `speaker_stream()` | 获取 speaker playback stream，用于注册到 UAC device |

## 关键代码

### UAC 设备组装

```c
ret = mic_init(audio_dev);
ret = speaker_init();

const uac_device_config_t uac_config = {
    .capture = mic_stream(),
    .playback = speaker_stream(),
};

ret = uac_device_register(&uac_config);
ret = usb_device_start(usb_event_handler);
```

### MIC 数据流

```c
static uint32_t mic_read(uac_stream_t *stream, uint8_t *buffer, uint32_t bytes)
{
    mic_priv_t *priv = (mic_priv_t *)stream->priv;
    uint32_t copied;

    taskENTER_CRITICAL();
    copied = ring_buf_get(&priv->ring, buffer, bytes);
    taskEXIT_CRITICAL();

    return copied;
}
```

### Speaker 数据流

```c
static uint32_t speaker_write(uac_stream_t *stream, const uint8_t *data, uint32_t bytes)
{
    speaker_priv_t *priv = (speaker_priv_t *)stream->priv;
    uint32_t sample_bytes = priv->sample_bits / 8U;
    uint32_t samples = bytes / sample_bytes;

    int ret = lisa_audio_play_write(priv->audio_dev, data, samples);
    return (ret > 0) ? (uint32_t)ret * sample_bytes : 0U;
}
```

### UAC 与音频任务

```c
/* MIC task: 从 capture stream 读取数据并发到 UAC IN endpoint */
/* Speaker OUT callback: 将 USB OUT 小包放入 ring buffer */
/* Speaker task: 从 ring buffer 取数据并写入 lisa_audio_play */
```

## 目录结构

```text
src/
├── main.c              # 组装 USB device、UAC、MIC 和 speaker
├── mic/                # lisa_audio_record 输入流
│   ├── mic.c
│   └── mic.h
├── speaker/            # lisa_audio_play 输出流
│   ├── speaker.c
│   └── speaker.h
└── uac/                # USB device、UAC descriptor、class callback 和 endpoint 数据流
    ├── usb_config.h
    ├── usb_device.c
    ├── usb_device.h
    ├── usb_device_config.h
    ├── uac_descriptors.c
    ├── uac_descriptors.h
    ├── uac_device.c
    ├── uac_device.h
    ├── uac_stream.c
    └── uac_stream.h
```

## 配置说明

常用 Kconfig 配置项：

| 配置项 | 默认值 | 说明 |
|--------|--------|------|
| `CONFIG_UAC_MIC_SAMPLE_RATE` | 16000 | MIC 采样率 |
| `CONFIG_UAC_MIC_CHANNELS` | 2 | MIC 通道数，UAC 描述符最多支持 16 ch |
| `CONFIG_UAC_MIC_SAMPLE_BITS` | 16 | MIC 采样深度 |
| `CONFIG_UAC_SPK_SAMPLE_RATE` | 16000 | Speaker 采样率 |
| `CONFIG_UAC_SPK_CHANNELS` | 1 | Speaker 通道数 |
| `CONFIG_UAC_SPK_SAMPLE_BITS` | 16 | Speaker 采样深度 |
| `CONFIG_UAC_HS_INTERVAL` | 1 | High-Speed isochronous endpoint interval |
| `CONFIG_UAC_MIC_RING_MS` | 256 | MIC ring buffer 时长 |
| `CONFIG_UAC_SPK_RING_MS` | 256 | Speaker ring buffer 时长 |
| `CONFIG_UAC_PLAY_BUFFER_MS` | 16 | `lisa_audio_play` 单个播放缓冲时长 |
| `CONFIG_UAC_PLAY_BUFFER_COUNT` | 12 | `lisa_audio_play` 播放缓冲数量 |
| `CONFIG_UAC_ENABLE_BOARD_PA` | 板级默认 | 是否启用板级 speaker PA 控制 |

底层 CherryUSB 栈由 `prj.conf` 中的 `CONFIG_CHERRYUSB*` 开关选择；这些配置只负责启用 USB device 栈，不属于 UAC 音频参数。

## 注意事项

1. **协议版本**：本示例使用 CherryUSB UAC1.0；TinyUSB UAC2.0 可参考 `samples/subsys/usb/device/uac`。
2. **USB 速度**：本示例默认使用 USB High-Speed，并在 `uac_descriptors.h` 中做编译期检查。
3. **真实音频链路**：MIC 和 speaker 均对接真实 `lisa_audio`，因此录放效果受开发板硬件、麦克风、功放、喇叭和增益配置影响。
4. **Venusa PA_EN**：Venusa RD EVB 需要控制 GPIOA0 上的 PA_EN，建议使用 `-DCONFIG_FILES=venusa_rd_evb_overlay.conf` 编译。
5. **多通道 MIC**：UAC 描述符最多支持 16 ch；当前 `lisa_audio` 通常只提供 1/2 路真实 MIC，超过硬件输入路数的通道会补 0。
6. **音量控制**：Host 下发的 UAC volume 会映射到 `lisa_audio_play_set_gain()`，实际音量曲线由 `CONFIG_UAC_PLAY_VOLUME_DIVISOR` 和底层 codec/PA 决定。
7. **任务模型**：MIC 侧由 UAC task 主动读取 capture stream；speaker 侧先在 OUT callback 中缓存 USB 数据，再由 speaker task 按 `lisa_audio_play` 粒度写入播放驱动。
8. **驱动支持**：Windows 10/11、Linux 均原生支持 USB Audio，无需额外私有驱动。
9. **实机验证**：USB Audio 需要 PC 枚举和实际录放验证，`sample.yaml` 标记为 `build_only: true`。
