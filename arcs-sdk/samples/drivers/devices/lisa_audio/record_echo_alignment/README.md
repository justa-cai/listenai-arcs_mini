# LISA AUDIO 录音与回采信号相位对齐示例（自动校准工具）

## 功能说明

用于自动测量并补偿录音信号与回采信号之间的相位偏移：播放内置正弦测试音频，同时读取 Record 与回采信号数据，借助互相关算法计算偏移，最后写入补偿参数验证校准效果。

## 硬件连接

- **麦克风**：接 GPIOA28-A31（模拟或 PDM，按硬件设计选择）
- **扬声器/耳机**：接 DAC 输出（功放或有源音箱）
- **回采信号通道**：启用 `CONFIG_LISA_AUDIO_PLAY_ECHO_ENABLE` 后自动使用 DAC 回采 DMA 通道

## 使用场景

适用于声学算法调试（AEC、回声对消前的延迟测量）、硬件回采信号链路验证、以及生产测试中对录播延迟的快速校准。

## 示例步骤

1. 初始化 `audio0` 设备并注册统一回调，准备音频缓冲区与测试音频表
2. 配置播放与录音参数，设置测试频率、搜索范围和跳过的前导噪声时间
3. 启动“测试 1/2”两轮测量：播放测试音频，同时采集 Record/回采信号数据，互相关得到偏移
4. 如果两次结果一致，计算补偿参数 `record_skip` / `echo_skip` 并写入驱动
5. 执行“测试 3”验证补偿是否把偏移逼近 0 个采样点
6. 打印结果，包含频率检测、偏移统计以及补偿后的结论

## 编译运行

```bash
./build.sh -C -DBOARD=arcs_evb -S samples/drivers/devices/lisa_audio/record_echo_alignment
```

## 预期输出

**串口终端：**

```
I/audio_sample    [1034:42:44.159 1 main] LISA Audio 驱动示例 - 录音与ECHO相位对齐校准
I/audio_sample    [1034:42:44.160 1 main] Audio 设备获取成功
I/audio_sample    [1034:42:44.160 1 main] 统一回调注册成功
I/audio_sample    [1034:42:44.160 1 main] 准备测试音频数据 (使用 audio_clip)...
I/audio_sample    [1034:42:44.160 1 main] 测试音频数据准备完成 (400.0Hz)
I/lisa_audio_record [1034:42:44.161 1 main] Record configured: rate=16000, gain=30/8 dB
I/lisa_audio_play [1034:42:44.162 1 main] Play configured: rate=16000, gain=0/-18 dB, buffers=12×256
I/lisa_audio_record [1034:42:44.162 1 main] Record started
I/audio_sample    [1034:42:44.162 1 main] 录音已启动
I/lisa_audio_play [1034:42:44.163 1 main] Play started
I/audio_sample    [1034:42:44.164 1 main] 播音已启动
I/audio_sample    [1034:42:44.263 1 main] 测试参数: 频率=400.0Hz, 周期=40采样点, 最大搜索偏移=40
I/audio_sample    [1034:42:44.264 1 main] 

======== 测试 1: 测量系统偏移 (第1次) ========
I/audio_sample    [1034:42:44.264 1 main] ========================================
I/audio_sample    [1034:42:44.264 1 main] 开始测试: 录音跳过0, ECHO跳过0, 最大搜索偏移40
I/audio_sample    [1034:42:44.264 1 main] ========================================
I/audio_sample    [1034:42:44.264 1 main] 相位补偿已设置
I/audio_sample    [1034:42:44.270 1 main] 开始写入测试音频...
I/audio_sample    [1034:42:47.106 1 main] 等待播放完成...
I/audio_sample    [1034:42:47.306 1 main] ========================================
I/audio_sample    [1034:42:47.306 1 main] 频率检测结果:
I/audio_sample    [1034:42:47.306 1 main]   目标频率: 400.0 Hz
I/audio_sample    [1034:42:47.306 1 main]   录音频率: 400.0 Hz [OK]
I/audio_sample    [1034:42:47.306 1 main]   ECHO频率: 400.0 Hz [OK]
I/audio_sample    [1034:42:47.307 1 main] 频率验证通过, 继续相位偏移计算
I/audio_sample    [1034:42:47.307 1 main] ========================================
I/audio_sample    [1034:42:47.589 1 main] ========================================
I/audio_sample    [1034:42:47.589 1 main] 相位分析结果:
I/audio_sample    [1034:42:47.589 1 main]   录音采样数: 48000
I/audio_sample    [1034:42:47.590 1 main]   ECHO采样数: 48000
I/audio_sample    [1034:42:47.590 1 main]   相位偏移: 11 个采样点
I/audio_sample    [1034:42:47.590 1 main]   时间偏移: 687.5 us
I/audio_sample    [1034:42:47.590 1 main]   结论: ECHO 落后于录音
I/audio_sample    [1034:42:47.590 1 main] ========================================
I/audio_sample    [1034:42:47.590 1 main] 第1次测量偏移: 11 个采样点
I/audio_sample    [1034:42:49.590 1 main] 

======== 测试 2: 测量系统偏移 (第2次, 验证一致性) ========
I/audio_sample    [1034:42:49.590 1 main] ========================================
I/audio_sample    [1034:42:49.590 1 main] 开始测试: 录音跳过0, ECHO跳过0, 最大搜索偏移40
I/audio_sample    [1034:42:49.590 1 main] ========================================
I/audio_sample    [1034:42:49.590 1 main] 相位补偿已设置
I/audio_sample    [1034:42:49.594 1 main] 开始写入测试音频...
I/audio_sample    [1034:42:52.434 1 main] 等待播放完成...
I/audio_sample    [1034:42:52.624 1 main] ========================================
I/audio_sample    [1034:42:52.624 1 main] 频率检测结果:
I/audio_sample    [1034:42:52.624 1 main]   目标频率: 400.0 Hz
I/audio_sample    [1034:42:52.624 1 main]   录音频率: 400.0 Hz [OK]
I/audio_sample    [1034:42:52.624 1 main]   ECHO频率: 400.0 Hz [OK]
I/audio_sample    [1034:42:52.625 1 main] 频率验证通过, 继续相位偏移计算
I/audio_sample    [1034:42:52.625 1 main] ========================================
I/audio_sample    [1034:42:52.907 1 main] ========================================
I/audio_sample    [1034:42:52.907 1 main] 相位分析结果:
I/audio_sample    [1034:42:52.909 1 main]   录音采样数: 48000
I/audio_sample    [1034:42:52.909 1 main]   ECHO采样数: 48000
I/audio_sample    [1034:42:52.909 1 main]   相位偏移: 11 个采样点
I/audio_sample    [1034:42:52.909 1 main]   时间偏移: 687.5 us
I/audio_sample    [1034:42:52.909 1 main]   结论: ECHO 落后于录音
I/audio_sample    [1034:42:52.909 1 main] ========================================
I/audio_sample    [1034:42:52.909 1 main] 第2次测量偏移: 11 个采样点
I/audio_sample    [1034:42:52.909 1 main] 两次测量结果一致, 系统偏移稳定.
I/audio_sample    [1034:42:54.909 1 main] 使用第1次测量结果 (11) 进行补偿
I/audio_sample    [1034:42:54.909 1 main] 

======== 测试 3: 自动对齐验证 (补偿: rec_skip=0, echo_skip=11) ========
I/audio_sample    [1034:42:54.909 1 main] ========================================
I/audio_sample    [1034:42:54.909 1 main] 开始测试: 录音跳过0, ECHO跳过11, 最大搜索偏移40
I/audio_sample    [1034:42:54.909 1 main] ========================================
I/audio_sample    [1034:42:54.909 1 main] 相位补偿已设置
I/audio_sample    [1034:42:54.913 1 main] 开始写入测试音频...
I/audio_sample    [1034:42:57.746 1 main] 等待播放完成...
I/audio_sample    [1034:42:57.936 1 main] ========================================
I/audio_sample    [1034:42:57.936 1 main] 频率检测结果:
I/audio_sample    [1034:42:57.936 1 main]   目标频率: 400.0 Hz
I/audio_sample    [1034:42:57.936 1 main]   录音频率: 400.0 Hz [OK]
I/audio_sample    [1034:42:57.937 1 main]   ECHO频率: 400.0 Hz [OK]
I/audio_sample    [1034:42:57.937 1 main] 频率验证通过, 继续相位偏移计算
I/audio_sample    [1034:42:57.937 1 main] ========================================
I/audio_sample    [1034:42:58.219 1 main] ========================================
I/audio_sample    [1034:42:58.219 1 main] 相位分析结果:
I/audio_sample    [1034:42:58.220 1 main]   录音采样数: 48000
I/audio_sample    [1034:42:58.220 1 main]   ECHO采样数: 48000
I/audio_sample    [1034:42:58.220 1 main]   相位偏移: 0 个采样点
I/audio_sample    [1034:42:58.220 1 main]   时间偏移: 0.0 us
I/audio_sample    [1034:42:58.220 1 main]   结论: 录音与 ECHO 相位对齐
I/audio_sample    [1034:42:58.220 1 main] ========================================
I/audio_sample    [1034:42:58.220 1 main] 自动对齐成功! 补偿后测量偏移: 0
I/audio_sample    [1034:42:58.420 1 main] 

========================================
I/audio_sample    [1034:42:58.420 1 main] 所有测试完成!

```

## 核心 API

| API | 说明 |
|-----|------|
| `lisa_audio_register_callback()` | 同时接收录音与回采信号数据流 |
| `lisa_audio_set_phase_compensation()` | 写入 record/echo skip 补偿值 |
| `lisa_audio_record_config()` / `lisa_audio_play_config()` | 配置录音和播放格式、增益、缓冲区 |
| `lisa_audio_record_start()` / `lisa_audio_play_start()` | 启动 Record/Play DMA |
| `lisa_audio_play_write()` | 将测试音频写入播放队列 |
| `lisa_audio_play_flush()` | 等待播放完成以保证测量稳定 |

## 关键代码

```c
/* 互相关搜索：限定 ±period_samples 提升效率 */
for (int offset = -(int)max_offset; offset <= (int)max_offset; offset++) {
    int64_t corr = 0;
    for (uint32_t i = 0; i < sample_count - max_offset; i++) {
        corr += (int32_t)record[i] * (int32_t)echo[i + offset];
    }
    if (corr > best_corr) {
        best_corr = corr;
        best_offset = offset;
    }
}

/* 周期归一化 */
while (best_offset > (int)period / 2) best_offset -= period;
while (best_offset < -(int)period / 2) best_offset += period;

lisa_audio_set_phase_compensation(audio_dev,
    &(lisa_audio_phase_comp_t){
        .record_skip_samples = best_offset < 0 ? (uint16_t)(-best_offset) : 0,
        .echo_skip_samples   = best_offset > 0 ? (uint16_t)(best_offset)  : 0,
    });
```

## 配置说明

### prj.conf 关键项

```kconfig
CONFIG_LISA_DEVICE=y
CONFIG_LISA_AUDIO_DEVICE=y
CONFIG_LISA_AUDIO_PLAY_ECHO_ENABLE=y
```

### 可调参数

- **`AUDIO_CLIP_FREQ`**：测试正弦波频率，默认 300/400/500 Hz 循环
- **`calibration_max_offset`**：搜索范围 = 1 × 周期，可根据场景加大
- **`skip_ms`**：跳过播放初期的启动噪声，避免干扰互相关

## 注意事项

1. **环境噪声**：建议在安静环境下测试，否则频率检测可能失败
2. **播放音量**：扬声器音量过低会导致回采信号幅度不足，互相关结果不稳定

## 自定义测试音频

```bash
cd samples/drivers/devices/lisa_audio/record_echo_alignment/utils
python3 generate_audio.py --frequency 500 --output ../src/audio_clip.h
```

- `--frequency`：推荐 200-1000 Hz
- `--amplitude`：默认 32000，必要时降低防止播放削波

