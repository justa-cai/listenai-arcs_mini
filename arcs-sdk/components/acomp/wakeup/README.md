# 语音唤醒组件

## 简介

ACOMP Wakeup 是 ARCS SDK 的语音唤醒组件，提供基于音频流的语音唤醒功能。该组件支持单麦克风和双麦克风唤醒算法，支持WAKUP/ESR模式，并提供灵活的唤醒门限配置和音频流管理接口。

## 主要特性

- **多麦克风支持**：支持单麦克风和双麦克风唤醒算法
- **双模式运行**：支持唤醒模式和 ESR 模式
- **灵活门限配置**：提供 6 级唤醒门限等级，从极易唤醒到禁用唤醒
- **事件回调机制**：支持唤醒结果、音频流更新、超时等多种事件通知
- **音频流管理**：提供完整的音频流通道管理和缓冲区操作接口
- **状态管理**：支持初始化、就绪、启动、停止等完整的生命周期管理
- **跨核通信**：基于 IPC 机制实现音频流的跨核传输

## 组件架构

ACOMP Wakeup 组件采用双核异构架构,CP 核(应用核)通过 ACOMP IPC 框架与 AP 核(算法核)进行通信,实现语音唤醒功能。

```
┌─────────────────────────────────────────────────────────────────────┐
│                        CP 核 (Application Core)                      │
│                                                                       │
│  ┌──────────────────────────────────────────────────────────────┐   │
│  │                      用户应用层                               │   │
│  │  ┌────────────────────────────────────────────────────────┐  │   │
│  │  │  • 唤醒事件回调处理 (关键词/角度/超时)                 │  │   │
│  │  │  • 音频数据采集 (麦克风/回采)                          │  │   │
│  │  │  • 算法输出音频处理 (回声消除后的音频)                 │  │   │
│  │  └────────────────────┬───────────────────────────────────┘  │   │
│  └───────────────────────┼──────────────────────────────────────┘   │
│                          │                                           │
│  ┌───────────────────────▼──────────────────────────────────────┐   │
│  │              ACOMP Wakeup 组件 (CP 侧)                       │   │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐       │   │
│  │  │ 生命周期管理 │  │ 事件回调管理 │  │ 参数配置接口 │       │   │
│  │  │ init/prepare │  │ add_callback │  │ set_mode/    │       │   │
│  │  │ start/stop   │  │ remove_cb    │  │ threshold    │       │   │
│  │  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘       │   │
│  │         │                 │                 │                │   │
│  │  ┌──────▼─────────────────▼─────────────────▼───────┐       │   │
│  │  │          音频流管理 (Stream API)                 │       │   │
│  │  │  • M2R 流: 音频输入 (mic+ref → 算法)            │       │   │
│  │  │  • R2M 流: 算法输出 (回声消除后的音频 → 应用)   │       │   │
│  │  │  • tx_alloc/submit, rx_get/release              │       │   │
│  │  └──────────────────────┬───────────────────────────┘       │   │
│  └─────────────────────────┼─────────────────────────────────────   │
│                            │                                         │
│  ┌─────────────────────────▼─────────────────────────────────────┐  │
│  │                   ACOMP IPC 层 (CP 侧)                        │  │
│  │  • IPC 消息封装 (NEW/FREE/CONTROL/NOTIFY)                     │  │
│  │  • 设备查询和管理                                             │  │
│  │  • 同步/异步命令处理                                          │  │
│  └─────────────────────────┬─────────────────────────────────────┘  │
└────────────────────────────┼────────────────────────────────────────┘
                             │
                             │ IC Message (IPC 消息通道)
                             │
┌────────────────────────────▼────────────────────────────────────────┐
│                        AP 核 (Algorithm Core)                        │
│                                                                       │
│  ┌─────────────────────────────────────────────────────────────┐    │
│  │                   ACOMP Framework (AP 侧)                    │    │
│  │                                                               │    │
│  │  • 接收 CP 核的 IPC 命令 (prepare/start/stop/control)        │    │
│  │  • 处理音频流数据 (M2R 接收 / R2M 发送)                      │    │
│  │  • 执行唤醒算法引擎                                          │    │
│  │  • 推送唤醒事件到 CP 核 (关键词/角度/超时)                  │    │
│  │                                                               │    │
│  └─────────────────────────────────────────────────────────────┘    │
│                                                                       │
│  ┌─────────────────────────────────────────────────────────────┐    │
│  │              唤醒算法引擎 (Wakeup Engine)                    │    │
│  │                                                               │    │
│  │  • 音频预处理 (降噪/回声消除)                                │    │
│  │  • 特征提取和关键词识别                                      │    │
│  │  • 角度估计 (DOA)                                            │    │
│  │  • 支持单麦/双麦、Wakeup/ESR 模式                            │    │
│  │                                                               │    │
│  └─────────────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────────────┘

                    ┌─────────────────────────┐
                    │   共享内存 (Shared Mem)  │
                    │  ┌────────────────────┐  │
                    │  │ VirtQueue 环形缓冲 │  │
                    │  │ (Stream IPC 数据)  │  │
                    │  └────────────────────┘  │
                    └─────────────────────────┘
```

### 架构说明

#### 1. CP 核 (应用核) - 开放接口层
- **用户应用层**: 处理唤醒事件,采集音频数据,处理算法输出
- **ACOMP Wakeup 组件**: 提供生命周期管理、事件回调、参数配置等 API
- **音频流管理**: 管理 M2R(Master to Remote) 和 R2M(Remote to Master) 数据流
- **ACOMP IPC 层**: 封装 IPC 消息,与 AP 核通信

#### 2. AP 核 (算法核) - 算法实现层
- **ACOMP Framework**: 接收 IPC 命令,管理算法生命周期,处理音频流数据
- **唤醒算法引擎**: 执行语音唤醒算法,支持单麦/双麦、Wakeup/ESR 模式

> **注意**: AP 核的具体实现为闭源算法库,开发者只需关注 CP 核的 API 使用即可。

#### 3. 通信机制
- **控制通道**: 通过 IC Message 传递控制命令 (prepare/start/stop/control)
- **数据通道**: 通过 VirtQueue 共享内存实现零拷贝音频流传输
- **事件通知**: AP 核通过 NOTIFY 消息主动推送唤醒结果到 CP 核

#### 4. 数据流向
- **M2R 流**: CP 核采集的音频数据 (麦克风+回采) → AP 核算法引擎
- **R2M 流**: AP 核算法输出 (回声消除后的音频) → CP 核应用层
- **事件流**: AP 核唤醒结果/角度/超时事件 → CP 核回调函数

## 音频数据结构

### 输入音频数据结构

根据配置的算法类型，输入音频数据结构有所不同：

**双麦克风模式** (`CONFIG_ACOMP_WAKEUP_ALGORITHM_TYPE_DUAL_MIC=y`):

```c
typedef struct {
    short mic0;         /* mic0的音频 */
    short mic1;         /* mic1的音频 */
    short ref0;         /* 回采的音频 */
    short ref1;         /* 回采的音频，如只有一路回采，则可复制ref0的数据 */
} acomp_wakeup_audio_in_t;
```

**单麦克风模式** (`CONFIG_ACOMP_WAKEUP_ALGORITHM_TYPE_SINGLE_MIC=y`):

```c
typedef struct {
    short mic0;         /* mic0的音频 */
    short ref0;         /* 回采的音频 */
} acomp_wakeup_audio_in_t;
```

### 输出音频数据结构

算法处理后输出的音频数据为 5 通道格式：

```c
typedef struct {
    short mic0;         /* mic0的音频 */
    short mic1;         /* mic1的音频，在单麦算法的情况下，mic1的音频实际为mic0的音频 */
    short ref0;         /* 回采的音频 */
    short out1;         /* 回声消除之后的音频 */
    short out2;         /* 算法输出的其他音频 */
} acomp_wakeup_audio_out_t;
```

### 音频帧大小定义

组件提供了一组宏定义用于计算音频帧大小：

**输入音频帧信息**:

| 宏定义 | 说明 | 值 |
|--------|------|----|
| `ACOMP_WAKEUP_AUDIO_INPUT_LEN_ONE_SAMPLE` | 单个输入音频采样点的字节数 | `sizeof(acomp_wakeup_audio_in_t)` |
| `ACOMP_WAKEUP_AUDIO_INPUT_SAMPLE_CNT` | 每帧输入音频的采样点数量 | 256 |
| `ACOMP_WAKEUP_AUDIO_INPUT_LEN_ONCE_FRAME` | 单帧输入音频数据的总字节数 | 单麦: 1024 字节<br>双麦: 2048 字节 |
| `ACOMP_WAKEUP_ESR_NEED_FRAME_CNT` | 一次完整识别需要输入的音频帧数量 | 10 |

**输出音频帧信息**:

| 宏定义 | 说明 | 值 |
|--------|------|----|
| `ACOMP_WAKEUP_AUDIO_OUTPUT_CHANNELS_PER_SAMPLE` | 输出音频每个采样点的通道数 | 5 |
| `ACOMP_WAKEUP_AUDIO_OUTPUT_LEN_ONE_SAMPLE` | 单个输出音频采样点的字节数 | 10 字节 |
| `ACOMP_WAKEUP_AUDIO_OUTPUT_MAX_LEN_ONCE_FRAME` | 单次输出音频数据的最大字节数 | 25600 字节 |

## 配置选项

### 基本配置

| 配置项 | 说明 | 默认值 |
|--------|------|--------|
| `CONFIG_ACOMP` | 启用ACOMP组件 | n |
| `CONFIG_ACOMP_WAKEUP` | 启用ACOMP组件的语音唤醒组件 | n |

### 算法类型配置

**注意**: 单麦和双麦算法互斥，只能选择其中一种。

| 配置项 | 说明 | 默认值 |
|--------|------|--------|
| `CONFIG_ACOMP_WAKEUP_ALGORITHM_TYPE_SINGLE_MIC` | 单麦克风唤醒算法 | y |
| `CONFIG_ACOMP_WAKEUP_ALGORITHM_TYPE_DUAL_MIC` | 双麦克风唤醒算法 | n |

### 资源配置

| 配置项 | 说明 | 默认值 |
|--------|------|--------|
| `CONFIG_ACOMP_WAKEUP_RES_CAE_ESR_MLP_ADDRESS` | CAE ESR MLP 模型地址(存储在flash) | 0xcd00000 |
| `CONFIG_ACOMP_WAKEUP_RES_CAE_ESR_MLP_LENGTH` | CAE ESR MLP 模型长度 | 419904 |
| `CONFIG_ACOMP_WAKEUP_RES_AI_WRAP_ADDRESS` | AI Wrap 资源地址(存储在flash) | 0xcd00000 |
| `CONFIG_ACOMP_WAKEUP_RES_AI_WRAP_LENGTH` | AI Wrap 资源长度 | 419904 |

## 快速开始

### 1. 启用组件

在项目的 `prj.conf` 文件中添加：

```kconfig
# 启用 ACOMP 组件
CONFIG_ACOMP=y

# 启用ACOMP组件的WAKEUP组件
CONFIG_ACOMP_WAKEUP=y
# 选择算法类型（单麦或双麦）
CONFIG_ACOMP_WAKEUP_ALGORITHM_TYPE_DUAL_MIC=y

# 配置wakeup组件所需资源在flash的位置和大小(由用户来决定资源的放置位置和大小)
CONFIG_ACOMP_WAKEUP_RES_CAE_ESR_MLP_ADDRESS=0x30200000
CONFIG_ACOMP_WAKEUP_RES_CAE_ESR_MLP_LENGTH=2926656
CONFIG_ACOMP_WAKEUP_RES_AI_WRAP_ADDRESS=0x304D0000
CONFIG_ACOMP_WAKEUP_RES_AI_WRAP_LENGTH=1008
```

### 2. 初始化唤醒组件

在应用程序中初始化唤醒组件：

```c
#include "acomp.h"
#include "wakeup/acomp_wakeup.h"

int main(int argc, char **argv)
{
    // 初始化 ACOMP 框架
    acomp_init();
    
    // 初始化唤醒组件
    int ret = acomp_wakeup_init();
    if (ret != ACOMP_ERR_OK) {
        printf("Wakeup init failed: %d\n", ret);
        return -1;
    }
    
    // 你的应用代码
    return 0;
}
```

### 3. 注册事件回调

使用事件回调函数接收唤醒结果和状态更新：

```c
#include "wakeup/acomp_wakeup.h"

void wakeup_event_handler(uint32_t event, void *event_data, 
                         uint32_t event_data_len, void *priv)
{
    if (event & WAKEUP_CB_EVENT_ENGINE_RLT) {
        // 唤醒结果
        printf("Wakeup result: %s\n", (char *)event_data);
    } else if (event & WAKEUP_CB_EVENT_STREAM_UPDATE) {
        // 音频流更新
        printf("Stream update\n");
    } else if (event & WAKEUP_CB_EVENT_ENGINE_TIMEOUT) {
        // 唤醒超时
        printf("Wakeup timeout\n");
    } else if (event & WAKEUP_CB_EVENT_ENGINE_ANGLE) {
        // 角度信息
        printf("Angle info received\n");
    }
}

// 注册回调
int ret = acomp_wakeup_add_callback(
    WAKEUP_CB_EVENT_ENGINE_RLT | 
    WAKEUP_CB_EVENT_STREAM_UPDATE | 
    WAKEUP_CB_EVENT_ENGINE_TIMEOUT |
    WAKEUP_CB_EVENT_ENGINE_ANGLE,
    wakeup_event_handler, 
    NULL
);
```

### 4. 启动唤醒服务

完整的启动流程：

```c
#include "wakeup/acomp_wakeup.h"

void start_wakeup_service(void)
{
    int ret;
    
    // 1. 就绪组件（分配资源）
    ret = acomp_wakeup_prepare();
    if (ret != ACOMP_ERR_OK) {
        printf("Wakeup prepare failed: %d\n", ret);
        return;
    }
    
    // 2. 启动唤醒
    ret = acomp_wakeup_start();
    if (ret != ACOMP_ERR_OK) {
        printf("Wakeup start failed: %d\n", ret);
        return;
    }
    
    printf("Wakeup service started\n");
}

void stop_wakeup_service(void)
{
    int ret;
    
    // 1. 停止唤醒
    ret = acomp_wakeup_stop();
    if (ret != ACOMP_ERR_OK) {
        printf("Wakeup stop failed: %d\n", ret);
        return;
    }
    
    // 2. 清理资源
    ret = acomp_wakeup_cleanup();
    if (ret != ACOMP_ERR_OK) {
        printf("Wakeup cleanup failed: %d\n", ret);
        return;
    }
    
    printf("Wakeup service stopped\n");
}
```

### 5. 配置唤醒参数

设置算法模式和唤醒门限（必须在启动后设置）：

```c
#include "wakeup/acomp_wakeup.h"

void configure_wakeup(void)
{
    int ret;
    
    // 1. 就绪并启动唤醒组件
    ret = acomp_wakeup_prepare();
    ret = acomp_wakeup_start();
    
    // 2. 启动后设置算法模式和门限（必须在 start 之后）
    acomp_wakeup_set_algo_mode(ACOMP_WAKEUP_ALGO_MODE_WAKEUP);
    acomp_wakeup_set_threshold(ACOMP_WAKEUP_THRESHOLD_LEVEL_3);
    
    // 或者设置为 ESR 模式
    // acomp_wakeup_set_algo_mode(ACOMP_WAKEUP_ALGO_MODE_ESR);
}
```

### 6. 把音频数据通过TX流通道发送给AP核算法引擎处理

使能 M2R (Master to Remote) 流通道,用于将音频数据发送给 AP 核:

```c
#include "wakeup/acomp_wakeup.h"

#define TX_STREAM_CH_INDEX    0
#define TX_STREAM_CH_NAME     "stream.mix2ch"

void setup_tx_stream(void)
{
    // 1. 创建 M2R 流通道描述符
    acomp_stream_chn_create_desc_t desc = {
        .cname = TX_STREAM_CH_NAME,
        .direction = ACOMP_STREAM_DIRECTION_M2R,  // Master to Remote
        .index = TX_STREAM_CH_INDEX,
        .buffer_size = ACOMP_WAKEUP_AUDIO_INPUT_LEN_ONCE_FRAME,    // 单帧音频数据大小
        .num_descs = 4,         // 缓冲区描述符数量(根据实际需求设置)
        .kick_policy = 1,       // 自动触发
    };
    
    // 2. 使能流通道
    int ret = acomp_wakeup_stream_ch_enable(TX_STREAM_CH_INDEX, &desc);
    if (ret != ACOMP_ERR_OK) {
        printf("Failed to enable tx stream: %d\n", ret);
    }
}

void send_data_to_remote(void)
{
    uint8_t *buffer;
    uint32_t buf_size;
    uint16_t desc_idx;
    
    // 1. 分配发送缓冲区
    buffer = acomp_wakeup_stream_tx_buffer_alloc(TX_STREAM_CH_INDEX, 
                                                  &buf_size, &desc_idx);
    if (buffer && buf_size > 0) {
        // 2. 填充音频数据到缓冲区
        // 数据格式为 acomp_wakeup_audio_in_t 数组，包含 256 个采样点
        // 双麦: {mic0, mic1, ref0, ref1} * 256
        // 单麦: {mic0, ref0} * 256
        acomp_wakeup_audio_in_t *audio_frame = (acomp_wakeup_audio_in_t *)buffer;
        // 填充从麦克风采集的音频数据
        // fill_audio_data(audio_frame, ACOMP_WAKEUP_AUDIO_INPUT_SAMPLE_CNT);
        
        // 3. 提交缓冲区发送
        acomp_wakeup_stream_tx_buffer_submit(TX_STREAM_CH_INDEX, 
                                             buffer, buf_size, desc_idx);
    }
}
```

### 7. 使能 RX 流通道并接收数据

使能 R2M (Remote to Master) 流通道,用于接收 AP 核的输出数据:

```c
#include "wakeup/acomp_wakeup.h"

#define RX_STREAM_CH_INDEX   1
#define RX_STREAM_CH_NAME    "stream.tocloud"

void setup_rx_stream(void)
{
    // 1. 创建 R2M 流通道描述符
    acomp_stream_chn_create_desc_t desc = {
        .cname = RX_STREAM_CH_NAME,
        .direction = ACOMP_STREAM_DIRECTION_R2M,  // Remote to Master
        .index = RX_STREAM_CH_INDEX,
        .buffer_size = ACOMP_WAKEUP_AUDIO_OUTPUT_MAX_LEN_ONCE_FRAME,   // 最大输出帧大小
        .num_descs = 8,         // 缓冲区描述符数量
        .kick_policy = 1,       // 自动触发
    };
    
    // 2. 使能流通道
    int ret = acomp_wakeup_stream_ch_enable(RX_STREAM_CH_INDEX, &desc);
    if (ret != ACOMP_ERR_OK) {
        printf("Failed to enable rx stream: %d\n", ret);
    }
}

void receive_data_from_remote(void)
{
    uint8_t *buffer;
    uint32_t len;
    uint16_t desc_idx;
    
    // 1. 获取算法输出的音频数据
    buffer = acomp_wakeup_stream_rx_buffer_get(RX_STREAM_CH_INDEX, &len, &desc_idx);
    
    if (buffer && len > 0) {
        // 2. 处理接收到的数据
        // 数据格式为 acomp_wakeup_audio_out_t 数组，5通道交织
        // {mic0, mic1, ref0, out1, out2} * N 个采样点
        // 其中 out1 为回声消除后的音频，可用于云端识别
        acomp_wakeup_audio_out_t *audio_out = (acomp_wakeup_audio_out_t *)buffer;
        int sample_count = len / sizeof(acomp_wakeup_audio_out_t);
        // process_output_audio(audio_out, sample_count);
        
        // 3. 释放缓冲区
        acomp_wakeup_stream_rx_buffer_release(RX_STREAM_CH_INDEX, 
                                              desc_idx, len, buffer);
    }
}
```


## API 参考

### 生命周期管理

#### acomp_wakeup_init

```c
int acomp_wakeup_init(void);
```

**功能**：初始化语音唤醒组件

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_NO_MEM`：内存不足
- `ACOMP_ERR_INVALID_STATE`：无效状态
- `ACOMP_ERR_NOT_FOUND`：设备未找到

#### acomp_wakeup_prepare

```c
int acomp_wakeup_prepare(void);
```

**功能**：就绪语音唤醒组件，初始化内存块及算法资源

**说明**：在调用 `acomp_wakeup_start()` 之前必须调用此函数

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_NO_MEM`：内存不足
- `ACOMP_ERR_INVALID_STATE`：无效状态

#### acomp_wakeup_cleanup

```c
int acomp_wakeup_cleanup(void);
```

**功能**：复位语音唤醒组件，释放内存块及算法资源

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_INVALID_STATE`：无效状态

#### acomp_wakeup_start

```c
int acomp_wakeup_start(void);
```

**功能**：启动语音唤醒组件，同步启动音频流传输

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_NO_MEM`：内存不足
- `ACOMP_ERR_INVALID_STATE`：无效状态

#### acomp_wakeup_stop

```c
int acomp_wakeup_stop(void);
```

**功能**：停止语音唤醒组件，同步停止音频流传输

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_INVALID_STATE`：无效状态

### 参数配置

#### acomp_wakeup_set_algo_mode

```c
int acomp_wakeup_set_algo_mode(acomp_wakeup_algo_mode_e mode);
```

**功能**：设置算法模式

**说明**：该函数必须在调用 `acomp_wakeup_start()` 之后设置才能生效

**参数**：
- `mode`：算法模式
  - `ACOMP_WAKEUP_ALGO_MODE_WAKEUP`：唤醒模式
  - `ACOMP_WAKEUP_ALGO_MODE_ESR`：ESR 模式

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_INVALID_ARG`：参数错误
- `ACOMP_ERR_INVALID_STATE`：无效状态

#### acomp_wakeup_set_threshold

```c
int acomp_wakeup_set_threshold(acomp_wakeup_threshold_level_e level);
```

**功能**：设置唤醒门限等级

**说明**：该函数必须在调用 `acomp_wakeup_start()` 之后设置才能生效

**参数**：
- `level`：门限等级 (1-6)
  - `ACOMP_WAKEUP_THRESHOLD_LEVEL_1`：最低，极易唤醒
  - `ACOMP_WAKEUP_THRESHOLD_LEVEL_2`：易唤醒
  - `ACOMP_WAKEUP_THRESHOLD_LEVEL_3`：默认档位
  - `ACOMP_WAKEUP_THRESHOLD_LEVEL_4`：难唤醒
  - `ACOMP_WAKEUP_THRESHOLD_LEVEL_5`：极难唤醒
  - `ACOMP_WAKEUP_THRESHOLD_LEVEL_6`：最高，禁用唤醒

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_INVALID_ARG`：参数错误
- `ACOMP_ERR_INVALID_STATE`：无效状态

### 事件回调

#### acomp_wakeup_add_callback

```c
int acomp_wakeup_add_callback(uint32_t events, wakeup_event_cb_t cb, void *priv);
```

**功能**：添加事件回调函数

**参数**：
- `events`：事件位掩码，可同时注册多个事件
  - `WAKEUP_CB_EVENT_STREAM_UPDATE`：音频数据流更新
  - `WAKEUP_CB_EVENT_ENGINE_RLT`：语音唤醒引擎结果返回
  - `WAKEUP_CB_EVENT_ENGINE_TIMEOUT`：唤醒超时
  - `WAKEUP_CB_EVENT_ENGINE_ANGLE`：角度信息
- `cb`：回调函数指针
- `priv`：回调函数的私有数据指针

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_NO_MEM`：内存不足
- `ACOMP_ERR_INVALID_ARG`：参数错误
- `ACOMP_ERR_INVALID_STATE`：无效状态

#### acomp_wakeup_remove_callback

```c
int acomp_wakeup_remove_callback(wakeup_event_cb_t cb);
```

**功能**：移除回调函数

**参数**：
- `cb`：待移除的回调函数

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_INVALID_ARG`：参数错误
- `ACOMP_ERR_INVALID_STATE`：无效状态
- `ACOMP_ERR_NOT_SUPPORTED`：不支持的操作

### 音频流管理

#### acomp_wakeup_stream_ch_enable

```c
int acomp_wakeup_stream_ch_enable(int chn, acomp_stream_chn_create_desc_t *desc);
```

**功能**：使能音频流通道

**参数**：
- `chn`：通道索引
- `desc`：通道描述符指针

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_INVALID_STATE`：无效状态
- `ACOMP_ERR_INVALID_ARG`：参数错误
- `ACOMP_ERR_CREATE_STREAM_FAILED`：创建流失败

#### acomp_wakeup_stream_ch_disable

```c
int acomp_wakeup_stream_ch_disable(int chn);
```

**功能**：禁用音频流通道

**参数**：
- `chn`：通道索引

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_INVALID_STATE`：无效状态
- `ACOMP_ERR_INVALID_ARG`：参数错误

#### acomp_wakeup_stream_rx_buffer_get

```c
void* acomp_wakeup_stream_rx_buffer_get(int chn, uint32_t* len, uint16_t* desc_idx);
```

**功能**：获取 RX 流缓冲区

**参数**：
- `chn`：通道索引
- `len`：数据长度指针（输出）
- `desc_idx`：描述符索引指针（输出）

**返回值**：缓冲区指针，失败返回 NULL

#### acomp_wakeup_stream_rx_buffer_release

```c
int acomp_wakeup_stream_rx_buffer_release(int chn, uint16_t desc_idx, 
                                          uint32_t len, void* buffer);
```

**功能**：释放 RX 流缓冲区

**参数**：
- `chn`：通道索引
- `desc_idx`：描述符索引
- `len`：数据长度
- `buffer`：缓冲区指针

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_INVALID_STATE`：无效状态
- `ACOMP_ERR_INVALID_ARG`：参数错误

#### acomp_wakeup_stream_tx_buffer_alloc

```c
void* acomp_wakeup_stream_tx_buffer_alloc(int chn, uint32_t* len, uint16_t* desc_idx);
```

**功能**：分配 TX 流缓冲区用于向 remote 发送音频数据

**参数**：
- `chn`：通道索引
- `len`：可用缓冲区长度指针（输出）
- `desc_idx`：描述符索引指针（输出）

**返回值**：缓冲区指针，失败返回 NULL

#### acomp_wakeup_stream_tx_buffer_submit

```c
int acomp_wakeup_stream_tx_buffer_submit(int chn, void* buffer, 
                                         uint32_t len, uint16_t desc_idx);
```

**功能**：提交 TX 流缓冲区发送音频数据到 remote

**参数**：
- `chn`：通道索引
- `buffer`：缓冲区指针
- `len`：数据长度
- `desc_idx`：描述符索引

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_INVALID_STATE`：无效状态
- `ACOMP_ERR_INVALID_ARG`：参数错误

## 使用示例

### 完整示例

```c
#include "acomp.h"
#include "wakeup/acomp_wakeup.h"
#include "lisa_log.h"

#define TAG "wakeup_demo"

// 事件回调函数
void wakeup_event_handler(uint32_t event, void *event_data, 
                         uint32_t event_data_len, void *priv)
{
    if (event & WAKEUP_CB_EVENT_ENGINE_RLT) {
        LISA_LOGI(TAG, "Wakeup result: %d, %s", 
                  event_data_len, (char *)event_data);
    } else if (event & WAKEUP_CB_EVENT_STREAM_UPDATE) {
        LISA_LOGI(TAG, "Stream update data: %d, len: %d", 
                  *(uint32_t*)event_data, event_data_len);
    } else if (event & WAKEUP_CB_EVENT_ENGINE_TIMEOUT) {
        LISA_LOGI(TAG, "Wakeup timeout");
    } else {
        LISA_LOGW(TAG, "Unknown wakeup event: 0x%X", event);
    }
}

void wakeup_demo(void)
{
    int ret;
    
    // 1. 初始化 ACOMP 框架
    acomp_init();
    
    // 2. 初始化唤醒组件
    ret = acomp_wakeup_init();
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "Wakeup init failed: %d", ret);
        return;
    }
    
    // 3. 注册事件回调
    ret = acomp_wakeup_add_callback(
        WAKEUP_CB_EVENT_ENGINE_RLT | 
        WAKEUP_CB_EVENT_STREAM_UPDATE | 
        WAKEUP_CB_EVENT_ENGINE_TIMEOUT,
        wakeup_event_handler, 
        NULL
    );
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "Add callback failed: %d", ret);
        return;
    }
    
    // 4. 就绪组件
    ret = acomp_wakeup_prepare();
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "Wakeup prepare failed: %d", ret);
        return;
    }
    LISA_LOGI(TAG, "Wakeup prepared");
    
    // 5. 启动唤醒
    ret = acomp_wakeup_start();
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "Wakeup start failed: %d", ret);
        return;
    }
    LISA_LOGI(TAG, "Wakeup started");
    
    // 6. 配置唤醒参数（必须在 start 之后）
    acomp_wakeup_set_algo_mode(ACOMP_WAKEUP_ALGO_MODE_WAKEUP);
    acomp_wakeup_set_threshold(ACOMP_WAKEUP_THRESHOLD_LEVEL_3);
    
    // 7. 运行一段时间
    vTaskDelay(pdMS_TO_TICKS(10000));
    
    // 8. 停止唤醒
    ret = acomp_wakeup_stop();
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "Wakeup stop failed: %d", ret);
    }
    LISA_LOGI(TAG, "Wakeup stopped");
    
    // 9. 清理资源
    ret = acomp_wakeup_cleanup();
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "Wakeup cleanup failed: %d", ret);
    }
    LISA_LOGI(TAG, "Wakeup cleaned up");
}
```

### 音频流处理示例

#### 音频数据输入给算法引擎
```c
#include "wakeup/acomp_wakeup.h"

void audio_stream_tx_example(void)
{
    int chn = 0;  // 通道 0
    uint32_t len;
    uint16_t desc_idx;
    void *buffer;
    
    while (1) {
        // 发送音频数据
        buffer = acomp_wakeup_stream_tx_buffer_alloc(chn, &len, &desc_idx);
        if (buffer) {
            // 填充音频数据
            fill_audio_data(buffer, len);
            
            // 提交发送
            acomp_wakeup_stream_tx_buffer_submit(chn, buffer, len, desc_idx);
        }
    }
}
```

#### 获取算法引擎输出的音频数据，并处理

```c
#include "wakeup/acomp_wakeup.h"

void audio_stream_rx_example(void)
{
    int chn = 0;  // 通道 0
    uint32_t len;
    uint16_t desc_idx;
    void *buffer;
    
    while (1) {
        // 接收音频数据
        buffer = acomp_wakeup_stream_rx_buffer_get(chn, &len, &desc_idx);
        if (buffer) {
            // 处理算法输出的音频数据
            process_audio_data(buffer, len);
            
            // 释放缓冲区
            acomp_wakeup_stream_rx_buffer_release(chn, desc_idx, len, buffer);
        }
    }
}
```

## 注意事项

1. **初始化顺序**：必须先调用 `acomp_init()` 初始化 ACOMP 框架，再调用 `acomp_wakeup_init()`
2. **生命周期管理**：启动前必须调用 `acomp_wakeup_prepare()`，停止后应调用 `acomp_wakeup_cleanup()` 释放资源
3. **状态检查**：所有 API 都会进行状态检查，确保在正确的状态下调用相应的函数
4. **事件回调**：回调函数在组件内部线程中执行，应避免长时间阻塞操作
5. **内存管理**：组件使用动态内存分配，确保系统有足够的堆内存
6. **资源配置**：确保配置的模型地址和长度与实际烧录的模型匹配
7. **算法选择**：单麦和双麦算法互斥，只能选择其中一种
8. **门限调整**：根据实际使用场景调整唤醒门限，平衡误唤醒率和唤醒成功率
9. **音频流管理**：使用音频流接口时，必须成对调用 get/release 和 alloc/submit
10. **跨核通信**：音频流基于 IPC 机制，注意跨核数据传输的延迟和同步问题

## 常见问题

**Q: 唤醒组件初始化失败怎么办？**

A: 检查以下几点：
- 确保 ACOMP 框架已正确初始化
- 检查系统是否有足够的堆内存
- 确认音频硬件设备是否正常工作
- 检查配置的模型资源地址和长度是否正确

**Q: 无法触发唤醒回调？**

A: 检查：
- 确认已正确注册事件回调函数
- 检查唤醒门限设置是否过高（尝试降低门限等级）
- 确认麦克风是否正常采集音频数据
- 检查算法模式设置是否正确

**Q: 唤醒成功率低怎么办？**

A: 可以尝试：
- 降低唤醒门限等级（如从 3 降到 2）
- 检查麦克风音频质量和增益设置
- 确认使用的算法类型（单麦/双麦）与硬件配置匹配
- 检查环境噪声是否过大

**Q: 误唤醒率高怎么办？**

A: 可以尝试：
- 提高唤醒门限等级（如从 3 升到 4）
- 检查是否有持续的背景噪声干扰
- 确认麦克风位置和方向是否合理

**Q: 如何在唤醒模式和 ESR 模式之间切换？**

A: 使用 `acomp_wakeup_set_algo_mode()` 函数：
```c
// 切换到唤醒模式
acomp_wakeup_set_algo_mode(ACOMP_WAKEUP_ALGO_MODE_WAKEUP);

// 切换到 ESR 模式
acomp_wakeup_set_algo_mode(ACOMP_WAKEUP_ALGO_MODE_ESR);
```
注意：模式切换应在停止状态下进行。

## 依赖项

- `ACOMP 框架`：音频组件框架
- `ACOMP Stream IPC`：音频流跨核通信
- `lisa_log`：日志系统
- `FreeRTOS`：实时操作系统
- `唤醒算法库`：语音唤醒算法引擎
- `音频驱动`：麦克风和音频处理硬件驱动

## 相关文档

- ACOMP 框架文档
- 音频流 IPC 接口文档
- 语音唤醒算法说明文档
