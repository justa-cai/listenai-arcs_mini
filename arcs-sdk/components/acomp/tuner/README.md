# 调音组件

## 简介

ACOMP Tuner 是 ARCS SDK 的音频调音组件，提供 EQ（均衡器）、DRC（动态范围压缩）和 Limiter（限幅器）等音频处理功能。该组件支持采样率配置、调音参数设置、限幅器参数设置和音量调节，音频数据通过双向流通道（M2R 输入 / R2M 输出）与 AP 核算法引擎交互。

## 主要特性

- **EQ/DRC/Limiter**：支持均衡器、动态范围压缩和限幅器音频处理
- **采样率配置**：支持设置音频采样率
- **调音参数设置**：支持自定义 EQ/DRC 调音参数
- **限幅器参数设置**：支持独立配置限幅器参数
- **音量控制**：支持设置输出音量系数
- **使能控制**：支持动态开启/关闭调音处理
- **双向音频流**：支持 M2R（输入原始音频）和 R2M（输出处理后音频）双向流通道
- **事件回调机制**：支持状态变更和数据流更新事件通知
- **跨核通信**：基于 IPC 机制实现音频数据的跨核传输

## 配置选项

### 基本配置

| 配置项 | 说明 | 默认值 |
|--------|------|--------|
| `CONFIG_ACOMP` | 启用 ACOMP 组件 | n |
| `CONFIG_ACOMP_TUNER` | 启用 ACOMP 组件的调音组件 | n |

### 流通道配置

| 配置项 | 说明 | 默认值 |
|--------|------|--------|
| `CONFIG_ACOMP_TUNER_STREAM_BUFFER_SIZE` | 流缓冲区大小(字节) | 2560 |
| `CONFIG_ACOMP_TUNER_STREAM_NUM_DESCS` | 流描述符数量 | 16 |

## 快速开始

### 1. 启用组件

在项目的 `prj.conf` 文件中添加：

```kconfig
# 启用 ACOMP 组件
CONFIG_ACOMP=y

# 启用 ACOMP 组件的调音组件
CONFIG_ACOMP_TUNER=y
```

### 2. 初始化调音组件

```c
#include "acomp.h"
#include "tuner/acomp_tuner.h"

int main(int argc, char **argv)
{
    // 初始化 ACOMP 框架
    acomp_init();

    // 初始化调音组件
    int ret = acomp_tuner_init();
    if (ret != ACOMP_ERR_OK) {
        printf("Tuner init failed: %d\n", ret);
        return -1;
    }

    return 0;
}
```

### 3. 注册事件回调

```c
#include "tuner/acomp_tuner.h"

void tuner_event_handler(uint32_t event, void *event_data,
                         uint32_t event_data_len, void *priv)
{
    if (event & TUNER_CB_EVENT_STATUS) {
        uint32_t status = *(uint32_t *)event_data;
        printf("Tuner status: %u\n", status);
    } else if (event & TUNER_CB_EVENT_STREAM_UPDATE) {
        // 有新的处理后音频数据可读
        printf("Tuner stream update\n");
    }
}

// 注册回调
int ret = acomp_tuner_add_callback(
    TUNER_CB_EVENT_STATUS | TUNER_CB_EVENT_STREAM_UPDATE,
    tuner_event_handler,
    NULL
);
```

### 4. 启动调音服务

```c
void start_tuner_service(void)
{
    int ret;

    // 1. 启动调音组件
    ret = acomp_tuner_start();
    if (ret != ACOMP_ERR_OK) {
        printf("Tuner start failed: %d\n", ret);
        return;
    }

    // 2. 设置采样率
    acomp_tuner_set_samplerate(16000.0f);

    // 3. 使能调音处理
    acomp_tuner_enable(1);

    printf("Tuner service started\n");
}

void stop_tuner_service(void)
{
    // 1. 关闭调音处理
    acomp_tuner_enable(0);

    // 2. 停止调音组件
    acomp_tuner_stop();

    // 3. 清理资源
    acomp_tuner_cleanup();
}
```

### 5. 创建双向音频流通道

```c
#include "tuner/acomp_tuner.h"

#define TX_CH_INDEX  0
#define RX_CH_INDEX  1

void setup_tuner_streams(void)
{
    // 创建 M2R 流通道（输入原始音频）
    acomp_stream_chn_create_desc_t tx_desc = {
        .cname = "stream.tuner_in",
        .direction = ACOMP_STREAM_DIRECTION_M2R,
        .index = TX_CH_INDEX,
        .buffer_size = CONFIG_ACOMP_TUNER_STREAM_BUFFER_SIZE,
        .num_descs = CONFIG_ACOMP_TUNER_STREAM_NUM_DESCS,
        .kick_policy = 1,
    };
    acomp_tuner_stream_ch_enable(TX_CH_INDEX, &tx_desc);

    // 创建 R2M 流通道（输出处理后音频）
    acomp_stream_chn_create_desc_t rx_desc = {
        .cname = "stream.tuner_out",
        .direction = ACOMP_STREAM_DIRECTION_R2M,
        .index = RX_CH_INDEX,
        .buffer_size = CONFIG_ACOMP_TUNER_STREAM_BUFFER_SIZE,
        .num_descs = CONFIG_ACOMP_TUNER_STREAM_NUM_DESCS,
        .kick_policy = 1,
    };
    acomp_tuner_stream_ch_enable(RX_CH_INDEX, &rx_desc);
}
```

### 6. 发送音频数据并接收处理结果

```c
void send_audio_to_tuner(uint8_t *audio_data, uint32_t audio_len)
{
    uint8_t *buffer;
    uint32_t buf_size;
    uint16_t desc_idx;

    // 分配发送缓冲区
    buffer = acomp_tuner_stream_tx_buffer_alloc(TX_CH_INDEX, &buf_size, &desc_idx);
    if (buffer && buf_size > 0) {
        memcpy(buffer, audio_data, audio_len);
        acomp_tuner_stream_tx_buffer_submit(TX_CH_INDEX, buffer, audio_len, desc_idx);
    }
}

void receive_processed_audio(void)
{
    uint8_t *buffer;
    uint32_t len;
    uint16_t desc_idx;

    // 获取处理后的音频数据
    buffer = acomp_tuner_stream_rx_buffer_get(RX_CH_INDEX, &len, &desc_idx);
    if (buffer && len > 0) {
        // 播放或转发处理后的音频
        // play_audio(buffer, len);

        // 释放缓冲区
        acomp_tuner_stream_rx_buffer_release(RX_CH_INDEX, desc_idx, len, buffer);
    }
}
```

## API 参考

### 生命周期管理

#### acomp_tuner_init

```c
int acomp_tuner_init(void);
```

**功能**：初始化调音组件

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_NO_MEM`：内存不足
- `ACOMP_ERR_INVALID_STATE`：组件已初始化
- `ACOMP_ERR_NOT_FOUND`：设备未找到
- `ACOMP_ERR_CREATE_STREAM_FAILED`：创建流失败

#### acomp_tuner_start

```c
int acomp_tuner_start(void);
```

**功能**：启动调音组件

**说明**：启动后才能使能调音处理或进行流数据交互

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_INVALID_STATE`：无效状态

#### acomp_tuner_stop

```c
int acomp_tuner_stop(void);
```

**功能**：停止调音组件

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_INVALID_STATE`：无效状态

#### acomp_tuner_cleanup

```c
int acomp_tuner_cleanup(void);
```

**功能**：清理调音组件资源，注销 IPC 回调并释放流对象及回调链表

**说明**：若组件尚未初始化，调用该函数也会直接返回成功

**返回值**：
- `ACOMP_ERR_OK`：成功

### 参数配置

#### acomp_tuner_enable

```c
int acomp_tuner_enable(uint8_t en);
```

**功能**：使能或关闭调音处理

**参数**：
- `en`：使能开关，`0` 表示关闭，非 `0` 表示开启

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_NO_MEM`：内存不足
- `ACOMP_ERR_INVALID_STATE`：无效状态

#### acomp_tuner_set_samplerate

```c
int acomp_tuner_set_samplerate(float sr);
```

**功能**：设置采样率

**参数**：
- `sr`：采样率，单位 Hz

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_NO_MEM`：内存不足
- `ACOMP_ERR_INVALID_STATE`：无效状态

#### acomp_tuner_set_param

```c
int acomp_tuner_set_param(void *param, uint32_t len);
```

**功能**：设置调音参数（EQ/DRC 参数）

**参数**：
- `param`：参数数据缓冲区指针
- `len`：参数数据长度(字节)

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_INVALID_ARG`：参数错误
- `ACOMP_ERR_NO_MEM`：内存不足
- `ACOMP_ERR_INVALID_STATE`：无效状态

#### acomp_tuner_set_limiter

```c
int acomp_tuner_set_limiter(void *param, uint32_t len);
```

**功能**：设置限幅器参数

**参数**：
- `param`：限幅器参数数据缓冲区指针
- `len`：限幅器参数数据长度(字节)

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_INVALID_ARG`：参数错误
- `ACOMP_ERR_NO_MEM`：内存不足
- `ACOMP_ERR_INVALID_STATE`：无效状态

#### acomp_tuner_set_volume

```c
int acomp_tuner_set_volume(float vol);
```

**功能**：设置输出音量

**参数**：
- `vol`：音量系数

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_NO_MEM`：内存不足
- `ACOMP_ERR_INVALID_STATE`：无效状态

### 事件回调

#### acomp_tuner_add_callback

```c
int acomp_tuner_add_callback(uint32_t events, tuner_event_cb_t cb, void *priv);
```

**功能**：添加事件回调函数

**参数**：
- `events`：事件位掩码，可同时注册多个事件
  - `TUNER_CB_EVENT_STATUS`：状态变更通知，`event_data` 指向 `uint32_t` 状态值
  - `TUNER_CB_EVENT_STREAM_UPDATE`：数据流更新通知，`event_data` 指向 `acomp_stream_channel_t`
- `cb`：回调函数指针
- `priv`：回调函数的私有数据指针

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_NO_MEM`：内存不足
- `ACOMP_ERR_INVALID_STATE`：无效状态

#### acomp_tuner_remove_callback

```c
int acomp_tuner_remove_callback(tuner_event_cb_t cb);
```

**功能**：移除回调函数

**参数**：
- `cb`：待移除的回调函数

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_INVALID_STATE`：无效状态

### 音频流管理

#### acomp_tuner_stream_ch_enable

```c
int acomp_tuner_stream_ch_enable(int chn, acomp_stream_chn_create_desc_t *desc);
```

**功能**：使能流通道

**参数**：
- `chn`：通道索引
- `desc`：通道描述符指针

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_INVALID_STATE`：无效状态
- `ACOMP_ERR_INVALID_ARG`：参数错误
- `ACOMP_ERR_CREATE_STREAM_FAILED`：创建流失败

#### acomp_tuner_stream_ch_disable

```c
int acomp_tuner_stream_ch_disable(int chn);
```

**功能**：禁用流通道

**参数**：
- `chn`：通道索引

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_INVALID_STATE`：无效状态
- `ACOMP_ERR_INVALID_ARG`：参数错误

#### acomp_tuner_stream_tx_buffer_alloc

```c
void *acomp_tuner_stream_tx_buffer_alloc(int chn, uint32_t *len, uint16_t *desc_idx);
```

**功能**：分配 TX 流缓冲区用于向 AP 核发送音频数据

**参数**：
- `chn`：通道索引
- `len`：可用缓冲区长度指针（输出）
- `desc_idx`：描述符索引指针（输出）

**返回值**：缓冲区指针，失败返回 NULL

#### acomp_tuner_stream_tx_buffer_submit

```c
int acomp_tuner_stream_tx_buffer_submit(int chn, void *buffer, uint32_t len, uint16_t desc_idx);
```

**功能**：提交 TX 流缓冲区发送音频数据到 AP 核

**参数**：
- `chn`：通道索引
- `buffer`：缓冲区指针
- `len`：数据长度
- `desc_idx`：描述符索引

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_INVALID_STATE`：无效状态

#### acomp_tuner_stream_kick

```c
int acomp_tuner_stream_kick(int chn);
```

**功能**：主动触发流通道 kick 操作

**参数**：
- `chn`：通道索引

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_INVALID_STATE`：无效状态

#### acomp_tuner_stream_rx_buffer_get

```c
void *acomp_tuner_stream_rx_buffer_get(int chn, uint32_t *len, uint16_t *desc_idx);
```

**功能**：获取 RX 流缓冲区中处理后的音频数据

**参数**：
- `chn`：通道索引
- `len`：数据长度指针（输出）
- `desc_idx`：描述符索引指针（输出）

**返回值**：缓冲区指针，失败返回 NULL

#### acomp_tuner_stream_rx_buffer_release

```c
int acomp_tuner_stream_rx_buffer_release(int chn, uint16_t desc_idx, uint32_t len, void *buffer);
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

## 使用示例

### 完整示例

```c
#include "acomp.h"
#include "tuner/acomp_tuner.h"
#include "lisa_log.h"

#define TAG "tuner_demo"

#define TX_CH  0
#define RX_CH  1

void tuner_event_handler(uint32_t event, void *event_data,
                         uint32_t event_data_len, void *priv)
{
    if (event & TUNER_CB_EVENT_STATUS) {
        uint32_t status = *(uint32_t *)event_data;
        LISA_LOGI(TAG, "Tuner status: %u", status);
    } else if (event & TUNER_CB_EVENT_STREAM_UPDATE) {
        // 有新的处理后音频数据
        uint32_t len;
        uint16_t desc_idx;
        void *buffer = acomp_tuner_stream_rx_buffer_get(RX_CH, &len, &desc_idx);
        if (buffer && len > 0) {
            LISA_LOGI(TAG, "Processed audio received, len: %u", len);
            // play_audio(buffer, len);
            acomp_tuner_stream_rx_buffer_release(RX_CH, desc_idx, len, buffer);
        }
    }
}

void tuner_demo(void)
{
    int ret;

    // 1. 初始化 ACOMP 框架
    acomp_init();

    // 2. 初始化调音组件
    ret = acomp_tuner_init();
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "Tuner init failed: %d", ret);
        return;
    }

    // 3. 注册事件回调
    acomp_tuner_add_callback(
        TUNER_CB_EVENT_STATUS | TUNER_CB_EVENT_STREAM_UPDATE,
        tuner_event_handler, NULL
    );

    // 4. 启动调音组件
    ret = acomp_tuner_start();
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "Tuner start failed: %d", ret);
        return;
    }
    LISA_LOGI(TAG, "Tuner started");

    // 5. 创建双向流通道
    acomp_stream_chn_create_desc_t tx_desc = {
        .cname = "stream.tuner_in",
        .direction = ACOMP_STREAM_DIRECTION_M2R,
        .index = TX_CH,
        .buffer_size = CONFIG_ACOMP_TUNER_STREAM_BUFFER_SIZE,
        .num_descs = CONFIG_ACOMP_TUNER_STREAM_NUM_DESCS,
        .kick_policy = 1,
    };
    acomp_tuner_stream_ch_enable(TX_CH, &tx_desc);

    acomp_stream_chn_create_desc_t rx_desc = {
        .cname = "stream.tuner_out",
        .direction = ACOMP_STREAM_DIRECTION_R2M,
        .index = RX_CH,
        .buffer_size = CONFIG_ACOMP_TUNER_STREAM_BUFFER_SIZE,
        .num_descs = CONFIG_ACOMP_TUNER_STREAM_NUM_DESCS,
        .kick_policy = 1,
    };
    acomp_tuner_stream_ch_enable(RX_CH, &rx_desc);

    // 6. 配置调音参数
    acomp_tuner_set_samplerate(16000.0f);
    acomp_tuner_set_volume(1.0f);

    // 7. 使能调音处理
    acomp_tuner_enable(1);

    // 8. 运行一段时间
    vTaskDelay(pdMS_TO_TICKS(10000));

    // 9. 关闭并清理
    acomp_tuner_enable(0);
    acomp_tuner_stop();
    acomp_tuner_cleanup();
    LISA_LOGI(TAG, "Tuner cleaned up");
}
```

## 注意事项

1. **初始化顺序**：必须先调用 `acomp_init()` 初始化 ACOMP 框架，再调用 `acomp_tuner_init()`
2. **无 prepare 步骤**：Tuner 组件不需要 prepare 步骤，init 后直接 start 即可
3. **使能控制**：调音处理默认关闭，必须调用 `acomp_tuner_enable(1)` 开启
4. **双向流通道**：Tuner 支持 M2R（输入）和 R2M（输出）双向流通道，分别用于发送原始音频和接收处理后音频
5. **音频流管理**：使用音频流接口时，必须成对调用 alloc/submit 和 get/release
6. **事件回调**：回调函数在组件内部线程中执行，应避免长时间阻塞操作
7. **cleanup 安全**：`acomp_tuner_cleanup()` 在组件未初始化时也可安全调用

## 常见问题

**Q: 调音组件初始化失败怎么办？**

A: 检查以下几点：
- 确保 ACOMP 框架已正确初始化
- 检查系统是否有足够的堆内存
- 确认 AP 核固件已正确烧录

**Q: 使能调音后没有音频输出？**

A: 检查：
- 确认 M2R 和 R2M 流通道都已正确创建
- 确认已调用 `acomp_tuner_enable(1)` 开启调音
- 检查采样率是否正确设置
- 确认音量系数不为 0

**Q: 如何动态调整 EQ 参数？**

A: 在运行过程中可以随时调用 `acomp_tuner_set_param()` 更新 EQ/DRC 参数，参数会实时生效。

## 依赖项

- `ACOMP 框架`：算法组件框架
- `ACOMP Stream IPC`：音频流跨核通信
- `lisa_log`：日志系统
- `FreeRTOS`：实时操作系统
- `调音算法库`：EQ/DRC/Limiter 算法引擎
