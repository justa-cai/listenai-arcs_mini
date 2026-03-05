# Audio 驱动

基于 lisa_device 框架的 Audio 设备驱动，为 ARCS 平台提供统一的音频录音(Record)和播放(Play)接口。

## 功能特性

- **设备支持**: Audio0 音频设备
- **录音功能**: 支持多通道录音、增益控制、高通滤波、差分输入
- **播放功能**: 支持多通道播放、增益控制、缓冲区管理
- **音频格式**: 灵活配置采样率（8kHz-96kHz）、通道数（单声道/立体声）、采样位深（16/24/32位）
- **统一回调**: 提供统一的音频事件回调，同时接收录音数据和播放回声数据
- **相位补偿**: 支持录音和播放之间的相位调整
- **控制功能**: 支持启动、停止、暂停、恢复等控制操作

## 配置选项

在 `prj.conf` 中启用驱动：

```kconfig
CONFIG_LISA_AUDIO_DEVICE=y
CONFIG_LISA_AUDIO0=y          # 启用 Audio0 设备
```

## API 接口

### 回调管理接口

```c
int lisa_audio_register_callback(lisa_device_t *dev, lisa_audio_callback_t callback, void *user_data);
int lisa_audio_unregister_callback(lisa_device_t *dev, lisa_audio_callback_t callback);
```

### 录音接口

```c
// 配置
int lisa_audio_record_config(lisa_device_t *dev, const lisa_audio_record_config_t *config);

// 控制
int lisa_audio_record_start(lisa_device_t *dev);
int lisa_audio_record_stop(lisa_device_t *dev);
int lisa_audio_record_pause(lisa_device_t *dev);
int lisa_audio_record_resume(lisa_device_t *dev);
int lisa_audio_record_set_gain(lisa_device_t *dev, const lisa_audio_gain_t *gain);
```

### 播放接口

```c
// 配置
int lisa_audio_play_config(lisa_device_t *dev, const lisa_audio_play_config_t *config);

// 数据传输
int lisa_audio_play_write(lisa_device_t *dev, const void *buffer, uint32_t samples);
int lisa_audio_play_get_buffer(lisa_device_t *dev, void **buffer, uint32_t timeout_ms);

// 控制
int lisa_audio_play_start(lisa_device_t *dev);
int lisa_audio_play_stop(lisa_device_t *dev);
int lisa_audio_play_flush(lisa_device_t *dev);
int lisa_audio_play_set_gain(lisa_device_t *dev, const lisa_audio_gain_t *gain);
```

### 通用接口

```c
int lisa_audio_set_phase_compensation(lisa_device_t *dev, const lisa_audio_phase_compensation_t *compensation);
int lisa_audio_get_phase_compensation(lisa_device_t *dev, lisa_audio_phase_compensation_t *compensation);
int lisa_audio_ioctl(lisa_device_t *dev, uint8_t cmd, void *arg);
```

## 使用示例

### 基本录音配置与启动

```c
#include "lisa_audio.h"

// 获取 Audio0 设备
lisa_device_t *audio0 = lisa_device_get_by_name("audio0");
if (!audio0) {
    return -1;
}

// 配置录音参数
lisa_audio_record_config_t record_config = {
    .format = {
        .sample_rate = LISA_AUDIO_RATE_16K,     // 16kHz 采样率
        .channels = LISA_AUDIO_CH_LEFT,         // 左声道
        .sample_bits = LISA_AUDIO_BIT_16,       // 16位采样
    },
    .gain = {
        .analog_gain = 0,                       // 模拟增益 0dB
        .digital_gain = 0,                      // 数字增益 0dB
    },
    .enable_hpf = true,                         // 启用高通滤波器
    .differential_input = false,                // 单端输入
    .buffer_count = 2,                          // 2个缓冲区
    .buffer_samples = 256,                      // 每个缓冲区256采样点
};

lisa_audio_record_config(audio0, &record_config);

// 启动录音
lisa_audio_record_start(audio0);
```

### 基本播放配置与数据写入

```c
// 配置播放参数
lisa_audio_play_config_t play_config = {
    .format = {
        .sample_rate = LISA_AUDIO_RATE_16K,     // 16kHz 采样率
        .channels = LISA_AUDIO_CH_STEREO,       // 立体声
        .sample_bits = LISA_AUDIO_BIT_16,       // 16位采样
    },
    .gain = {
        .analog_gain = 0,                       // 模拟增益 0dB
        .digital_gain = 0,                      // 数字增益 0dB
    },
    .buffer_count = 4,                          // 4个缓冲区
    .buffer_samples = 512,                      // 每个缓冲区512采样点
};

lisa_audio_play_config(audio0, &play_config);

// 启动播放
lisa_audio_play_start(audio0);

// 写入音频数据
int16_t audio_data[1024];  // 准备音频数据
// ... 填充 audio_data ...

int ret = lisa_audio_play_write(audio0, audio_data, 1024);
if (ret > 0) {
    // 成功写入 ret 个采样点
}

// 等待播放完成
lisa_audio_play_flush(audio0);

// 停止播放
lisa_audio_play_stop(audio0);
```

### 使用统一回调接收音频数据

```c
// 音频数据回调函数
void audio_event_callback(const lisa_audio_event_t *event, void *user_data)
{
    // 处理录音数据
    if (event->record_buffer && event->record_samples > 0) {
        const int16_t *record_data = (const int16_t *)event->record_buffer;
        // 处理录音数据...
        // 注意：回调在中断上下文中执行，应尽量简短快速
    }

    // 处理播放回声数据
    if (event->echo_buffer && event->echo_samples > 0) {
        const int16_t *echo_data = (const int16_t *)event->echo_buffer;
        // 处理回声数据...
    }
}

// 注册回调
lisa_audio_register_callback(audio0, audio_event_callback, NULL);

// 配置并启动录音和播放
lisa_audio_record_config(audio0, &record_config);
lisa_audio_play_config(audio0, &play_config);
lisa_audio_record_start(audio0);
lisa_audio_play_start(audio0);

// ... 运行一段时间 ...

// 停止并注销回调
lisa_audio_record_stop(audio0);
lisa_audio_play_stop(audio0);
lisa_audio_unregister_callback(audio0, audio_event_callback);
```

### 动态调整增益

```c
// 设置录音增益
lisa_audio_gain_t record_gain = {
    .analog_gain = 6,   // 模拟增益 +6dB
    .digital_gain = 3,  // 数字增益 +3dB
};
lisa_audio_record_set_gain(audio0, &record_gain);

// 设置播放增益
lisa_audio_gain_t play_gain = {
    .analog_gain = -3,  // 模拟增益 -3dB
    .digital_gain = 0,  // 数字增益 0dB
};
lisa_audio_play_set_gain(audio0, &play_gain);
```

### 相位补偿配置

```c
// 设置相位补偿（用于调整录音和播放之间的相位差）
lisa_audio_phase_compensation_t phase_comp = {
    .record_skip_samples = 0,   // 录音数据不丢弃
    .echo_skip_samples = 32,    // ECHO数据丢弃32个采样点
};
lisa_audio_set_phase_compensation(audio0, &phase_comp);

// 读取当前相位补偿配置
lisa_audio_phase_compensation_t current_comp;
lisa_audio_get_phase_compensation(audio0, &current_comp);
```

### 使用播放缓冲区零拷贝方式

```c
// 获取空闲播放缓冲区
void *buffer;
int buffer_size = lisa_audio_play_get_buffer(audio0, &buffer, 1000);  // 超时1秒
if (buffer_size > 0) {
    // 直接向缓冲区写入数据（零拷贝）
    int16_t *audio_buf = (int16_t *)buffer;
    for (int i = 0; i < buffer_size; i++) {
        audio_buf[i] = /* 生成音频数据 */;
    }

    // 提交数据
    lisa_audio_play_write(audio0, buffer, buffer_size);
}
```

### 暂停和恢复录音

```c
// 启动录音
lisa_audio_record_start(audio0);

// ... 录音一段时间 ...

// 暂停录音
lisa_audio_record_pause(audio0);

// ... 暂停期间不会产生录音数据 ...

// 恢复录音
lisa_audio_record_resume(audio0);

// 停止录音
lisa_audio_record_stop(audio0);
```

## 音频格式配置

### 采样率

支持的采样率：
- `LISA_AUDIO_RATE_8K` - 8kHz
- `LISA_AUDIO_RATE_16K` - 16kHz
- `LISA_AUDIO_RATE_24K` - 24kHz
- `LISA_AUDIO_RATE_32K` - 32kHz
- `LISA_AUDIO_RATE_48K` - 48kHz
- `LISA_AUDIO_RATE_96K` - 96kHz

### 通道配置

- `LISA_AUDIO_CH_LEFT` - 左声道
- `LISA_AUDIO_CH_RIGHT` - 右声道
- `LISA_AUDIO_CH_STEREO` - 立体声（左+右）

### 采样位深

- `LISA_AUDIO_BIT_16` - 16位采样 (2字节)
- `LISA_AUDIO_BIT_24` - 24位采样 (3字节)
- `LISA_AUDIO_BIT_32` - 32位采样 (4字节)

### 采样点计算

对于立体声音频，采样点数是**所有通道的总和**。例如：
- 单声道 256 个采样点 = 256 个样本
- 立体声 256 个采样点 = 每个声道 128 个样本

对于 16 位立体声：
- 缓冲区大小（字节） = 采样点数 × 2（字节/采样） × 2（声道） = 采样点数 × 4

## 状态定义

- `LISA_AUDIO_STATUS_IDLE` - 空闲状态
- `LISA_AUDIO_STATUS_RUNNING` - 运行中
- `LISA_AUDIO_STATUS_PAUSED` - 已暂停
- `LISA_AUDIO_STATUS_ERROR` - 错误状态

## 注意事项

1. **设备初始化**: 使用前需确保设备已正确初始化
2. **配置顺序**: 需先调用配置函数，再调用启动函数
3. **回调上下文**: 音频数据回调在中断上下文中执行，应尽量简短快速，不要执行耗时操作
4. **缓冲区管理**:
   - 录音缓冲区由驱动内部管理
   - 播放缓冲区数量影响延迟和缓冲深度
5. **采样点数**: 对于多通道音频，采样点数是所有通道的总和
6. **增益范围**: 增益单位为 dB，具体范围取决于硬件支持
7. **相位补偿**: 用于调整录音和播放之间的时间对齐，具体值需根据实际测试确定
8. **线程安全**: 驱动内部使用互斥锁保护关键操作

## 文件说明

- `lisa_audio.h` - 驱动头文件，包含所有 API 和类型定义
- `lisa_audio_arcs.c` - ARCS 平台适配实现
- `lisa_audio_internal.h` - 内部实现头文件
- `CMakeLists.txt` - 构建配置
- `Kconfig` - 配置选项
