# ACOMP Tuner 音频处理示例

## 功能说明

本示例演示 ARCS 双核架构下 ACOMP Tuner 的完整音频处理链路。CP 侧 sample 使用 `audio0` 采集麦克风 PCM 数据，通过 IPC 流通道发送到 AP 侧 Tuner 算法处理，再将处理后的 PCM 回传给 CP 侧本地播放。

示例默认工作在 `48kHz`、`16bit`、单声道模式，AP 侧基于 IFLYTEK 音频处理库提供均衡、动态范围压缩、限幅等能力，覆盖了 Tuner 初始化、双向 Stream 通道建立、实时送流和回放的完整流程。

## 硬件连接

- 无需额外连接 AP/CP，两核均运行在同一颗芯片内部
- `audio0`：使用开发板默认麦克风输入和扬声器/耳机输出路径
- 音频格式：默认 `48kHz`、`16bit`、单声道
- 调试串口：建议同时观察 AP、CP 两侧日志，确认 IPC 建链和音频处理状态

## 示例内容

1. CP 侧初始化 `ic_message`，等待 AP 固件启动完成
2. 获取 `audio0` 设备，创建工作队列和输出消费任务
3. 初始化 `acomp` 框架和 `acomp_tuner` 组件，注册状态与 Stream 更新回调
4. 创建 `tuner.in`（`M2R`）和 `tuner.out`（`R2M`）两个音频流通道
5. 配置采样率和音量，启动 AP 侧 Tuner 算法
6. 启动本地播放和录音，将录音回调中的 PCM 数据异步送到 AP 侧处理
7. 在输出任务中等待 `TUNER_CB_EVENT_STREAM_UPDATE` 事件，持续取回 AP 处理后的 PCM 并播放
8. 运行约 30 秒后停止录音、播放和 Tuner，并完成资源清理

### AP/CP 分工

| 核心 | 主要职责 |
|------|----------|
| AP 侧 | 运行 Tuner 算法、接收 CP 侧送来的原始 PCM、完成音频处理并回传处理结果 |
| CP 侧 | 采集麦克风数据、建立 IPC Stream、送流到 AP、消费处理结果并本地播放 |

### AP 侧配套说明

- 默认配套 AP 固件镜像为 `res/ap.bin`
- 如需联调 AP 侧源码，请参考 `../apps/remote-ap/acomp/tuner/tuner.c` 和 `../apps/remote-ap/acomp/tuner/algo_tuner_params.c`
- 自行编译 AP 固件时，请确保启用 `CONFIG_ACOMP_TUNER=y`

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

本 sample 默认编译生成 CP 固件 `build/arcs.bin`。AP 侧默认使用目录内提供的 `res/ap.bin`；如需联调 AP 侧源码，请在 `../apps/remote-ap` 中单独构建对应固件。

## 烧录

本示例需要准备 Boot 固件、AP 固件和 CP 固件。请按下表地址烧录：

| 镜像 | 介质 | 烧录地址 | 说明 |
|------|------|----------|------|
| `boot.bin` | Flash | `0x000000` | Boot 固件 |
| `ap.bin` | Flash | `0x200000` | AP 固件，内置 Tuner 算法服务 |
| `arcs.bin` | Flash | `0x830000` | 本 sample 生成的 CP 固件 |

> 地址信息与 `res/res_info.md` 保持一致。

### 烧录顺序建议

1. 烧录 `boot.bin`
2. 烧录 `ap.bin`
3. 烧录 `arcs.bin`

### 参考烧录命令

```bash
cskburn -s /dev/ttyACM0 -b 3000000 -C arcs 0x000000 ./res/boot.bin
cskburn -s /dev/ttyACM0 -b 3000000 -C arcs 0x200000 ./res/ap.bin
cskburn -s /dev/ttyACM0 -b 3000000 -C arcs 0x830000 ./build/arcs.bin
```

如果使用自编译的 AP 固件，请将第二条命令中的 `./res/ap.bin` 替换为实际产物路径。

## 预期输出

### CP 侧日志

```text
I/tuner_sample    === Tuner Sample Started ===
I/tuner_sample    Audio pipeline: Mic -> CP -> AP(Tuner) -> CP -> Speaker
I/tuner_sample    IPC initialized
I/tuner_sample    Audio device ready
I/tuner_sample    Workqueue created
I/tuner_sample    ACOMP initialized
I/tuner_sample    Tuner initialized
I/tuner_sample    Input stream channel (M2R) enabled
I/tuner_sample    Output stream channel (R2M) enabled
I/tuner_sample    Tuner started and enabled
I/tuner_sample    Audio playback started
I/tuner_sample    Audio recording started
I/tuner_sample    === Audio pipeline running ===
I/tuner_sample    === Tuner Sample Completed, total frames=... ===
```

### AP 侧日志

```text
I/tuner           sys_acomp_tuner_init
I/tuner           tuner_create enter
I/tuner           tuner_algo_init done, sr=48000
INF:[acomp_stream_channel_create] ... 'tuner.in' ... direction=M2R ...
INF:[acomp_stream_channel_create] ... 'tuner.out' ... direction=R2M ...
I/tuner           SET_SAMPLERATE: 48000
I/tuner           SET_VOLUME: 1.000
I/tuner           tuner_start enter
I/tuner           tuner_start exit
I/tuner           ENABLE: 1
```

## 核心 API

| API | 说明 |
|-----|------|
| `acomp_tuner_init()` | 初始化 Tuner 客户端并绑定远端 `acomp.tuner` 设备 |
| `acomp_tuner_add_callback()` | 注册状态和 Stream 更新回调 |
| `acomp_tuner_stream_ch_enable()` | 创建并使能 `M2R` / `R2M` 音频流通道 |
| `acomp_tuner_set_samplerate()` | 配置算法处理采样率 |
| `acomp_tuner_set_volume()` | 配置算法音量系数 |
| `acomp_tuner_start()` / `acomp_tuner_enable()` | 启动并使能 AP 侧 Tuner 算法 |
| `acomp_tuner_stream_tx_buffer_alloc()` / `acomp_tuner_stream_tx_buffer_submit()` | 分配并提交发送到 AP 的 PCM 缓冲 |
| `acomp_tuner_stream_kick()` | 触发 AP 侧处理当前输入数据 |
| `acomp_tuner_stream_rx_buffer_get()` / `acomp_tuner_stream_rx_buffer_release()` | 获取并释放 AP 回传的处理后 PCM |
| `lisa_audio_register_callback()` | 注册录音回调，将 PCM 数据送入 Tuner 链路 |

## 功能详解

### 音频处理链路

```text
CP 核 (本示例)                          AP 核 (remote-ap)
+-----------------+                     +-----------------+
| lisa_audio      |                     | tuner algo      |
| (mic record)    |                     | (EQ/DRC/Limiter)|
|        |        |   M2R stream        |        ^        |
|        v        | ==================> |        |        |
| tuner TX stream |   (原始 PCM)        | tuner RX stream |
|                 |                     |        |        |
| tuner RX stream |   R2M stream        |        v        |
|        |        | <================== | tuner TX stream |
|        v        |   (处理后 PCM)      |                 |
| lisa_audio      |                     +-----------------+
| (speaker play)  |
+-----------------+
```

`M2R` 通道负责把 CP 侧采集到的原始 PCM 送到 AP；`R2M` 通道负责把 AP 侧处理后的 PCM 回传给 CP。CP 侧通过工作队列避免在录音回调中直接执行耗时送流操作，并通过独立输出任务及时消费 AP 回传数据，保证链路实时性。

## 关键代码

### 初始化 Tuner 与回调

```c
acomp_init();

ret = acomp_tuner_init();
ret = acomp_tuner_add_callback(TUNER_CB_EVENT_STATUS |
                               TUNER_CB_EVENT_STREAM_UPDATE,
                               tuner_event_callback, NULL);
```

### 创建双向 Stream 通道

```c
acomp_stream_chn_create_desc_t input_desc = {
    .cname = "tuner.in",
    .direction = ACOMP_STREAM_DIRECTION_M2R,
    .index = TUNER_CHN_INPUT,
    .buffer_size = TUNER_STREAM_BUFFER_SIZE,
    .num_descs = TUNER_STREAM_NUM_DESCS,
    .kick_policy = 1,
};
acomp_tuner_stream_ch_enable(TUNER_CHN_INPUT, &input_desc);

acomp_stream_chn_create_desc_t output_desc = {
    .cname = "tuner.out",
    .direction = ACOMP_STREAM_DIRECTION_R2M,
    .index = TUNER_CHN_OUTPUT,
    .buffer_size = TUNER_STREAM_BUFFER_SIZE,
    .num_descs = TUNER_STREAM_NUM_DESCS,
    .kick_policy = 1,
};
acomp_tuner_stream_ch_enable(TUNER_CHN_OUTPUT, &output_desc);
```

### 录音数据送入 AP

```c
static void audio_record_callback(const lisa_audio_event_t *event, void *user_data)
{
    tuner_audio_msg_t *msg = psram_malloc(sizeof(tuner_audio_msg_t));

    msg->data = (const int16_t *)event->record_buffer;
    msg->samples = event->record_samples;
    workqueue_submit(tuner_wq, tuner_wq_handler, msg);
}

static void tuner_wq_handler(void *para)
{
    tx_buf = acomp_tuner_stream_tx_buffer_alloc(TUNER_CHN_INPUT, &tx_len, &tx_desc_idx);
    memcpy(tx_buf, msg->data, copy_len);
    acomp_tuner_stream_tx_buffer_submit(TUNER_CHN_INPUT, tx_buf, copy_len, tx_desc_idx);
    acomp_tuner_stream_kick(TUNER_CHN_INPUT);
}
```

### 消费 AP 返回音频并播放

```c
rx_buf = acomp_tuner_stream_rx_buffer_get(TUNER_CHN_OUTPUT, &rx_len, &rx_desc_idx);
if (rx_buf != NULL && rx_len > 0) {
    uint32_t samples = rx_len / sizeof(int16_t);
    lisa_audio_play_write(audio_dev, rx_buf, samples);
    acomp_tuner_stream_rx_buffer_release(TUNER_CHN_OUTPUT, rx_desc_idx, rx_len, rx_buf);
}
```

## 配置说明

本示例的关键配置位于 `prj.conf`：

| 配置项 | 说明 |
|--------|------|
| `CONFIG_ACOMP=y` | 启用 `acomp` 框架 |
| `CONFIG_ACOMP_TUNER=y` | 启用 CP 侧 Tuner 组件 |
| `CONFIG_LISA_AUDIO_DEVICE=y` | 启用 `audio0` 音频设备 |
| `CONFIG_LISA_AUDIO_PLAY_ECHO_ENABLE=y` | 启用播放侧回采能力 |
| `CONFIG_WORK_QUEUE=y` | 通过工作队列异步处理录音回调送流 |
| `CONFIG_PSRAM_HEAP_SIZE=0x400000` | 为音频缓冲和动态消息分配提供 PSRAM 空间 |
| `CONFIG_MEM_FLASH_BASE=0x30830000` | 对应 CP 固件烧录偏移 `0x830000` |

## 注意事项

1. Boot、AP、CP 固件请保持同一套版本，避免 IPC 协议或流通道参数不匹配
2. 如需自行编译 AP 固件，必须确保 `CONFIG_ACOMP_TUNER=y` 已启用
3. `audio0` 需要支持 `48kHz`、`16bit`、单声道录音和播放路径
4. AP 回传的音频缓冲需要及时调用 `acomp_tuner_stream_rx_buffer_release()` 释放，否则会导致回放链路阻塞
5. 正常运行时不应持续出现 `Record queue full` 或 `frame dropped` 日志；若出现，通常表示录音送流或回放消费链路没有及时处理数据
6. 本 sample 默认运行约 30 秒后退出，如需长时间运行，可调整 `src/main.c` 中的延时逻辑

## 相关文档

- `res/res_info.md`：镜像烧录地址说明
- `src/main.c`：CP 侧音频采集、送流和回放控制入口
- `../apps/remote-ap/acomp/tuner/tuner.c`：AP 侧 Tuner 服务实现入口

```{toctree}
:hidden:

res/res_info.md
```
