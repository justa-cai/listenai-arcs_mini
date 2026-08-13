# Zig Audio Play 示例

## 功能说明

该示例演示如何在 Zig 中通过 `arcs.Audio` 打开音频设备、配置播放参数，并在运行时输出一段由 Zig 代码生成的 PCM 音频。示例依次播放 1kHz 正弦波、静音、500Hz 正弦波和静音，用于验证 Zig 侧音频 HAL 封装、缓冲写入与播放控制流程。

## 硬件连接

该示例依赖板级音频播放通路，请先确认：

- 已启用 `audio0` 设备
- 板子具备可用的音频输出路径
- 外接耳机、喇叭或评估板默认音频输出通道可正常工作

## 示例内容

1. 打开 `audio0` 设备
2. 配置 16kHz / 16bit / Mono 播放参数
3. 在 Zig 中生成 1kHz 和 500Hz 正弦波采样数据
4. 按段写入播放缓冲
5. 刷新播放队列并停止播放
6. 输出播放结果日志

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

建议将示例路径替换为：

```bash
./build.sh -C -S samples/libraries/zig/audio_play -DBOARD=arcs_evb
```

## 烧录

```{eval-rst}
.. include:: /sample_flash.rst
```

## 预期输出

串口中可看到类似日志：

```text
=== Zig Audio Play Demo ===
Calling zig_audio_play_main()...
Audio play demo starting...
audio0 device opened
Play configured: 16kHz/16bit/Mono
Play started — generating tones...
Segment 1: 1kHz tone (0.8s, 12800 samples)
Segment 2: silence (0.2s)
Segment 3: 500Hz tone (0.8s, 12800 samples)
Segment 4: silence (0.2s)
Flushing audio...
Stopping play...
Audio play demo completed! Total duration: 2s
=== Zig Audio Play PASSED ===
```

听感上应为一声较高的“嘟”、短暂静音，再接一声较低的“嘟”。

## 核心 API

| API | 说明 |
|-----|------|
| `arcs.Audio.open()` | 打开音频设备 |
| `audio.configPlay()` | 配置播放格式、增益和缓冲参数 |
| `audio.startPlay()` | 启动播放 |
| `audio.writePlayData()` | 写入 PCM 数据 |
| `audio.flushPlay()` | 刷新剩余播放数据 |
| `audio.stopPlay()` | 停止播放 |

## 关键代码

```zig
const play_cfg = arcs.Audio.PlayConfig{
    .format = .{
        .sample_rate = 16000,
        .channels = .left,
        .sample_bits = .bit_16,
    },
    .gain = .{
        .analog_gain = 0,
        .digital_gain = -12,
    },
    .buffer_count = 12,
    .buffer_samples = 256,
};

audio.configPlay(play_cfg) catch return -3;
audio.startPlay() catch return -4;
```

## 注意事项

- **音频链路**: 该示例对板级音频链路有依赖，若没有可用输出路径，可能只能看到日志而听不到声音，因此当前 sample 继续保持 build-only
- **调试强化**: 各播放分段、`flushPlay()` 与 `stopPlay()` 失败现在建议显式报错并返回失败码，便于真机调试时定位播放链路问题
- **内存预算**: `prj.conf` 已调整内存配置以适应音频缓冲需求，移植到其他工程时要注意 heap/PSRAM 预算
- **实现方式**: 当前示例使用运行时分段写入方式，避免一次性分配大块音频数组
- **CI 方向**: 后续若要接入真机 CI，建议优先引入可自动判定的回采/loopback 方案，而不是仅依赖串口 PASS 日志
