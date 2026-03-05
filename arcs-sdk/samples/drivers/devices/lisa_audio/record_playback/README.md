# LISA Audio 录音后播放示例（单声道 + 回采信号可选）

## 功能说明

演示 LISA Audio 驱动的基础采集与播放流程：先录制 3 秒单声道音频，再把采集到的样本写回播放通道完成回放。

示例使用统一回调收集 Record 数据，可选启用回采信号 capture，用于验证 DAC 输出与回采信号的一致性。

### 新特性

- **统一回调派发**：`lisa_audio_register_callback()` 同时推送录音与回采信号数据，简化事件处理
- **双阶段流程**：先采集再播放，避免 Record/Play 同时占用硬件资源
- **可选回采信号收集**：构建带回采信号功能的固件时，可直接捕获 DAC 回采数据用于 AEC 调试

## 硬件连接

- **GPIOA28-A31**：麦克风输入（模拟或 PDM，依据硬件搭建选择）
- **DAC_OUT_L/R**：接功放、耳机或有源音箱

若仅验证驱动链路，可将麦克风与扬声器保持默认开发板连线，无需额外跳线。

## 使用场景

适用于需要验证“先录音、后播放”链路的入门场景，例如音频采集质量评估、录播基线测试以及回采信号数据抓取。

## 示例步骤

1. 调用 `lisa_device_get("audio0")` 获取统一音频设备句柄
2. 注册统一回调，分配录音和（可选）回采信号缓冲区
3. 配置 `lisa_audio_record_config()`，开启 HPF、差分输入与 Ping-Pong 缓冲后启动录音
4. 等待 3 秒采集完成并停止 Record
5. 配置 `lisa_audio_play_config()`，根据录得的样本数调用 `lisa_audio_play_write()` 写入数据
6. 使用 `lisa_audio_play_flush()` 等待播放完成，最后释放资源

## 编译运行

```bash
./build.sh -C -DBOARD=arcs_evb -S samples/drivers/devices/lisa_audio/record_playback
```

## 预期输出

**串口终端：**

```
[audio_sample] ========================================
[audio_sample] LISA Audio 驱动示例 - 录音后播音
[audio_sample] ========================================
[audio_sample] 音频设备获取成功
[audio_sample] 音频缓冲区分配成功 (96000 字节)
[audio_sample] 音频参数:
[audio_sample]   采样率: 16000 Hz
[audio_sample]   通道: 单声道
[audio_sample]   位宽: 16 位
[audio_sample]   录音时长: 3 秒
[audio_sample]   ADC 增益: 30/0 dB
[audio_sample]   DAC 增益: 6/-20 dB
[audio_sample]
[audio_sample] 步骤 1/2: 开始录音...
[audio_sample] 开始录音 (3 秒)...
[lisa_audio] ADC configured: rate=16000, gain=30/0 dB, buffers=25×256
[lisa_audio] ADC started
[audio_sample] 已录制 16000/48000 样本 (33%)
[audio_sample] 已录制 32000/48000 样本 (66%)
[audio_sample] 已录制 48000/48000 样本 (100%)
[lisa_audio] ADC stopped
[audio_sample] 录音完成! 共录制 48000 样本
[audio_sample]
[audio_sample] 等待 1 秒后开始播放...
[audio_sample]
[audio_sample] 步骤 2/2: 开始播放...
[audio_sample] 开始播放 (48000 样本)...
[lisa_audio] DAC configured: rate=16000, gain=6/-20 dB, buffers=12×256
[lisa_audio] DAC started
[audio_sample] 已播放 16000/48000 样本 (33%)
[audio_sample] 已播放 32000/48000 样本 (66%)
[audio_sample] 已播放 48000/48000 样本 (100%)
[lisa_audio] DAC stopped
[audio_sample] 播放完成! 共播放 48000 样本
[audio_sample]
[audio_sample] ========================================
[audio_sample] 示例完成! 录音和播放都成功
[audio_sample] ========================================
```

## 核心 API

| API | 说明 |
|-----|------|
| `lisa_device_get()` | 获取 `audio0` 设备句柄 |
| `lisa_audio_register_callback()` | 注册统一数据回调（Record + 回采信号） |
| `lisa_audio_record_config()` / `lisa_audio_record_start()` | 配置并启动录音 |
| `lisa_audio_play_config()` / `lisa_audio_play_start()` | 配置并启动播放 |
| `lisa_audio_play_write()` | 把录音样本写入播放队列 |
| `lisa_audio_play_flush()` / `lisa_audio_play_stop()` | 等待播放完成并停止 |

## 配置说明

### 关键宏

- **`RECORD_DURATION_SEC`**：录音时长，默认 3 秒，可改成更长的校准段
- **`SAMPLE_RATE`**：采样率，默认 `LISA_AUDIO_RATE_16K`，如需高保真可调至 48 kHz
- **`RECORD_ANALOG_GAIN` / `PLAY_ANALOG_GAIN`**：分别控制 ADC/DAC 模拟增益，单位 dB

### 必需配置

`samples/drivers/devices/lisa_audio/record_playback/prj.conf` 中需启用：

```kconfig
CONFIG_LISA_DEVICE=y
CONFIG_LISA_AUDIO_DEVICE=y
CONFIG_LISA_AUDIO_PLAY_ECHO_ENABLE=y   # 若需要采集回采信号，可打开
```

## 注意事项

1. **内存占用**：16kHz × 3 秒 × 16 位 ≈ 96 KB，确保外部 SRAM 充足
2. **互斥运行**：示例未开启 Record/Play 并行，务必等待录音停止后再启动播放
3. **增益调节**：若录音过小或播放失真，可在 `main.c` 中调整模拟/数字增益宏
4. **回采信号依赖**：启用回采信号功能时会占用 DAC DMA 通道 3，与立体声 Record 冲突
5. **资源释放**：退出前需调用 `lisa_audio_unregister_callback()` 并释放堆内存

## 故障排除

### 获取设备失败
- **现象**：日志输出 `获取 Audio 设备失败`
- **处理**：确认 `CONFIG_LISA_AUDIO_DEVICE=y` 且驱动初始化成功

### 内存分配失败
- **现象**：`audio_buffer` 或 `echo_buffer` 分配报错
- **处理**：缩短录音时长、降低采样率或扩大 `exram` 堆配置

### 录音/播放无声
- **现象**：录音样本全 0 或播放无输出
- **处理**：检查 GPIOA28-A31、DAC 输出连线，并适当提高增益

## 扩展示例

- **更长录音**：把 `RECORD_DURATION_SEC` 改为 5，更新 `TOTAL_SAMPLES`
- **更高采样率**：将 `SAMPLE_RATE` 改为 `LISA_AUDIO_RATE_48K` 并调整缓冲数量
- **回采信号回放**：启用 `CONFIG_LISA_AUDIO_PLAY_ECHO_ENABLE` 后，示例会额外播放回采数据

