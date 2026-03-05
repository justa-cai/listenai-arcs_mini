# 双麦唤醒算法示例

## 功能说明

演示如何使用 ACOMP 唤醒算法组件实现语音唤醒功能,包括音频数据采集、回采处理、唤醒检测和结果输出。

本示例展示了双麦克风阵列的唤醒算法应用,支持关键词识别、角度检测、超时处理以及算法回声消除音频输出等完整的唤醒流程。

用户可以通过语音唤醒算法，算法检测到唤醒词或命令词后，会输出识别结果。

## 硬件连接

- **音频输入**: 双麦克风输入(MIC1, MIC2),用ADC采样，采样率 16kHz
- **音频输出**: DAC单声道播放输出,同时回采参考信号，采样率 16kHz
- **调试串口**: CP核的UART0,PA3引脚，波特率 921600,用于日志输出
- **调试串口**: AP核的UART1,PA4引脚，波特率 921600,用于日志输出

## 示例内容

1. 初始化 ACOMP 唤醒算法引擎
2. 配置音频输入输出设备(双麦克风录音 + 单声道播放(同时获取回采))
3. 创建音频数据流通道(M2R 和 R2M)
4. 采集音频数据并融合麦克风和回采信号
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
********Arcs SDK 0.0.22 @ v0.0.1-691-gfaa226ea04ba********
Running on hart-id: 1
I/elog            [341:39:56.905 1 elog_async] EasyLogger V2.2.99 is initialize success.
I/lisa_audio_record [341:39:56.906 1 audio_dispatch] LISA Audio Record initialized
I/lisa_audio_play [341:39:56.906 1 audio_dispatch] LISA Audio Play initialized
I/main            [341:39:56.906 1 main] CP=======! Hard ID: 1
I/main            [341:39:56.908 1 main] ic_message_init done!
I/acomp_ipc       [341:39:58.908 1 rpc_client] [0]acomp remote dev index 2 name acomp.logger
I/acomp_ipc       [341:39:58.908 1 rpc_client] [1]acomp remote dev index 1 name acomp.wakeup
I/acomp_wakeup    [341:39:58.908 1 main] acomp wakeup init enter
I/acomp_wakeup    [341:39:58.908 1 main] acomp wakeup dev index 1,name:acomp.wakeup
I/acomp_wakeup    [341:39:58.911 1 main] acomp wakeup init exit
I/acomp_wakeup    [341:39:58.911 1 main] acomp wakeup prepare enter
I/acomp_wakeup    [341:39:58.914 1 main] acomp wakeup prepare exit
I/wakeup          [341:39:58.914 1 main] acomp_wakeup_prepare ret:0
I/acomp_wakeup    [341:39:58.914 1 main] acomp wakeup start enter
I/acomp_wakeup    [341:40:00.090 1 main] acomp wakeup start exit
I/wakeup          [341:40:00.090 1 main] acomp_wakeup_start ret:0
I/acomp_wakeup    [341:40:00.090 1 main] acomp wakeup set algo mode enter, mode=0
I/acomp_wakeup    [341:40:00.095 1 main] acomp wakeup set algo mode exit
I/wakeup          [341:40:00.095 1 main] acomp_wakeup_set_algo_mode ret:0
INF:[acomp_stream_channel_create]Acomp stream channel 0x2882b774,0x20015b50,'stream.tocloud' created successfully ,vring phy addr=0x2880d680, num_descs=8, buffer_size=15360, direction=R2M, role=Master

I/acomp_wakeup    [341:40:00.100 1 main] acomp_wakeup_stream_ch_enable chn(stream.tocloud) index(1),desc(0x2880cd08)
INF:[acomp_stream_channel_create]Acomp stream channel 0x28831854,0x20015c38,'stream.mix2ch' created successfully ,vring phy addr=0x2882f7c0, num_descs=4, buffer_size=2048, direction=M2R, role=Master

I/acomp_wakeup    [341:40:00.104 1 main] acomp_wakeup_stream_ch_enable chn(stream.mix2ch) index(0),desc(0x2880ccf8)
I/wakeup_in       [341:40:00.105 1 main] Audio 设备获取成功
I/wakeup_in       [341:40:00.105 1 main] 统一回调注册成功
I/lisa_audio_record [341:40:00.105 1 main] Record configured: rate=16000, gain=30/0 dB
I/lisa_audio_play [341:40:00.105 1 main] Play configured: rate=16000, gain=0/-18 dB, buffers=12×256
I/lisa_audio_record [341:40:00.106 1 main] Record started
I/wakeup_in       [341:40:00.106 1 main] 录音已启动
I/lisa_audio_play [341:40:00.106 1 main] Play started
I/wakeup_in       [341:40:00.106 1 main] 播音已启动
I/wakeup_in       [341:40:00.106 1 main] 相位补偿已设置
```

**唤醒检测成功:**

- 用户可以用语音来唤醒算法，算法检测到唤醒词或命令词后，会输出识别结果

- 唤醒词：你好小奥，小奥同学
- 命令词：开浴霸，关浴霸，开恒温暖，关恒温暖，打开自然风，关闭自然风，具体命令词见res目录下minglingci.txt

> 注意: 算法需要被唤醒后，才可以识别命令词

```
I/wakeup          [341:40:05.778 1 rpc_client] WAKE(1): KEY=1(ni hao xiao ao), NCM=876, len:278
I/wakeup          [341:40:14.339 1 rpc_client] WAKE(0): KEY=1(kai yu ba), NCM=380, len:350
I/wakeup          [341:40:18.098 1 rpc_client] WAKE(1): KEY=1(ni hao xiao ao), NCM=848, len:278
I/wakeup          [341:40:26.001 1 rpc_client] WAKE(1): KEY=1(ni hao xiao ao), NCM=786, len:279
I/wakeup          [341:40:28.677 1 rpc_client] WAKE(0): KEY=2(guan yu ba), NCM=711, len:352
```

**唤醒超时:**

算法需要被唤醒后，才可以识别命令词，如长时间没有识别到命令词，会触发超时事件，用户需重新唤醒

```
I/wakeup          [341:40:44.745 1 rpc_client] wakeup timeout
```

### AP日志
```
********Arcs SDK@test_deploy-163-gb9c7998b-@v0.0.22********
Running on hart-id: 0
AP Hard ID: 0
boot cp from address: 0x30500000
luna_version:0x3000200
I/wakeup          [00:00:00.058 0 main] sys_acomp_wakeup_init
I/components      [00:00:00.060 0 main] Reserved driver ID: 1
I/components      [00:00:00.060 0 main] acomp register driver index:1,name:acomp.wakeup
I/logger          [00:00:00.061 0 main] sys_acomp_logger_init
I/components      [00:00:00.062 0 main] Reserved driver ID: 2
I/components      [00:00:00.063 0 main] acomp register driver index:2,name:acomp.logger
INF:[acomp_context_init 257]components context init success
I/wakeup          [00:00:02.059 0 rpc_async_serv] wakeup_create enter
I/wakeup          [00:00:02.060 0 rpc_async_serv] wakeup_create exit
I/wakeup          [00:00:02.061 0 acomp_workqueue] wakeup_prepare enter
I/wakeup          [00:00:02.062 0 a_workqueue] wakeup_prepare exit
I/wakeup          [00:00:02.064 0 acomp_workqueue] wakeup_start enter
I/wakeup_algo     [00:00:02.065 0 acomp_workqueue] cae_esr_mlp Resource, addr=0x30200000, size=2926656
I/wakeup_algo     [00:00:02.066 0 acomp_workqueue] Wrap json, addr=0x304d0000, size=1008
JSON content (1008 chars):
{"wrap":{"dev":"4002","adc":0,"trans":1,"param":[{"key":"wakesoffset","val":160},{"key":"wakeeoffset","val":-160}]},"cae":{"dist":30,"bits":16,"chns":{"mic":2,"ref":2,"out":9},"wkmask":3,"param":[{"key":"mode","val":"0"},{"key":"textdep","val":1},{"key":"16386","val":100},{"key":"16387","val":150},{"key":"32770","val":0},{"key":"8","val":0},{"key":"9","val":0}]},"esr":{"param":[{"key":"prewstat","val":2},{"key":"prewthr","val":400},{"key":"mode","val":1},{"key":"model","val":0},{"key":"1105","val":0},{"key":"1107","val":0},{"key":"1138","val":1},{"key":"1106","val":0},{"key":"2119","val":0},{"key":"1124","val":1},{"key":"1180","val":2},{"key":"1126","val":63},{"key":"1125","val":10},{"key":"1118","val":8},{"key":"1119","val":40},{"key":"1130","val":5},{"key":"1131","val":40},{"key":"1132","val":5},{"key":"1133","val":8},{"key":"1134","val":-1},{"key":"1136","val":0},{"key":"1408","val":2},{"key":"1409","val":4},{"key":"1129","val":8}],"switchparam":[{"key":"2118","val_main":500,"val_cmd":0}]}}
I/wakeup_algo     [00:00:02.079 0 acomp_workqueue] 
ai_mem_t memap: json: 0x304d0000, psram: 0x0, size: 0, res: 0x30200000, size: 2926656
ai_get_info ret=0, len=795
info content (795 chars):
{"wrap":{"version":"600x.aiwrap.v1000.1.2.1 beta (Nov 21 2025 15:58:50)","inst_size":31712,"share_size":15360,"samples":{"in":256,"out":1280}},"cae":{"version":"5001.cae.v1000.1.2.1 beta (Nov 13 2025 15:08:05)","memory":{"inst_ram_size":47904,"temp_ram_size":310496,"psram_size":1451168},"feature":{"mic_num":2,"ref_num":2,"local_out_channels":5,"aec_out_channels":0,"asr_out_channels":1,"nbit":16,"nsample":16000,"frame_num":5,"frame_size":256,"support_pre_wk_info":1,"support_wk_info":1}},"esr":{"version":"mini-esr-ARCS Tag5.0.0.0.2.1_27,Sep  9 2025,17:48:46","memory":{"share_mem_size":240672,"inst_mem_size":251232},"feature":{"input_pcm_channel":5,"input_pcm_bit":16,"input_pcm_frame_count":8,"input_pcm_sample_rate":16000,"max_main_word_count":2,"max_asr_word_count":107,"alog_type":27}}}
I/wakeup_algo     [00:00:02.403 0 acomp_workqueue] wrap_size: 31712
I/wakeup_algo     [00:00:02.404 0 acomp_workqueue] cae_psram: 1451168
I/wakeup_algo     [00:00:02.405 0 acomp_workqueue] inst_ram_size: 47904
I/wakeup_algo     [00:00:02.406 0 acomp_workqueue] temp_ram_size: 310496
I/wakeup_algo     [00:00:02.407 0 acomp_workqueue] esr_share_mem: 240672
I/wakeup_algo     [00:00:02.407 0 acomp_workqueue] esr_psram: 251232
I/wakeup_algo     [00:00:02.409 0 acomp_workqueue] Memory requirements - psram_size: 1734144, share_size: 358432
I/wakeup_algo     [00:00:02.410 0 acomp_workqueue] algo_psram_buffer allocated at 0x28021b00, size=1734144
Resource: arcs_cldnn_aes_1024_d1_meidi7ci_20250714.bin,addr: 0x281c9100, size: 247744, compress: 0
Resource: arcs_cldnn_mctd_1024_d5d0_meidi7ci_20250710.bin,addr: 0x282058c0, size: 345408, compress: 0
Resource: arcs_cldnn_sp_1024_d4d0_20250714.bin,addr: 0x28259e00, size: 345024, compress: 0
Resource: cae_ffw0_501.json,addr: 0x282ae1c0, size: 141, compress: 0
Resource: mlp_main.bin,addr: 0x282ae260, size: 616864, compress: 0
Resource: mlp_mlc.bin,addr: 0x28344c00, size: 1299680, compress: 0
Resource: filler_keywords_main.bin,addr: 0x284820e0, size: 608, compress: 0
Resource: filler_keywords_main.bin,addr: 0x28482340, size: 608, compress: 0
Resource: filler_keywords_minglingci.bin,addr: 0x284825a0, size: 34400, compress: 0
Resource: filler_keywords_minglingci.bin,addr: 0x2848ac00, size: 34400, compress: 0
I/wakeup_algo     [00:00:03.226 0 acomp_workqueue] ai_create success(used: psram=0x1a75e0, share=0x57800)
I/wakeup_algo     [00:00:03.228 0 acomp_workqueue] esrstag: 1
I/wakeup_algo     [00:00:03.229 0 acomp_workqueue] algo_set_mode(0)
I/wakeup_algo     [00:00:03.238 0 acomp_workqueue] MODE=0
I/wakeup          [00:00:03.239 0 acomp_workqueue] wakeup_start exit
I/wakeup          [00:00:03.241 0 acomp_workqueue] wakeup_control enter
I/wakeup          [00:00:03.242 0 acomp_workqueue] wakeup_control: WAKEUP_IPC_CONTROL_SUBCMD_ALGO_MODE_SET
I/wakeup          [00:00:03.243 0 acomp_workqueue] Set algo mode: 0
I/wakeup_algo     [00:00:03.244 0 acomp_workqueue] Algo mode set to: WAKEUP
INF:[acomp_stream_channel_create]Acomp stream channel 0x280016a0,0x20052180,'stream.tocloud' created successfully ,vring phy addr=0x2880d680, num_descs=8, buffer_size=25600, direction=R2M, role=Remote

INF:[acomp_stream_channel_create]Acomp stream channel 0x280016d0,0x200521ec,'stream.mix2ch' created successfully ,vring phy addr=0x288437c0, num_descs=4, buffer_size=2048, direction=M2R, role=Remote

I/wakeup_algo     [00:00:04.971 0 acomp_workqueue] counter_avg=204.51(M/S), cnt_part=327218363
E/wakeup_algo     [00:00:05.761 0 acomp_workqueue] [wrap]error: code=1; msg={"rlt":[{"istart":43,"iresid":1,"iduration":13,"nfillerscore":1333,"nkeywordscore":12970,"ncm":919,"ncmThreshold":400,"keyword":"ni hao xiao ao","intent":"ni hao xiao ao","bMain":1,"bAbsorb":0}]}
I/wakeup_algo     [00:00:06.019 0 acomp_workqueue] [wrap]wakeup: chn=1; result={"rlt":[{"istart":43,"iresid":1,"iduration":20,"nfillerscore":1759,"nkeywordscore":18812,"ncm":830,"ncmThreshold":400,"keyword":"ni hao xiao ao","intent":"ni hao xiao ao","nDelayFrame":0,"nThrowFrame":62,"decId":0,"branch":0,"wakeUpType":0,"bMain":1,"bAbsorb":0,"iframe":65}]}
I/wakeup          [00:00:06.023 0 acomp_workqueue] wakeup_algo_result_cb: data_len(276)
I/wakeup_algo     [00:00:06.027 0 acomp_workqueue] WAKE(1): CHN=1, KEY=1(ni hao xiao ao), NCM=830, len:276
I/wakeup_algo     [00:00:06.113 0 acomp_workqueue] algo_set_mode(1)
I/wakeup_algo     [00:00:06.123 0 acomp_workqueue] MODE=1
I/wakeup_algo     [00:00:06.577 0 acomp_workqueue] counter_avg=219.85(M/S), cnt_part=351752029
I/wakeup_algo     [00:00:08.177 0 acomp_workqueue] counter_avg=220.23(M/S), cnt_part=352369137
I/wakeup_algo     [00:00:09.778 0 acomp_workqueue] counter_avg=220.39(M/S), cnt_part=352616103
I/wakeup_algo     [00:00:09.938 0 acomp_workqueue] [wrap]wakeup: chn=1; result={"rlt":[{"istart":77,"iresid":2,"iduration":20,"nfillerscore":0,"nkeywordscore":0,"ncm":502,"ncmThreshold":300,"keyword":"guan yu ba","intent":"guan yu ba","nRltIdx":1,"nDelayFrame":5,"nThrowFrame":96,"decId":0,"branch":0,"wakeUpType":0,"VadGap":0,"nE2eIntervalFrame":0,"nE2eNodeFrame":0,"nStartStateThreshold":0,"bMain":0,"bAbsorb":0,"iframe":164}]}
I/wakeup          [00:00:09.942 0 acomp_workqueue] wakeup_algo_result_cb: data_len(350)
I/wakeup_algo     [00:00:09.947 0 acomp_workqueue] WAKE(0): CHN=1, KEY=2(guan yu ba), NCM=502, len:350
I/wakeup_algo     [00:00:11.380 0 acomp_workqueue] counter_avg=222.83(M/S), cnt_part=356531282
I/wakeup_algo     [00:00:12.977 0 acomp_workqueue] counter_avg=220.21(M/S), cnt_part=352343366
I/wakeup_algo     [00:00:14.579 0 acomp_workqueue] counter_avg=220.40(M/S), cnt_part=352634514
I/wakeup_algo     [00:00:15.220 0 acomp_workqueue] [wrap]wakeup: chn=1; result={"rlt":[{"istart":203,"iresid":1,"iduration":26,"nfillerscore":0,"nkeywordscore":0,"ncm":648,"ncmThreshold":300,"keyword":"kai yu ba","intent":"kai yu ba","nRltIdx":2,"nDelayFrame":5,"nThrowFrame":228,"decId":0,"branch":0,"wakeUpType":0,"VadGap":0,"nE2eIntervalFrame":0,"nE2eNodeFrame":0,"nStartStateThreshold":0,"bMain":0,"bAbsorb":0,"iframe":296}]}
I/wakeup          [00:00:15.225 0 acomp_workqueue] wakeup_algo_result_cb: data_len(350)
I/wakeup_algo     [00:00:15.228 0 acomp_workqueue] WAKE(0): CHN=1, KEY=1(kai yu ba), NCM=648, len:350
I/wakeup_algo     [00:00:16.177 0 acomp_workqueue] counter_avg=222.50(M/S), cnt_part=355992438
I/wakeup_algo     [00:00:17.777 0 acomp_workqueue] counter_avg=218.77(M/S), cnt_part=350037055
I/wakeup_algo     [00:00:19.377 0 acomp_workqueue] counter_avg=219.22(M/S), cnt_part=350752636
I/wakeup_algo     [00:00:20.977 0 acomp_workqueue] counter_avg=219.33(M/S), cnt_part=350930771
I/wakeup_algo     [00:00:22.577 0 acomp_workqueue] counter_avg=218.72(M/S), cnt_part=349958010
I/wakeup_algo     [00:00:24.177 0 acomp_workqueue] counter_avg=219.19(M/S), cnt_part=350711441
I/wakeup_algo     [00:00:25.778 0 acomp_workqueue] counter_avg=219.53(M/S), cnt_part=351251256
I/wakeup_algo     [00:00:27.377 0 acomp_workqueue] counter_avg=219.40(M/S), cnt_part=351035391
I/wakeup_algo     [00:00:28.977 0 acomp_workqueue] counter_avg=219.51(M/S), cnt_part=351213292
I/wakeup_algo     [00:00:30.577 0 acomp_workqueue] counter_avg=219.62(M/S), cnt_part=351384765
I/wakeup_algo     [00:00:31.220 0 acomp_workqueue] algo_set_mode(0)
I/wakeup_algo     [00:00:31.229 0 acomp_workqueue] MODE=0
I/wakeup          [00:00:31.230 0 acomp_workqueue] wakeup_algo_timeout_cb
I/wakeup_algo     [00:00:32.173 0 acomp_workqueue] counter_avg=214.95(M/S), cnt_part=343912662
I/wakeup_algo     [00:00:33.774 0 acomp_workqueue] counter_avg=213.49(M/S), cnt_part=341580196

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

### 双麦克风模式 (CONFIG_ACOMP_WAKEUP_ALGORITHM_TYPE_DUAL_MIC)

- **输入通道**: 2 个麦克风 + 2 个回采参考
- **数据格式**: 4 通道交织(mic1, mic2, ref1, ref2)
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
CONFIG_ACOMP_WAKEUP_ALGORITHM_TYPE_DUAL_MIC=y
CONFIG_ACOMP_WAKEUP_RES_CAE_ESR_MLP_ADDRESS=0x30200000
CONFIG_ACOMP_WAKEUP_RES_CAE_ESR_MLP_LENGTH=2926656
CONFIG_ACOMP_WAKEUP_RES_AI_WRAP_ADDRESS=0x304D0000
CONFIG_ACOMP_WAKEUP_RES_AI_WRAP_LENGTH=1008
```

### 堆内存配置

```
CONFIG_HEAP_SIZE=0x10000           /* 内部 SRAM 堆 64KB */
CONFIG_PSRAM_HEAP_SIZE=0x700000    /* PSRAM 堆 7MB */
```

## 数据流说明

### M2R 流(Master to Remote)

- **通道索引**: 0
- **通道名称**: "stream.mix2ch"
- **方向**: 应用核(CP) → 算法核(AP)
- **数据**: 融合后的麦克风和回采数据(输入给算法)
- **缓冲区大小**: 一次2048 字节(256 采样点 × 4 通道 × 2 字节)

### R2M 流(Remote to Master)

- **通道索引**: 1
- **通道名称**: "stream.tocloud"
- **方向**: 算法核(AP) → 应用核(CP)
- **数据**: 算法处理后的音频数据(5 通道)(mic1, mic2, ref, 回声消除后的音频（可用于意图识别）， 算法后端输入音频)
- **缓冲区大小**: 一次输出 15360或10240 字节

## 算法内存占用

双麦算法内存占用如下(粗略统计)， 详细内存分布见`memap.h`文件

|  MCU核心 |  内存类型 | 大小 | 备注 |
|---------|----------|----|-----|
|  AP |  FLASH | 3470KB   | 2个算法资源 + ap固件大小 + cp固件大小 |
|  AP |  PSRAM |  5000KB   | 算法实例用的PSRAM + 2个算法资源拷贝到PSRAM上 + 动态内存 |
|  AP |  SRAM  |  8KB | IPC共享内存 |
|  AP |  SRAM  |  62KB | AP的SRAM |
|  AP |  SRAM  |  350KB | 算法实例大小 |
|  AP |  SRAM  |  28KB | LUNA的*(.sharedmem.*) |
|  AP |  LUNASRAM |  64KB  | LUNA专用的SRAM大小 |
