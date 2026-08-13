# 发音拼读组件

## 简介

ACOMP WSP (Word Spell Pronunciation) 是 ARCS SDK 的发音拼读组件，提供基于音频流的发音评测和拼读检测功能。该组件支持 CSP（连续语音处理）和 VAD（语音活动检测），支持麦克风采集和外部音频输入两种模式，并提供灵活的参数配置和事件回调机制。

## 主要特性

- **发音拼读评测**：支持基于音频的发音拼读评测，返回识别结果
- **CSP 连续语音处理**：支持连续语音处理模式
- **VAD 语音活动检测**：支持 VAD 检测，提供语音起始和结束事件通知，可配置门限和末端点 GAP
- **双模式输入**：支持麦克风采集（Capture）和外部音频输入（Playback）两种模式
- **ADC 增益配置**：支持配置 ADC 模拟增益和数字增益
- **音频裁剪**：支持裁剪启动后/停止前指定时长的音频数据
- **录音通道选择**：支持选择左声道或右声道
- **事件回调机制**：支持拼读结果、VAD 起止、数据流更新、写入错误等事件通知
- **跨核通信**：基于 IPC 机制实现音频流的跨核传输

## 参数数据结构

```c
typedef struct {
    int32_t alg_wsp_en;          /* CSP 开关：0-关闭; 1-开启 */
    int32_t alg_vad_en;          /* VAD 开关：0-关闭; 1-开启 */
    int32_t alg_vad_threshold;   /* VAD 门限 */
    int32_t alg_vad_gap;         /* VAD 末端点 GAP */
    int32_t adc_a_gain;          /* ADC 模拟增益 */
    int32_t adc_d_gain;          /* ADC 数字增益 */
    int32_t stream_cut_front_ms; /* 裁剪启动后音频数据的长度（ms） */
    int32_t stream_cut_last_ms;  /* 裁剪停止前音频数据的长度（ms） */
    int32_t stream_direction;    /* 音频流方向：0-Capture(MIC); 1-Playback(外部输入) */
    int32_t stream_channel_index;/* 录音通道号：0-左声道; 1-右声道 */
} acomp_wsp_params_t;
```

### 识别结果数据结构

```c
typedef struct {
    comp_wsp_result_type_e type; /* 结果类型 */
    uint32_t len;                /* 数据长度 */
    uint8_t data[];              /* 结果数据 */
} comp_wsp_result_t;
```

结果类型：

| 类型 | 枚举值 | 说明 |
|------|--------|------|
| `COMP_WSP_RESULT_TYPE_UNKNOWN` | 0 | 未知类型 |
| `COMP_WSP_RESULT_TYPE_STREAM` | 1 | 流式结果 |
| `COMP_WSP_RESULT_TYPE_FINISH` | 2 | 最终结果 |

### 音频帧大小

| 宏定义 | 值 | 说明 |
|--------|----|------|
| `WSP_FRAME_BYTES` | 320 字节 | 单帧音频数据大小 |

## 配置选项

### 基本配置

| 配置项 | 说明 | 默认值 |
|--------|------|--------|
| `CONFIG_ACOMP` | 启用 ACOMP 组件 | n |
| `CONFIG_ACOMP_WSP` | 启用 ACOMP 组件的发音拼读组件 | n |

### 资源配置

| 配置项 | 说明 | 默认值 |
|--------|------|--------|
| `CONFIG_ACOMP_WSP_RES_ENCODER_ADDRESS` | 编码器模型地址(Flash) | 0xd200000 |
| `CONFIG_ACOMP_WSP_RES_ENCODER_LENGTH` | 编码器模型大小(字节) | 3546864 |
| `CONFIG_ACOMP_WSP_RES_DECODER_ADDRESS` | 解码器模型地址(Flash) | 0xcd00000 |
| `CONFIG_ACOMP_WSP_RES_DECODER_LENGTH` | 解码器模型大小(字节) | 419904 |

## 快速开始

### 1. 启用组件

在项目的 `prj.conf` 文件中添加：

```kconfig
# 启用 ACOMP 组件
CONFIG_ACOMP=y

# 启用 ACOMP 组件的 WSP 组件
CONFIG_ACOMP_WSP=y

# 配置 WSP 组件所需资源在 Flash 的位置和大小
CONFIG_ACOMP_WSP_RES_ENCODER_ADDRESS=0xd200000
CONFIG_ACOMP_WSP_RES_ENCODER_LENGTH=3546864
CONFIG_ACOMP_WSP_RES_DECODER_ADDRESS=0xcd00000
CONFIG_ACOMP_WSP_RES_DECODER_LENGTH=419904
```

### 2. 初始化 WSP 组件

```c
#include "acomp.h"
#include "wsp/acomp_wsp.h"

int main(int argc, char **argv)
{
    // 初始化 ACOMP 框架
    acomp_init();

    // 初始化 WSP 组件
    int ret = acomp_wsp_init();
    if (ret != ACOMP_ERR_OK) {
        printf("WSP init failed: %d\n", ret);
        return -1;
    }

    return 0;
}
```

### 3. 注册事件回调

```c
#include "wsp/acomp_wsp.h"

void wsp_event_handler(uint32_t event, void *event_data,
                       uint32_t event_data_len, void *priv)
{
    if (event & WSP_CB_EVENT_ENGINE_RLT) {
        // 拼读结果
        comp_wsp_result_t *result = (comp_wsp_result_t *)event_data;
        printf("WSP result: type=%d, len=%u\n", result->type, result->len);
    } else if (event & WSP_CB_EVENT_ENGINE_VAD_BEGIN) {
        // VAD 检测到语音起始
        uint32_t frame_idx = *(uint32_t *)event_data;
        printf("VAD begin at frame %u\n", frame_idx);
    } else if (event & WSP_CB_EVENT_ENGINE_VAD_END) {
        // VAD 检测到语音结束
        uint32_t frame_idx = *(uint32_t *)event_data;
        printf("VAD end at frame %u\n", frame_idx);
    } else if (event & WSP_CB_EVENT_STREAM_UPDATE) {
        // 音频流更新
        printf("Stream update\n");
    }
}

// 注册回调
int ret = acomp_wsp_add_callback(
    WSP_CB_EVENT_ENGINE_RLT |
    WSP_CB_EVENT_ENGINE_VAD_BEGIN |
    WSP_CB_EVENT_ENGINE_VAD_END |
    WSP_CB_EVENT_STREAM_UPDATE,
    wsp_event_handler,
    NULL
);
```

### 4. 配置参数并启动

```c
void start_wsp_service(void)
{
    int ret;

    // 1. 设置组件参数（必须在 prepare 之前）
    acomp_wsp_params_t params = {
        .alg_wsp_en = 1,             // 开启 CSP
        .alg_vad_en = 1,             // 开启 VAD
        .alg_vad_threshold = 50,     // VAD 门限
        .alg_vad_gap = 30,           // VAD 末端点 GAP
        .adc_a_gain = 0,             // ADC 模拟增益
        .adc_d_gain = 0,             // ADC 数字增益
        .stream_cut_front_ms = 0,    // 不裁剪启动后音频
        .stream_cut_last_ms = 0,     // 不裁剪停止前音频
        .stream_direction = 0,       // Capture 模式（MIC 采集）
        .stream_channel_index = 0,   // 左声道
    };
    acomp_wsp_params_set(&params);

    // 2. 就绪组件（分配资源）
    ret = acomp_wsp_prepare();
    if (ret != ACOMP_ERR_OK) {
        printf("WSP prepare failed: %d\n", ret);
        return;
    }

    // 3. 启动 WSP
    ret = acomp_wsp_start();
    if (ret != ACOMP_ERR_OK) {
        printf("WSP start failed: %d\n", ret);
        return;
    }

    printf("WSP service started\n");
}

void stop_wsp_service(void)
{
    acomp_wsp_stop();
    acomp_wsp_cleanup();
}
```

## API 参考

### 生命周期管理

#### acomp_wsp_init

```c
int acomp_wsp_init(void);
```

**功能**：初始化发音拼读组件

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_NO_MEM`：内存不足
- `ACOMP_ERR_INVALID_STATE`：无效状态
- `ACOMP_ERR_NOT_FOUND`：设备未找到

#### acomp_wsp_prepare

```c
int acomp_wsp_prepare(void);
```

**功能**：就绪发音拼读组件，初始化内存块及算法资源

**说明**：在调用 `acomp_wsp_start()` 之前必须调用此函数。资源地址来自 Kconfig 配置（编码器和解码器模型）。

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_NO_MEM`：内存不足
- `ACOMP_ERR_INVALID_STATE`：无效状态

#### acomp_wsp_cleanup

```c
int acomp_wsp_cleanup(void);
```

**功能**：复位发音拼读组件，释放内存块及算法资源

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_INVALID_STATE`：无效状态

#### acomp_wsp_start

```c
int acomp_wsp_start(void);
```

**功能**：启动发音拼读组件，同步启动音频流传输

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_NO_MEM`：内存不足
- `ACOMP_ERR_INVALID_STATE`：无效状态

#### acomp_wsp_stop

```c
int acomp_wsp_stop(void);
```

**功能**：停止发音拼读组件，同步停止音频流传输

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_NO_MEM`：内存不足
- `ACOMP_ERR_INVALID_STATE`：无效状态

### 参数配置

#### acomp_wsp_params_set

```c
int acomp_wsp_params_set(acomp_wsp_params_t *params);
```

**功能**：设置组件参数

**说明**：参数在组件进入就绪态时生效（调用 `acomp_wsp_prepare()`）

**参数**：
- `params`：参数结构指针，详见 [参数数据结构](#参数数据结构)

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_INVALID_ARG`：参数错误
- `ACOMP_ERR_INVALID_STATE`：无效状态

### 事件回调

#### acomp_wsp_add_callback

```c
int acomp_wsp_add_callback(uint32_t events, wsp_event_cb_t cb, void *priv);
```

**功能**：添加事件回调函数

**参数**：
- `events`：事件位掩码，可同时注册多个事件
  - `WSP_CB_EVENT_STREAM_UPDATE`：音频数据流更新
  - `WSP_CB_EVENT_ENGINE_RLT`：发音拼读引擎结果返回
  - `WSP_CB_EVENT_ENGINE_VAD_BEGIN`：VAD 检测到语音起始
  - `WSP_CB_EVENT_ENGINE_VAD_END`：VAD 检测到语音结束
  - `WSP_CB_EVENT_ENGINE_WR_ERR`：算法引擎数据写入出错
- `cb`：回调函数指针
- `priv`：回调函数的私有数据指针

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_NO_MEM`：内存不足
- `ACOMP_ERR_INVALID_ARG`：参数错误
- `ACOMP_ERR_INVALID_STATE`：无效状态

#### acomp_wsp_remove_callback

```c
int acomp_wsp_remove_callback(wsp_event_cb_t cb);
```

**功能**：移除回调函数

**参数**：
- `cb`：待移除的回调函数

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_INVALID_ARG`：参数错误
- `ACOMP_ERR_INVALID_STATE`：无效状态
- `ACOMP_ERR_NOT_SUPPORTED`：不支持的操作

## 使用示例

### 完整示例

```c
#include "acomp.h"
#include "wsp/acomp_wsp.h"
#include "lisa_log.h"

#define TAG "wsp_demo"

void wsp_event_handler(uint32_t event, void *event_data,
                       uint32_t event_data_len, void *priv)
{
    if (event & WSP_CB_EVENT_ENGINE_RLT) {
        comp_wsp_result_t *result = (comp_wsp_result_t *)event_data;
        if (result->type == COMP_WSP_RESULT_TYPE_FINISH) {
            LISA_LOGI(TAG, "WSP final result, len: %u", result->len);
        } else if (result->type == COMP_WSP_RESULT_TYPE_STREAM) {
            LISA_LOGI(TAG, "WSP stream result, len: %u", result->len);
        }
    } else if (event & WSP_CB_EVENT_ENGINE_VAD_BEGIN) {
        uint32_t frame_idx = *(uint32_t *)event_data;
        LISA_LOGI(TAG, "VAD begin at frame %u", frame_idx);
    } else if (event & WSP_CB_EVENT_ENGINE_VAD_END) {
        uint32_t frame_idx = *(uint32_t *)event_data;
        LISA_LOGI(TAG, "VAD end at frame %u", frame_idx);
    } else if (event & WSP_CB_EVENT_ENGINE_WR_ERR) {
        LISA_LOGE(TAG, "WSP write error");
    }
}

void wsp_demo(void)
{
    int ret;

    // 1. 初始化 ACOMP 框架
    acomp_init();

    // 2. 初始化 WSP 组件
    ret = acomp_wsp_init();
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "WSP init failed: %d", ret);
        return;
    }

    // 3. 注册事件回调
    acomp_wsp_add_callback(
        WSP_CB_EVENT_ENGINE_RLT |
        WSP_CB_EVENT_ENGINE_VAD_BEGIN |
        WSP_CB_EVENT_ENGINE_VAD_END |
        WSP_CB_EVENT_STREAM_UPDATE |
        WSP_CB_EVENT_ENGINE_WR_ERR,
        wsp_event_handler, NULL
    );

    // 4. 配置参数
    acomp_wsp_params_t params = {
        .alg_wsp_en = 1,
        .alg_vad_en = 1,
        .alg_vad_threshold = 50,
        .alg_vad_gap = 30,
        .adc_a_gain = 0,
        .adc_d_gain = 0,
        .stream_cut_front_ms = 0,
        .stream_cut_last_ms = 0,
        .stream_direction = 0,       // MIC 采集
        .stream_channel_index = 0,   // 左声道
    };
    acomp_wsp_params_set(&params);

    // 5. 就绪组件
    ret = acomp_wsp_prepare();
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "WSP prepare failed: %d", ret);
        return;
    }
    LISA_LOGI(TAG, "WSP prepared");

    // 6. 启动 WSP
    ret = acomp_wsp_start();
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "WSP start failed: %d", ret);
        return;
    }
    LISA_LOGI(TAG, "WSP started");

    // 7. 等待识别结果（通过回调返回）
    vTaskDelay(pdMS_TO_TICKS(10000));

    // 8. 停止
    acomp_wsp_stop();
    LISA_LOGI(TAG, "WSP stopped");

    // 9. 清理资源
    acomp_wsp_cleanup();
    LISA_LOGI(TAG, "WSP cleaned up");
}
```

## 注意事项

1. **初始化顺序**：必须先调用 `acomp_init()` 初始化 ACOMP 框架，再调用 `acomp_wsp_init()`
2. **生命周期管理**：启动前必须调用 `acomp_wsp_prepare()`，停止后应调用 `acomp_wsp_cleanup()` 释放资源
3. **参数设置时机**：参数通过 `acomp_wsp_params_set()` 设置，在 `acomp_wsp_prepare()` 调用时生效
4. **音频流方向**：`stream_direction = 0` 使用 MIC 采集，`stream_direction = 1` 使用外部输入
5. **音频帧大小**：每帧音频数据固定为 `WSP_FRAME_BYTES`（320 字节）
6. **VAD 配置**：根据实际场景调整 VAD 门限和末端点 GAP，平衡灵敏度和误触发率
7. **事件回调**：回调函数在组件内部线程中执行，应避免长时间阻塞操作
8. **资源配置**：确保配置的编码器和解码器模型地址与实际烧录的资源匹配
9. **跨核通信**：音频流基于 IPC 机制，注意跨核数据传输的延迟和同步问题

## 常见问题

**Q: WSP 组件初始化失败怎么办？**

A: 检查以下几点：
- 确保 ACOMP 框架已正确初始化
- 检查系统是否有足够的堆内存
- 确认编码器和解码器模型资源已正确烧录到 Flash

**Q: 无法触发拼读结果回调？**

A: 检查：
- 确认已正确注册 `WSP_CB_EVENT_ENGINE_RLT` 事件回调
- 检查 CSP 是否已开启（`alg_wsp_en = 1`）
- 确认麦克风是否正常采集音频数据
- 如果使用 Playback 模式，确认是否正确输入了音频数据

**Q: VAD 检测不灵敏怎么办？**

A: 建议：
- 降低 VAD 门限值（`alg_vad_threshold`）
- 检查 ADC 增益设置是否合理
- 确认麦克风硬件工作正常

**Q: VAD 误触发怎么办？**

A: 建议：
- 提高 VAD 门限值
- 增大末端点 GAP（`alg_vad_gap`）
- 检查环境噪声

## 依赖项

- `ACOMP 框架`：算法组件框架
- `ACOMP Stream IPC`：音频流跨核通信
- `lisa_log`：日志系统
- `FreeRTOS`：实时操作系统
- `WSP 算法库`：发音拼读算法引擎
- `音频驱动`：麦克风和音频处理硬件驱动
