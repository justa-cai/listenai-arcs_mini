# Opus 编解码器模块

## 概述

此模块为 ARCS 平台提供 Opus 音频编解码支持，基于 Xiph.Org Foundation 的 libopus v1.5.2。

## 文件结构

```
arcs-sdk/modules/opusdec/
├── CMakeLists.txt           # CMake 构建配置
├── Kconfig                  # Kconfig 配置
├── README.md                # 本文档
├── libopus.a                # 预编译的 RISC-V 静态库
├── opus_wrapper.c           # Opus 解码器封装实现
├── opus_wrapper.h           # Opus 解码器封装头文件
├── opus_encoder.c           # Opus 编码器封装实现
├── opus_encoder.h           # Opus 编码器封装头文件
└── include/opus/            # libopus 原始头文件
    ├── opus.h
    ├── opus_defines.h
    ├── opus_multistream.h
    ├── opus_projection.h
    └── opus_types.h
```

## 配置

在 `prj.conf` 中启用：

```ini
CONFIG_SDK_MODULE_OPUS_DECODER=y
```

## 解码器使用示例

```c
#include "opus_wrapper.h"

// 创建解码器
opus_decoder_config_t config = {
    .sample_rate = 48000,
    .channels = 1,
    .frame_size = 960,  // 48kHz / 50 = 960 (20ms)
    .application = 2049  // OPUS_APPLICATION_AUDIO
};

opus_decoder_t decoder = opus_decoder_create(&config);
if (!decoder) {
    // 错误处理
    return;
}

// 解码循环
while (has_opus_data) {
    uint8_t opus_packet[MAX_PACKET_SIZE];
    int16_t pcm_buffer[MAX_PCM_SIZE];
    size_t pcm_len = sizeof(pcm_buffer);

    // 获取 Opus 编码数据...

    // 解码
    opus_decode_result_t result = opus_decoder_decode(
        decoder,
        opus_packet,
        packet_len,
        pcm_buffer,
        &pcm_len
    );

    if (result == OPUS_DECODE_OK) {
        // 处理解码后的 PCM 数据
        // pcm_len 字节为单位
    } else {
        // 错误处理
        break;
    }
}

// 清理
opus_decoder_destroy(decoder);
```

## 编码器使用示例

```c
#include "opus_encoder.h"

// 创建编码器
opus_encoder_config_t config = {
    .sample_rate = 16000,
    .channels = 1,
    .frame_size = 320,      // 16kHz / 50 = 320 (20ms)
    .application = 2048,     // OPUS_APPLICATION_VOIP
    .bitrate = 24000,        // 24 kbps
    .complexity = 5,         // 0-10
    .vbr = true              // 可变比特率
};

opus_encoder_t encoder = opus_encoder_create(&config);
if (!encoder) {
    // 错误处理
    return;
}

// 编码循环
while (has_pcm_data) {
    int16_t pcm_buffer[320];   // 一帧 PCM 数据
    uint8_t opus_packet[1276]; // 最大 Opus 帧大小
    size_t opus_len = sizeof(opus_packet);

    // 获取 PCM 数据...

    // 编码
    opus_encode_result_t result = opus_encoder_encode(
        encoder,
        pcm_buffer,
        320,
        opus_packet,
        &opus_len
    );

    if (result == OPUS_ENCODE_OK) {
        // 发送 opus_packet，长度为 opus_len 字节
        send_to_server(opus_packet, opus_len);
    } else {
        // 错误处理
        break;
    }
}

// 清理
opus_encoder_destroy(encoder);
```

## API 参考

### 解码器 API (opus_wrapper.h)

| 函数 | 说明 |
|------|------|
| `opus_decoder_create()` | 创建解码器 |
| `opus_decoder_destroy()` | 销毁解码器 |
| `opus_decoder_decode()` | 解码 Opus 数据 |
| `opus_decoder_reset()` | 重置解码器状态 |
| `opus_decoder_get_sample_rate()` | 获取采样率 |
| `opus_decoder_get_channels()` | 获取声道数 |
| `opus_decoder_get_frame_size()` | 获取帧大小 |
| `opus_decoder_calc_pcm_size()` | 计算输出缓冲区大小 |
| `opus_decoder_get_version()` | 获取库版本 |

### 编码器 API (opus_encoder.h)

| 函数 | 说明 |
|------|------|
| `opus_encoder_create()` | 创建编码器 |
| `opus_encoder_destroy()` | 销毁编码器 |
| `opus_encoder_encode()` | 编码 PCM 数据为 Opus |
| `opus_encoder_reset()` | 重置编码器状态 |
| `opus_encoder_set_bitrate()` | 设置比特率 |
| `opus_encoder_get_sample_rate()` | 获取采样率 |
| `opus_encoder_get_channels()` | 获取声道数 |
| `opus_encoder_get_frame_size()` | 获取帧大小 |
| `opus_encoder_calc_packet_size()` | 计算输出缓冲区大小 |
| `opus_encoder_get_version()` | 获取库版本 |

## ASR 集成使用示例 (jk_asr.h)

```c
#include "jk_asr.h"

// 创建 ASR 客户端
jk_asr_t *asr = jk_asr_create("ws://192.168.1.100", "9200", &callbacks);

// 连接
jk_asr_connect(asr);

// 启用 Opus 编码模式
if (jk_asr_opus_enable(asr) == 0) {
    // 发送音频（会自动进行 Opus 编码）
    jk_asr_send_audio(asr, pcm_samples, sample_count);

    // 禁用 Opus 编码
    jk_asr_opus_disable(asr);
} else {
    // 使用普通 PCM 模式
    jk_asr_send_audio(asr, pcm_samples, sample_count);
}
```

## 支持的格式

- **采样率**: 8000, 12000, 16000, 24000, 48000 Hz
- **声道数**: 1 (单声道) 或 2 (立体声)
- **帧大小**: 通常为采样率/50 (20ms)
- **编码格式**: Opus 比特流
- **解码格式**: 16 位 PCM

## 编译信息

- **工具链**: RISC-V GCC 10.4.0 (Nuclei)
- **架构**: RV32IMACF (浮点指令)
- **ABI**: ilp32f
- **优化级别**: -O2

## 与 lisa_player 集成

lisa_player 模块已内置 OGG/Opus 解封装支持。此模块主要用于：
- 直接编解码 Opus 比特流（无需 OGG 容器）
- 自定义音频处理流程
- VoIP 或实时音频应用（如 ASR 语音识别）

## 许可证

libopus 使用 BSD 3-Clause 许可证。详见 opus 源码目录。
