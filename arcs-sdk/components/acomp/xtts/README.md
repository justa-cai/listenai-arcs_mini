# 语音合成组件

## 简介

ACOMP XTTS (Text-to-Speech) 是 ARCS SDK 的语音合成组件，提供基于文本输入的语音合成功能。该组件支持中英文语音合成，支持多种发音人角色切换，并提供语速、音量等参数配置接口，合成后的 PCM 音频通过 R2M 流通道输出。

## 主要特性

- **中英文语音合成**：支持中文和英文文本的语音合成
- **多发音人支持**：支持多种发音人角色切换（如凌小琪、Lucy 等）
- **语速控制**：支持语音播报速度调节
- **音量控制**：支持语音播报音量调节
- **事件回调机制**：支持合成状态通知和音频流更新事件
- **PCM 音频输出**：合成的 PCM 音频通过 R2M 流通道输出，应用层可直接获取播放
- **快捷 API**：提供 `acomp_xtts_do_prepare()` / `acomp_xtts_do_cleanup()` 快捷接口，简化资源管理
- **角色热切换**：支持通过 `acomp_xtts_do_prepare_with_role()` 在不同发音人之间切换，相同角色复用已有实例
- **跨核通信**：基于 IPC 机制实现文本和音频数据的跨核传输

## 配置选项

### 基本配置

| 配置项 | 说明 | 默认值 |
|--------|------|--------|
| `CONFIG_ACOMP` | 启用 ACOMP 组件 | n |
| `CONFIG_ACOMP_XTTS` | 启用 ACOMP 组件的 XTTS 组件 | n |

### 资源配置

XTTS 组件的资源分为 Flash 资源和 eMMC 资源两部分：

**eMMC 资源**:

| 配置项 | 说明 | 默认值 |
|--------|------|--------|
| `CONFIG_ACOMP_XTTS_RES_CNCN_DICT_EMMC_ADDR` | 中文字典 eMMC 地址 | 0x5f00000 |
| `CONFIG_ACOMP_XTTS_RES_CNCN_DICT_SIZE` | 中文字典大小(字节) | 3166858 |
| `CONFIG_ACOMP_XTTS_RES_ENUS_DICT_EMMC_ADDR` | 英文字典 eMMC 地址 | 0x6400000 |
| `CONFIG_ACOMP_XTTS_RES_ENUS_DICT_SIZE` | 英文字典大小(字节) | 2470275 |
| `CONFIG_ACOMP_XTTS_RES_CRF_EMMC_ADDR` | CRF 模型 eMMC 地址 | 0x6900000 |
| `CONFIG_ACOMP_XTTS_RES_CRF_SIZE` | CRF 模型大小(字节) | 7791842 |
| `CONFIG_ACOMP_XTTS_RES_FRONT_USER_EMMC_ADDR` | 前端用户字典 eMMC 地址 | 0x360c000 |
| `CONFIG_ACOMP_XTTS_RES_FRONT_USER_SIZE` | 前端用户字典大小(字节) | 3082688 |
| `CONFIG_ACOMP_XTTS_RES_REAR_EMMC_ADDR` | 后端模型 eMMC 地址 | 0x7d00000 |
| `CONFIG_ACOMP_XTTS_RES_REAR_SIZE` | 后端模型大小(字节) | 4273824 |
| `CONFIG_ACOMP_XTTS_RES_REAR_NHV_EMMC_ADDR` | 后端 NHV 模型 eMMC 地址 | 0x8200000 |
| `CONFIG_ACOMP_XTTS_RES_REAR_NHV_SIZE` | 后端 NHV 模型大小(字节) | 284992 |

**Flash 资源 (通过 resmgr)**:

| 配置项 | 说明 | 默认值 |
|--------|------|--------|
| `CONFIG_ACOMP_XTTS_RES_ROLE_RESID` | 角色资源在 Flash resmgr 中的 ID | 5 |
| `CONFIG_ACOMP_XTTS_RES_FRONT_RESID` | 前端资源在 Flash resmgr 中的 ID | 6 |

**流通道配置**:

| 配置项 | 说明 | 默认值 |
|--------|------|--------|
| `CONFIG_ACOMP_XTTS_STREAM_BUFFER_SIZE` | 流缓冲区大小 | 8192 |
| `CONFIG_ACOMP_XTTS_STREAM_NUM_DESCS` | 流描述符数量 | 32 |

## 快速开始

### 方式一：使用快捷 API（推荐）

快捷 API 封装了完整的初始化、资源加载、流通道创建和启动流程：

```c
#include "xtts/acomp_xtts.h"

void xtts_event_handler(uint32_t event, void *event_data,
                        uint32_t event_data_len, void *priv)
{
    if (event & XTTS_CB_EVENT_STATUS) {
        uint32_t status = *(uint32_t *)event_data;
        printf("XTTS status: %u\n", status);
    } else if (event & XTTS_CB_EVENT_STREAM_UPDATE) {
        // 有新的 PCM 数据可读
        printf("XTTS stream update\n");
    }
}

void xtts_quick_start(void)
{
    // 初始化 ACOMP 框架
    acomp_init();
    
    // 一键准备并启动 XTTS（使用默认角色凌小琪）
    int ret = acomp_xtts_do_prepare(xtts_event_handler, NULL);
    if (ret != 0) {
        printf("XTTS prepare failed: %d\n", ret);
        return;
    }
    
    // 合成文本
    const char *text = "你好，世界";
    acomp_xtts_synth_text(text, strlen(text));
    
    // 读取合成的 PCM 音频
    uint32_t len;
    uint16_t desc_idx;
    void *buffer = acomp_xtts_stream_rx_buffer_get(0, &len, &desc_idx);
    if (buffer && len > 0) {
        // 播放 PCM 音频数据
        // play_audio(buffer, len);
        acomp_xtts_stream_rx_buffer_release(0, desc_idx, len, buffer);
    }
    
    // 清理
    acomp_xtts_do_cleanup();
}
```

### 方式一（扩展）：使用指定角色

```c
// 使用 Lucy 英文角色
int ret = acomp_xtts_do_prepare_with_role(2, xtts_event_handler, NULL);

// 合成英文
const char *text = "Hello, world";
acomp_xtts_synth_text(text, strlen(text));
```

### 方式二：使用标准 API

#### 1. 启用组件

在项目的 `prj.conf` 文件中添加：

```kconfig
# 启用 ACOMP 组件
CONFIG_ACOMP=y

# 启用 ACOMP 组件的 XTTS 组件
CONFIG_ACOMP_XTTS=y
```

#### 2. 初始化 XTTS 组件

```c
#include "acomp.h"
#include "xtts/acomp_xtts.h"

int main(int argc, char **argv)
{
    // 初始化 ACOMP 框架
    acomp_init();
    
    // 初始化 XTTS 组件
    int ret = acomp_xtts_init();
    if (ret != ACOMP_ERR_OK) {
        printf("XTTS init failed: %d\n", ret);
        return -1;
    }
    
    return 0;
}
```

#### 3. 注册事件回调

```c
#include "xtts/acomp_xtts.h"

void xtts_event_handler(uint32_t event, void *event_data,
                        uint32_t event_data_len, void *priv)
{
    if (event & XTTS_CB_EVENT_STATUS) {
        uint32_t status = *(uint32_t *)event_data;
        printf("XTTS status: %u\n", status);
    } else if (event & XTTS_CB_EVENT_STREAM_UPDATE) {
        printf("XTTS stream update\n");
    }
}

// 注册回调
int ret = acomp_xtts_add_callback(
    XTTS_CB_EVENT_STATUS | XTTS_CB_EVENT_STREAM_UPDATE,
    xtts_event_handler,
    NULL
);
```

#### 4. 准备资源并启动

```c
void start_xtts_service(acomp_ipc_prepare_t *prepare)
{
    int ret;
    
    // 1. 就绪组件（传入资源配置）
    ret = acomp_xtts_prepare(prepare);
    if (ret != ACOMP_ERR_OK) {
        printf("XTTS prepare failed: %d\n", ret);
        return;
    }
    
    // 2. 创建 R2M 流通道（必须在 start 之前）
    acomp_stream_chn_create_desc_t desc = {
        .cname = "stream.xtts_pcm",
        .direction = ACOMP_STREAM_DIRECTION_R2M,  // Remote to Master
        .index = 0,
        .buffer_size = CONFIG_ACOMP_XTTS_STREAM_BUFFER_SIZE,
        .num_descs = CONFIG_ACOMP_XTTS_STREAM_NUM_DESCS,
        .kick_policy = 0,
    };
    acomp_xtts_stream_ch_enable(0, &desc);
    
    // 3. 启动 XTTS
    ret = acomp_xtts_start();
    if (ret != ACOMP_ERR_OK) {
        printf("XTTS start failed: %d\n", ret);
        return;
    }
    
    printf("XTTS service started\n");
}
```

#### 5. 合成文本并接收音频

```c
void synth_and_play(const char *text)
{
    // 1. 提交合成文本
    int ret = acomp_xtts_synth_text(text, strlen(text));
    if (ret != ACOMP_ERR_OK) {
        printf("Synth text failed: %d\n", ret);
        return;
    }
    
    // 2. 在 XTTS_CB_EVENT_STREAM_UPDATE 回调中读取 PCM 数据
    uint32_t len;
    uint16_t desc_idx;
    void *buffer = acomp_xtts_stream_rx_buffer_get(0, &len, &desc_idx);
    if (buffer && len > 0) {
        // 播放 PCM 音频
        // play_audio(buffer, len);
        
        // 释放缓冲区
        acomp_xtts_stream_rx_buffer_release(0, desc_idx, len, buffer);
    }
}
```

## API 参考

### 生命周期管理

#### acomp_xtts_init

```c
int acomp_xtts_init(void);
```

**功能**：初始化 XTTS 组件

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_NO_MEM`：内存不足
- `ACOMP_ERR_INVALID_STATE`：组件已初始化
- `ACOMP_ERR_NOT_FOUND`：设备未找到

#### acomp_xtts_prepare

```c
int acomp_xtts_prepare(acomp_ipc_prepare_t *prepare);
```

**功能**：就绪 XTTS 组件，将资源配置发送给 AP 端

**参数**：
- `prepare`：资源配置数据结构指针

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_INVALID_ARG`：参数错误

#### acomp_xtts_start

```c
int acomp_xtts_start(void);
```

**功能**：启动 XTTS 组件

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_INVALID_STATE`：无效状态

#### acomp_xtts_stop

```c
int acomp_xtts_stop(void);
```

**功能**：停止 XTTS 组件

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_INVALID_STATE`：无效状态

#### acomp_xtts_cleanup

```c
int acomp_xtts_cleanup(void);
```

**功能**：清理 XTTS 组件资源，注销 IPC 回调并释放流对象

**返回值**：
- `ACOMP_ERR_OK`：成功

### 快捷 API

#### acomp_xtts_do_prepare

```c
int acomp_xtts_do_prepare(xtts_event_cb_t event_cb, void *cb_priv);
```

**功能**：一键准备并启动 XTTS 组件（使用默认中文角色凌小琪）

**说明**：封装了 init、回调注册、资源加载（Flash + eMMC）、流通道创建和 start 的完整流程。资源地址来自 Kconfig 配置。不会清理其他算法组件，调用方需自行协调。

**参数**：
- `event_cb`：事件回调函数（首次调用时注册，可为 NULL）
- `cb_priv`：回调函数的私有数据指针

**返回值**：
- `0`：成功
- 负值：错误

#### acomp_xtts_do_prepare_with_role

```c
int acomp_xtts_do_prepare_with_role(int role, xtts_event_cb_t event_cb, void *cb_priv);
```

**功能**：使用指定角色准备并启动 XTTS 组件

**说明**：如果请求的角色与当前已准备的角色相同，则复用现有实例；否则先停止、清理再重新准备。

**参数**：
- `role`：角色标识
- `event_cb`：事件回调函数（首次调用时注册，可为 NULL）
- `cb_priv`：回调函数的私有数据指针

**返回值**：
- `0`：成功
- 负值：错误

#### acomp_xtts_do_cleanup

```c
int acomp_xtts_do_cleanup(void);
```

**功能**：一键停止并清理 XTTS 组件

**返回值**：
- `0`：成功

### 合成控制

#### acomp_xtts_synth_text

```c
int acomp_xtts_synth_text(const char *text, uint32_t len);
```

**功能**：提交文本进行语音合成

**参数**：
- `text`：待合成的文本字符串
- `len`：文本长度（字节）

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_INVALID_ARG`：text 为 NULL 或 len 为 0
- `ACOMP_ERR_NO_MEM`：内存不足
- `ACOMP_ERR_INVALID_STATE`：无效状态

### 参数配置

#### acomp_xtts_set_speed

```c
int acomp_xtts_set_speed(int speed);
```

**功能**：设置语音合成速度

**参数**：
- `speed`：语速值

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_INVALID_STATE`：无效状态

#### acomp_xtts_set_volume

```c
int acomp_xtts_set_volume(int volume);
```

**功能**：设置语音合成音量

**参数**：
- `volume`：音量值

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_INVALID_STATE`：无效状态

#### acomp_xtts_set_role

```c
int acomp_xtts_set_role(int role);
```

**功能**：设置发音人角色

**参数**：
- `role`：角色标识

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_INVALID_STATE`：无效状态

### 事件回调

#### acomp_xtts_add_callback

```c
int acomp_xtts_add_callback(uint32_t events, xtts_event_cb_t cb, void *priv);
```

**功能**：添加事件回调函数

**参数**：
- `events`：事件位掩码，可同时注册多个事件
  - `XTTS_CB_EVENT_STATUS`：合成状态通知
  - `XTTS_CB_EVENT_STREAM_UPDATE`：音频流数据更新（有新的 PCM 数据可读）
- `cb`：回调函数指针
- `priv`：回调函数的私有数据指针

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_INVALID_STATE`：无效状态

#### acomp_xtts_remove_callback

```c
int acomp_xtts_remove_callback(xtts_event_cb_t cb);
```

**功能**：移除回调函数

**参数**：
- `cb`：待移除的回调函数

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_INVALID_STATE`：无效状态

### 音频流管理

#### acomp_xtts_stream_ch_enable

```c
int acomp_xtts_stream_ch_enable(int chn, acomp_stream_chn_create_desc_t *desc);
```

**功能**：使能音频流通道，用于接收合成的 PCM 音频

**说明**：流通道必须在 `acomp_xtts_start()` 之前创建，否则 AP 核的 PCM 输出将被丢弃

**参数**：
- `chn`：通道索引
- `desc`：通道描述符指针

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_INVALID_STATE`：无效状态
- `ACOMP_ERR_INVALID_ARG`：参数错误
- `ACOMP_ERR_CREATE_STREAM_FAILED`：创建流失败

#### acomp_xtts_stream_ch_disable

```c
int acomp_xtts_stream_ch_disable(int chn);
```

**功能**：禁用音频流通道

**参数**：
- `chn`：通道索引

**返回值**：
- `ACOMP_ERR_OK`：成功
- `ACOMP_ERR_INVALID_STATE`：无效状态
- `ACOMP_ERR_INVALID_ARG`：参数错误

#### acomp_xtts_stream_rx_buffer_get

```c
void *acomp_xtts_stream_rx_buffer_get(int chn, uint32_t *len, uint16_t *desc_idx);
```

**功能**：获取 RX 流缓冲区中的 PCM 音频数据

**参数**：
- `chn`：通道索引
- `len`：数据长度指针（输出）
- `desc_idx`：描述符索引指针（输出）

**返回值**：缓冲区指针，失败返回 NULL

#### acomp_xtts_stream_rx_buffer_release

```c
int acomp_xtts_stream_rx_buffer_release(int chn, uint16_t desc_idx, uint32_t len, void *buffer);
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

## 使用示例

### 完整示例

```c
#include "acomp.h"
#include "xtts/acomp_xtts.h"
#include "lisa_log.h"

#define TAG "xtts_demo"

void xtts_event_handler(uint32_t event, void *event_data,
                        uint32_t event_data_len, void *priv)
{
    if (event & XTTS_CB_EVENT_STATUS) {
        uint32_t status = *(uint32_t *)event_data;
        LISA_LOGI(TAG, "XTTS status: %u", status);
    } else if (event & XTTS_CB_EVENT_STREAM_UPDATE) {
        // 有新的 PCM 数据，在此处读取并播放
        uint32_t len;
        uint16_t desc_idx;
        void *buffer = acomp_xtts_stream_rx_buffer_get(0, &len, &desc_idx);
        if (buffer && len > 0) {
            LISA_LOGI(TAG, "PCM data received, len: %u", len);
            // play_audio(buffer, len);
            acomp_xtts_stream_rx_buffer_release(0, desc_idx, len, buffer);
        }
    }
}

void xtts_demo(void)
{
    int ret;
    
    // 1. 初始化 ACOMP 框架
    acomp_init();
    
    // 2. 一键准备并启动 XTTS
    ret = acomp_xtts_do_prepare(xtts_event_handler, NULL);
    if (ret != 0) {
        LISA_LOGE(TAG, "XTTS prepare failed: %d", ret);
        return;
    }
    LISA_LOGI(TAG, "XTTS started");
    
    // 3. 设置语音参数
    acomp_xtts_set_speed(100);
    acomp_xtts_set_volume(100);
    
    // 4. 合成中文文本
    const char *text = "你好，欢迎使用语音合成功能";
    ret = acomp_xtts_synth_text(text, strlen(text));
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "Synth failed: %d", ret);
    }
    
    // 5. 等待合成完成
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    // 6. 清理
    acomp_xtts_do_cleanup();
    LISA_LOGI(TAG, "XTTS cleaned up");
}
```

### 角色切换示例

```c
void xtts_role_switch_demo(void)
{
    acomp_init();
    
    // 使用凌小琪中文角色
    acomp_xtts_do_prepare_with_role(1, xtts_event_handler, NULL);
    acomp_xtts_synth_text("你好", 6);
    
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    // 切换到 Lucy 英文角色（会自动清理并重新准备）
    acomp_xtts_do_prepare_with_role(2, xtts_event_handler, NULL);
    acomp_xtts_synth_text("Hello", 5);
    
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    // 再次使用同一角色（复用现有实例，无需重新准备）
    acomp_xtts_do_prepare_with_role(2, xtts_event_handler, NULL);
    acomp_xtts_synth_text("World", 5);
    
    vTaskDelay(pdMS_TO_TICKS(3000));
    acomp_xtts_do_cleanup();
}
```

## 注意事项

1. **初始化顺序**：必须先调用 `acomp_init()` 初始化 ACOMP 框架
2. **流通道时序**：R2M 流通道必须在 `acomp_xtts_start()` 之前创建，否则 AP 核的 PCM 数据将被丢弃
3. **快捷 API 推荐**：推荐使用 `acomp_xtts_do_prepare()` 系列快捷 API，自动处理资源加载和流通道创建
4. **角色切换**：`acomp_xtts_do_prepare_with_role()` 会自动判断是否需要重新准备，相同角色复用现有实例
5. **算法协调**：快捷 API 不会清理其他算法组件（如 CV、Translation），调用方需自行管理算法生命周期
6. **事件回调**：回调函数在组件内部线程中执行，应避免长时间阻塞操作
7. **PCM 缓冲区**：使用 RX 流接口时，必须成对调用 `rx_buffer_get` / `rx_buffer_release`
8. **资源配置**：确保 Flash 和 eMMC 中的模型资源已正确烧录，地址和大小与 Kconfig 配置匹配
9. **跨核通信**：音频流基于 IPC 机制，注意跨核数据传输的延迟和同步问题

## 常见问题

**Q: XTTS 组件初始化失败怎么办？**

A: 检查以下几点：
- 确保 ACOMP 框架已正确初始化
- 检查系统是否有足够的堆内存
- 确认 AP 核固件已正确烧录

**Q: 合成后没有收到 PCM 音频数据？**

A: 检查：
- 确认 R2M 流通道在 start 之前已创建
- 确认 `XTTS_CB_EVENT_STREAM_UPDATE` 事件回调已注册
- 检查合成文本是否为空
- 确认资源（字典、模型等）已正确加载

**Q: 如何切换发音人？**

A: 推荐使用 `acomp_xtts_do_prepare_with_role()` 接口，传入不同的 role 值。如果新角色与当前不同，组件会自动执行清理和重新准备。

**Q: 合成速度很慢怎么办？**

A: 建议：
- 减少每次合成的文本长度
- 确保 AP 核没有被其他算法任务占用
- 检查 eMMC 读取速度是否正常

## 依赖项

- `ACOMP 框架`：算法组件框架
- `ACOMP Stream IPC`：音频流跨核通信
- `resmgr`：Flash 资源管理器
- `lisa_log`：日志系统
- `lisa_mem`：内存管理
- `FreeRTOS`：实时操作系统
- `XTTS 算法库`：语音合成算法引擎
