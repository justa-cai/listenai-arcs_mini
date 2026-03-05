# 单麦唤醒算法示例

## 功能说明

演示如何使用 ACOMP 唤醒算法组件实现语音唤醒功能,包括音频数据采集、回采处理、唤醒检测和结果输出。

本示例展示了单麦克风的唤醒算法应用,支持关键词识别、超时处理以及算法回声消除音频输出等完整的唤醒流程。

用户可以通过语音唤醒算法，算法检测到唤醒词或命令词后，会输出识别结果。

## 硬件连接

- **音频输入**: 单麦克风输入(MIC1),用ADC采样，采样率 16kHz
- **音频输出**: DAC单声道播放输出,同时软回采参考信号，采样率 16kHz
- **调试串口**: CP核的UART0,PA3引脚，波特率 921600,用于日志输出
- **调试串口**: AP核的UART2,PA4引脚，波特率 921600,用于日志输出

## 示例内容

1. 初始化 ACOMP 唤醒算法引擎
2. 配置音频输入输出设备(单通道录音: mic0 + ref0(软回采) + 单声道播放)
3. 创建音频数据流通道(M2R 和 R2M)
4. 采集音频数据并融合麦克风和回采信号(录音第二路作为参考回采)
5. 将融合后的音频数据送入唤醒算法引擎
6. 处理唤醒结果(关键词、角度、超时等事件)
7. 输出回声消除后的音频数据流

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

### 烧录固件

```bash
# 烧录 AP 核固件（Boot Core）
# 烧录地址：0x30000000
cskburn -s /dev/ttyACM0 -b 3000000 -C arcs 0x00 ./res/ap.bin    

# 烧录算法资源到 0x30200000 和 0x304d0000
cskburn -s /dev/ttyACM0 -b 3000000 -C arcs 0x200000 ./res/algo/algo.bin
cskburn -s /dev/ttyACM0 -b 3000000 -C arcs 0x4d0000 ./res/algo/wrap.json

# 烧录 CP 核固件
# 烧录地址：0x30050000
cskburn -s /dev/ttyACM0 -b 3000000 -C arcs 0x500000 ./build/arcs.bin
```

## 预期输出

### CP日志

**初始化阶段:**
```
********Arcs SDK 0.0.22 @ v0.0.23.temp.docs-41-g718db8e869c5********
Running on hart-id: 1
I/elog            [341:39:56.907 1 elog_async] EasyLogger V2.2.99 is initialize success.

I/lisa_audio_record [341:39:56.907 1 audio_dispatch] LISA Audio Record initialized

I/lisa_audio_play [341:39:56.907 1 audio_dispatch] LISA Audio Play initialized

I/main            [341:39:56.908 1 main] CP=======! Hard ID: 1

I/main            [341:39:56.909 1 main] ic_message_init done!

I/acomp_ipc       [341:39:58.910 1 rpc_client] [0]acomp remote dev index 1 name acomp.wakeup

I/acomp_wakeup    [341:39:58.910 1 main] acomp wakeup init enter

I/acomp_wakeup    [341:39:58.910 1 main] acomp wakeup dev index 1,name:acomp.wakeup

I/acomp_wakeup    [341:39:58.912 1 main] acomp wakeup init exit

I/acomp_wakeup    [341:39:58.912 1 main] acomp wakeup prepare enter

I/acomp_wakeup    [341:39:58.913 1 main] acomp wakeup prepare exit

I/wakeup          [341:39:58.913 1 main] acomp_wakeup_prepare ret:0

I/acomp_wakeup    [341:39:58.913 1 main] acomp wakeup start enter

I/acomp_wakeup    [341:39:59.482 1 main] acomp wakeup start exit

I/wakeup          [341:39:59.482 1 main] acomp_wakeup_start ret:0

I/acomp_wakeup    [341:39:59.483 1 main] acomp wakeup set algo mode enter, mode=0

I/acomp_wakeup    [341:39:59.484 1 main] acomp wakeup set algo mode exit

I/wakeup          [341:39:59.484 1 main] acomp_wakeup_set_algo_mode ret:0

INF:[acomp_stream_channel_create]Acomp stream channel 0x2883f754,0x20015b50,'stream.tocloud' created successfully ,vring phy addr=0x2880d660, num_descs=8, buffer_size=25600, direction=R2M, role=Master

I/acomp_wakeup    [341:39:59.488 1 main] acomp_wakeup_stream_ch_enable chn(stream.tocloud) index(1),desc(0x2880cd08)

INF:[acomp_stream_channel_create]Acomp stream channel 0x28844834,0x20015c38,'stream.mix2ch' created successfully ,vring phy addr=0x288437a0, num_descs=4, buffer_size=1024, direction=M2R, role=Master

I/acomp_wakeup    [341:39:59.490 1 main] acomp_wakeup_stream_ch_enable chn(stream.mix2ch) index(0),desc(0x2880ccf8)

I/wakeup_in       [341:39:59.491 1 main] Audio 设备获取成功

I/wakeup_in       [341:39:59.491 1 main] 统一回调注册成功

I/lisa_audio_record [341:39:59.491 1 main] Record configured: rate=16000, gain=30/0 dB

I/lisa_audio_play [341:39:59.492 1 main] Play configured: rate=16000, gain=0/-18 dB, buffers=12×256

I/lisa_audio_record [341:39:59.492 1 main] Record started

I/wakeup_in       [341:39:59.492 1 main] 录音已启动

I/lisa_audio_play [341:39:59.492 1 main] Play started

I/wakeup_in       [341:39:59.493 1 main] 播音已启动

I/wakeup_in       [341:39:59.493 1 main] 相位补偿已设置

```

**唤醒检测成功:**

- 用户可以用语音来唤醒算法，算法检测到唤醒词或命令词后，会输出识别结果

- 唤醒词：小聆小聆
- 命令词：无

> 注意: 算法需要被唤醒后，才可以识别命令词

```
I/wakeup          [341:40:05.778 1 rpc_client] WAKE(1): KEY=0(xiao3 ling2 xiao3 ling2), NCM=157, len:371

```

**唤醒超时:**

算法需要被唤醒后，才可以识别命令词，如长时间没有识别到命令词，会触发超时事件，用户需重新唤醒

```
I/wakeup          [341:40:44.745 1 rpc_client] wakeup timeout
```

### AP日志
```
********Arcs SDK 0.0.22 @ v0.0.23.temp.docs-41-g718db8e869c5********
Running on hart-id: 0
I/elog            [00:00:00.039 0 elog_async] EasyLogger V2.2.99 is initialize success.

AP Hard ID: 0
boot cp from address: 0x30500000
luna_version:0x3000200
I/wakeup          [00:00:00.055 0 main] sys_acomp_wakeup_init

I/components      [00:00:00.056 0 main] Reserved driver ID: 1

I/components      [00:00:00.056 0 main] acomp register driver index:1,name:acomp.wakeup

INF:[acomp_context_init 257]components context init success
I/wakeup          [00:00:02.056 0 rpc_async_serv] wakeup_create enter

I/wakeup          [00:00:02.056 0 rpc_async_serv] wakeup_create exit

I/wakeup          [00:00:02.058 0 acomp_workqueue] wakeup_prepare enter

I/wakeup          [00:00:02.058 0 acomp_workqueue] wakeup_prepare exit

I/wakeup          [00:00:02.059 0 acomp_workqueue] wakeup_start enter

I/wakeup_algo     [00:00:02.059 0 acomp_workqueue] cae_esr_mlp Resource, addr=0x30200000, size=1431296

I/wakeup_algo     [00:00:02.059 0 acomp_workqueue] Wrap json, addr=0x304d0000, size=763

JSON content (763 chars):
{"wrap":{"dev":"4002","trans":0,"param":[{"key":"wakesoffset","val":160},{"key":"wakeeoffset","val":-160}]},"cae":{"dist":30,"bits":16,"chns":{"mic":1,"ref":1},"wkmask":3,"param":[{"key":"mode","val":"0"},{"key":"textdep","val":1}]},"esr":{"param":[{"key":"prewstat","val":5},{"key":"prewthr","val":10000},{"key":"mode","val":1},{"key":"model","val":0},{"key":"1305","val":1},{"key":"1106","val":0},{"key":"2119","val":0},{"key":"1124","val":1},{"key":"1180","val":2},{"key":"1126","val":63},{"key":"1125","val":10},{"key":"1105","val":2},{"key":"1118","val":8},{"key":"1119","val":40},{"key":"1130","val":5},{"key":"1131","val":8},{"key":"1136","val":1}],"switchparam":[{"key":"2118","val_main":[500],"val_cmd":[0]},{"key":"1107","val_main":[2],"val_cmd":[2]}]}}
I/wakeup_algo     [00:00:02.069 0 acomp_workqueue]
ai_mem_t memap: json: 0x304d0000, psram: 0x0, size: 0, res: 0x30200000, size: 1431296

ai_get_info ret=0, len=790
info content (790 chars):
{"wrap":{"version":"600x.aiwrap.v1000.1.0.1 beta arcs_sp (Aug 12 2025 18:47:23)","inst_size":8608,"share_size":2560,"samples":{"in":256,"out":1280}},"cae":{"version":"arcs.cae.v1000.0.0.1 beta (Aug 12 2025 18:04:12)","memory":{"inst_ram_size":29184,"temp_ram_size":51200,"psram_size":0},"feature":{"mic_num":1,"ref_num":1,"local_out_channels":1,"aec_out_channels":0,"asr_out_channels":0,"nbit":16,"nsample":16000,"frame_num":5,"frame_size":256,"support_pre_wk_info":0,"support_wk_info":0}},"esr":{"version":"mini-esr-ARCS Tag5.1.2.1.2.5_26,Aug 12 2025,10:17:16","memory":{"share_mem_size":58848,"inst_mem_size":60288},"feature":{"input_pcm_channel":1,"input_pcm_bit":16,"input_pcm_frame_count":8,"input_pcm_sample_rate":16000,"max_main_word_count":1,"max_asr_word_count":1,"alog_type":26}}}
I/wakeup_algo     [00:00:02.206 0 acomp_workqueue] wrap_size: 8608

I/wakeup_algo     [00:00:02.206 0 acomp_workqueue] cae_psram: 0

I/wakeup_algo     [00:00:02.206 0 acomp_workqueue] inst_ram_size: 29184

I/wakeup_algo     [00:00:02.206 0 acomp_workqueue] temp_ram_size: 51200

I/wakeup_algo     [00:00:02.206 0 acomp_workqueue] esr_share_mem: 58848

I/wakeup_algo     [00:00:02.206 0 acomp_workqueue] esr_psram: 60288

I/wakeup_algo     [00:00:02.207 0 acomp_workqueue] Memory requirements - psram_size: 68928, share_size: 88064

I/wakeup_algo     [00:00:02.207 0 acomp_workqueue] algo_psram_buffer allocated at 0x28024e80, size=68928

Resource: mlp.bin,addr: 0x28035bc0, size: 1223424, compress: 0
Resource: main.bin,addr: 0x281606c0, size: 912, compress: 0
Resource: cmds.bin,addr: 0x28160a60, size: 6048, compress: 0
Resource: cae_ffw0_100.json,addr: 0x28162200, size: 141, compress: 0
Resource: cae_aes.bin,addr: 0x281622a0, size: 200000, compress: 0
I/wakeup_algo     [00:00:02.625 0 acomp_workqueue] ai_create success(used: psram=0x10d00, share=0x157e0)

I/wakeup_algo     [00:00:02.626 0 acomp_workqueue] esrstag: 1

I/wakeup_algo     [00:00:02.626 0 acomp_workqueue] algo_set_mode(0)

I/wakeup_algo     [00:00:02.627 0 acomp_workqueue] MODE=0

I/wakeup          [00:00:02.627 0 acomp_workqueue] wakeup_start exit

I/wakeup          [00:00:02.628 0 acomp_workqueue] wakeup_control enter

I/wakeup          [00:00:02.629 0 acomp_workqueue] wakeup_control: WAKEUP_IPC_CONTROL_SUBCMD_ALGO_MODE_SET

I/wakeup          [00:00:02.629 0 acomp_workqueue] Set algo mode: 0

I/wakeup_algo     [00:00:02.629 0 acomp_workqueue] Algo mode set to: WAKEUP

INF:[acomp_stream_channel_create]Acomp stream channel 0x28004a20,0x20051d34,'stream.tocloud' created successfully ,vring phy addr=0x2880d660, num_descs=8, buffer_size=25600, direction=R2M, role=Remote

INF:[acomp_stream_channel_create]Acomp stream channel 0x28004a50,0x20051da0,'stream.mix2ch' created successfully ,vring phy addr=0x288437a0, num_descs=4, buffer_size=1024, direction=M2R, role=Remote

I/wakeup_algo     [00:00:04.307 0 acomp_workqueue] counter_avg=119.07(M/S), cnt_part=190512871

I/wakeup_algo     [00:00:05.907 0 acomp_workqueue] counter_avg=125.59(M/S), cnt_part=200938084

I/wakeup_algo     [00:00:07.507 0 acomp_workqueue] counter_avg=125.71(M/S), cnt_part=201138800

I/wakeup_algo     [00:00:09.107 0 acomp_workqueue] counter_avg=125.68(M/S), cnt_part=201081353


```

## 核心 API

| API | 说明 |
|-----|------|
| `acomp_wakeup_init()` | 初始化唤醒算法组件 |
| `acomp_wakeup_prepare()` | 准备唤醒算法引擎 |
| `acomp_wakeup_start()` | 启动唤醒算法引擎 |
| `acomp_wakeup_set_algo_mode()` | 设置算法模式(唤醒模式) |
| `acomp_wakeup_add_callback()` | 注册唤醒事件回调函数 |
| `acomp_wakeup_stream_ch_enable()` | 使能音频数据流通道 |
| `acomp_wakeup_stream_tx_buffer_alloc()` | 分配发送缓冲区(M2R) |
| `acomp_wakeup_stream_tx_buffer_submit()` | 提交发送缓冲区 |
| `acomp_wakeup_stream_rx_buffer_get()` | 获取接收缓冲区(R2M) |
| `acomp_wakeup_stream_rx_buffer_release()` | 释放接收缓冲区 |
| `lisa_audio_register_callback()` | 注册音频设备回调 |
| `lisa_audio_record_config()` | 配置录音参数 |
| `lisa_audio_play_config()` | 配置播放参数 |
| `lisa_audio_record_start()` | 启动录音 |
| `lisa_audio_play_start()` | 启动播放 |

## 关键代码

### 唤醒算法事件回调

```c
static void wakeup_event_handler(uint32_t event, void *event_data, uint32_t event_data_len, void *priv)
{
    if (event & WAKEUP_CB_EVENT_ENGINE_RLT) {
        /* 唤醒结果 */
        LISA_LOGI(TAG, "wakeup result:%d,%s", event_data_len, (char *)event_data);
    } else if (event & WAKEUP_CB_EVENT_ENGINE_TIMEOUT) {
        /* 唤醒超时 */
        LISA_LOGI(TAG, "wakeup timeout");
    } else if (event & WAKEUP_CB_EVENT_ENGINE_ANGLE) {
        /* 角度信息 */
        for (int i = 0; i < event_data_len/sizeof(short); i++) {
            short angle = *((short*)event_data + i);
            LISA_LOGD(TAG, "wakeup angle[%d]: %d", i, angle);
        }
    }
}

static void wakeup_stream_handler(uint32_t event, void *event_data, uint32_t event_data_len, void *priv)
{
    
    if (event & WAKEUP_CB_EVENT_STREAM_UPDATE) {
        /* 算法音频输出事件 */
#ifdef WAKEUP_AUDIO_OUT_STREAM_KICK_AUTO
        if (rx_sem != NULL) {
            xSemaphoreGive(rx_sem);
        }
#endif
    }
}
```

### 获取麦克风和回采的音频

```c
/* 音频回调函数 - 收集麦克风和回采数据 */
static void unified_audio_callback(const lisa_audio_event_t *event, void *user_data)
{
    wakeup_in_msg_t *msg = psram_malloc(sizeof(wakeup_in_msg_t));
    msg->mic = (mic_in_t*)event->record_buffer;
    msg->ref = (ref_in_t*)(event->echo_buffer ? event->echo_buffer : zero_echo);
    msg->sample_cnt = event->record_samples;
    
    /* 提交到工作队列处理 */
    workqueue_submit(wakeup_in_wq, wakeup_in_wq_handler, msg);
}
```

### 麦克风和回采的音频融合后送入算法

```c
static void wakeup_in_wq_handler(void *para)
{
    uint8_t *buffer;
    uint32_t buf_size;
    uint16_t desc_idx;

    wakeup_in_msg_t *msg = (wakeup_in_msg_t*)para;

    LISA_LOGD(LOG_TAG, "wakeup_in_wq_handler, sample_cnt: %u", msg->sample_cnt);

    buffer = acomp_wakeup_stream_tx_buffer_alloc(WAKEUP_AUDIO_MIX2CH_STREAM_CH_INDEX, &buf_size, &desc_idx);

    if(buffer && buf_size > 0){
        stream_data_fusion((acomp_wakeup_audio_in_t*)buffer, msg->mic, msg->ref, msg->sample_cnt);
        acomp_wakeup_stream_tx_buffer_submit(WAKEUP_AUDIO_MIX2CH_STREAM_CH_INDEX, buffer, buf_size, desc_idx);
    }

    psram_free(msg);
}
```

### 用户对算法输出的音频进行处理

用户可以获取算法输出的音频数据，进行处理，如发送算法输出的回声消除的音频到云端进行大模型意图识别

```c
static void wakeup_out_task(void *pvParameters)
{
    uint8_t *buffer;
    uint32_t len;
    uint16_t desc_idx;
    int ret;

    while (1) {
#ifdef WAKEUP_AUDIO_OUT_STREAM_KICK_AUTO
        /* 自动触发模式：等待来自回调的信号量 */
        xSemaphoreTake(rx_sem, pdMS_TO_TICKS(50));
#else
        /* 手动触发模式：等待超时时间 */
        vTaskDelay(pdMS_TO_TICKS(5));
#endif
        /* 从 R2M 流中获取数据 */
        while (1) {
            buffer = acomp_wakeup_stream_rx_buffer_get(WAKEUP_AUDIO_OUT_STREAM_CH_INDEX, &len, &desc_idx);
            if (buffer != NULL && len > 0) {
                LISA_LOGD(TAG, "acomp_wakeup_stream_rx_buffer_get ok");

                // 2. 处理接收到的数据
                // 数据格式为 acomp_wakeup_audio_out_t 数组，5通道交织
                // {mic0, mic1, ref0, out1, out2} * N 个采样点
                // 其中 out1 为回声消除后的音频，可用于云端识别

                // process_output_audio(buffer, len);


                /* 释放缓冲区 */
                ret = acomp_wakeup_stream_rx_buffer_release(WAKEUP_AUDIO_OUT_STREAM_CH_INDEX, desc_idx, len, buffer);
                if (ret != ACOMP_ERR_OK) {
                    LISA_LOGW(TAG, "Failed to release buffer: %d", ret);
                }
            } else {
                /* 没有更多数据,跳出内层循环 */
                break;
            }
        }
    }
}
```

## 算法模式说明

### 单麦克风模式 (CONFIG_ACOMP_WAKEUP_ALGORITHM_TYPE_DUAL_MIC=n)

- **输入通道**: 1 个麦克风 + 1 个回采参考
- **数据格式**: 2 通道交织(mic1,  ref1)
- **特点**: 支持波束成形和角度检测,唤醒性能更好

### 音频参数配置

```c
#define SAMPLE_RATE         LISA_AUDIO_RATE_16K    /* 采样率 16kHz */
#define SAMPLE_BITS         LISA_AUDIO_BIT_16      /* 采样位宽 16bit */
#define RECORD_CHANNELS     LISA_AUDIO_CH_STEREO   /* 录音双声道 */
#define PLAY_CHANNELS       LISA_AUDIO_CH_LEFT     /* 播放单声道 */
#define BUFFER_SAMPLES      ACOMP_WAKEUP_AUDIO_INPUT_SAMPLE_CNT    /* 缓冲区采样点数 */
```

### 增益配置

```c
#define RECORD_ANALOG_GAIN     30      /* 录音模拟增益 16dB */
#define RECORD_DIGITAL_GAIN    0       /* 录音数字增益 8dB */
#define PLAY_ANALOG_GAIN       0       /* 播放模拟增益 6dB */
#define PLAY_DIGITAL_GAIN      -18     /* 播放数字增益 -18dB */
```

### 算法资源配置

在 `prj.conf` 中配置算法资源地址和大小:

```
CONFIG_ACOMP_WAKEUP_ALGORITHM_TYPE_DUAL_MIC=n
CONFIG_ACOMP_WAKEUP_RES_CAE_ESR_MLP_ADDRESS=0x30200000
CONFIG_ACOMP_WAKEUP_RES_CAE_ESR_MLP_LENGTH=1431296
CONFIG_ACOMP_WAKEUP_RES_AI_WRAP_ADDRESS=0x304D0000
CONFIG_ACOMP_WAKEUP_RES_AI_WRAP_LENGTH=763
```

### 堆内存配置

```
CONFIG_HEAP_SIZE=0x10000           /* 内部 SRAM 堆 64KB */
CONFIG_PSRAM_HEAP_SIZE=0x700000    /* PSRAM 堆 7MB */
```

## 数据流说明

### M2R 流(Master to Remote)

- **通道索引**: 0
- **通道名称**: "stream.mi x 1ch"
- **方向**: 应用核(CP) → 算法核(AP)
- **数据**: 融合后的麦克风和回采数据(输入给算法)
- **缓冲区大小**: 一次1024 字节(256 采样点 × 2 通道 × 2 字节)

### R2M 流(Remote to Master)

- **通道索引**: 1
- **通道名称**: "stream.tocloud"
- **方向**: 算法核(AP) → 应用核(CP)
- **数据**: 算法处理后的音频数据(3 通道)(mic1, ref, 回声消除后的音频（可用于意图识别）， 算法后端输入音频)
- **缓冲区大小**: 一次输出 4帧 或6帧

## 算法内存占用

单麦算法内存占用如下(粗略统计)， 详细内存分布见`memap.h`文件

|  MCU核心 |  内存类型 | 大小 | 备注 |
|---------|----------|----|-----|
|  AP |  FLASH | 1674KB   | 2个算法资源 + ap固件大小 + cp固件大小 |
|  AP |  PSRAM |  1550KB   | 算法实例用的PSRAM + 2个算法资源拷贝到PSRAM上 + 动态内存 |
|  AP |  SRAM  |  8KB | IPC共享内存 |
|  AP |  SRAM  |  62KB | AP的SRAM |
|  AP |  SRAM  |  290KB | 算法实例大小 |
|  AP |  SRAM  |  28KB | LUNA的*(.sharedmem.*) |
|  AP |  LUNASRAM |  64KB  | LUNA专用的SRAM大小 |
