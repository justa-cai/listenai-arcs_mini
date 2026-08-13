# Lisa BT Audio Framework

蓝牙音频框架 - 支持 A2DP/HFP 的模块化音频处理框架

---

## 📋 目录

- [架构概览](#架构概览)
- [核心特性](#核心特性)
- [快速开始](#快速开始)
- [使用场景](#使用场景)
- [API 参考](#api-参考)
- [配置选项](#配置选项)
- [文件结构](#文件结构)

---

## 架构概览

### 分层设计

```
┌─────────────────────────────────────────┐
│           应用层 (Application)           │
└─────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────┐
│      场景适配层 (bt_sink/bt_source)      │
│  - 接收蓝牙事件                           │
│  - 管理 session 生命周期                  │
│  - 连接 session 与硬件（回调机制）         │
└─────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────┐
│        核心框架层 (core/)                │
│  - Session 管理                          │
│  - 编解码处理                            │
│  - 缓冲管理                              │
│  - 线程调度                              │
└─────────────────────────────────────────┘
           ↓                    ↓
┌────────────────────┐  ┌──────────────────┐
│  蓝牙协议栈适配     │  │  硬件接口抽象     │
│  (bt_audio_adapter)│  │  (interfaces/)   │
│  - A2DP/HFP 桥接   │  │  - lisa_audio    │
└────────────────────┘  │  - null_audio    │
                        └──────────────────┘
```

### 数据流向

**Sink 模式（接收音频）**
```
蓝牙数据 → bt_sink → session 解码 → 回调 → 硬件播放
```

**Source 模式（发送音频）**
```
应用数据 → bt_source → session 编码 → 定时器 → 蓝牙发送
```

### Stream Lifecycle

Source 和 Sink 使用 `core/bt_audio_stream_lifecycle` 统一管理流状态、session 快照和活跃操作计数。场景代码只在短临界区内获取 session/元数据快照，不能持锁调用 session IO、`stop/destroy` 或硬件接口回调，避免关闭流程与数据收发并发访问同一个 session。

---

## 核心特性

### ✨ 解耦设计
- **Session 独立**：不依赖硬件，通过回调机制交互
- **场景适配**：bt_sink/bt_source 连接 session 与硬件
- **硬件抽象**：支持多种音频接口实现

### 🎯 编解码支持
- **SBC**：A2DP 标准编解码
- **mSBC**：HFP 宽带语音（16kHz）
- **CVSD**：HFP 窄带语音（8kHz）

### 🔄 模式支持
- **编码模式**：自动编解码，动态缓冲
- **透传模式**：直接传输，零开销

### 📊 性能优化
- 动态缓冲分配（根据采样率计算）
- 预填充机制（防止启动卡顿）
- 零拷贝队列传输

---

## 快速开始

### Sink 场景（蓝牙耳机/音箱）

接收并播放蓝牙音频（A2DP 音乐播放）

```c
#include "bt_sink/bt_sink.h"
#include "bt_audio_adapter.h"

void app_init(void) {
    // 1. 初始化框架
    bt_audio_framework_init();
    
    // 2. 初始化 bt_sink（会自动注册到 adapter）
    bt_sink_init();
    
    // 3. 对接蓝牙协议栈
    os_task_cb_t *cb = bt_audio_adapter_get_os_task_cb();
    aud_os_init(cb);  // 将回调注册到蓝牙栈
    
    // 完成！等待蓝牙连接，音频会自动播放
}
```

**数据流**：
```
蓝牙 A2DP → adapter → bt_sink → session 解码 → lisa_audio → 扬声器
```

---

### Source 场景（蓝牙电话）

发送音频到蓝牙（A2DP/HFP 音频发送）

```c
#include "bt_source/bt_audio_interface_virtual.h"

void app_init(void) {
    // 1. 初始化虚拟接口
    bt_vintf_init();
    
    // 2. 打开播放流（A2DP）
    vintf_open_info_t open_info = {
        .type = VINTF_PROFILE_PLAYBACK,
        .open_complete_callback = on_open_complete,
    };
    vintf_profile_open(&open_info);
}

void send_audio(void) {
    // 写入 PCM 数据，框架自动编码并发送
    uint8_t pcm_data[320];  // 16kHz, 16bit, mono, 10ms
    vintf_playback_write(pcm_data, sizeof(pcm_data));
}
```

**数据流**：
```
应用 PCM → bt_source → session 编码 → 定时器发送 → 蓝牙
```

---

## 配置选项

### 基础配置

```kconfig
# 启用框架
CONFIG_LISA_BT_AUDIO_FRAMEWORK=y

# 最大会话数
CONFIG_LISA_BT_AUDIO_MAX_SESSIONS=2

# 线程配置
CONFIG_LISA_BT_AUDIO_DECODE_TASK_STACK_SIZE=4096
CONFIG_LISA_BT_AUDIO_DECODE_TASK_PRIORITY=5
CONFIG_LISA_BT_AUDIO_PLAYBACK_TASK_STACK_SIZE=2048
CONFIG_LISA_BT_AUDIO_PLAYBACK_TASK_PRIORITY=6
```

### 场景选择

```kconfig
# Sink 场景（接收音频）
CONFIG_LISA_BT_AUDIO_INTERFACE_LOCAL=y

# Source 场景（发送音频）
CONFIG_LISA_BT_AUDIO_INTERFACE_VIRTUAL=y

# 空接口（测试用）
CONFIG_LISA_BT_AUDIO_INTERFACE_NULL=y
```

### Codec 支持

```kconfig
CONFIG_AUDIO_CODEC_SBC=y    # A2DP 标准
CONFIG_AUDIO_CODEC_MSBC=y   # HFP 宽带
```

### 缓冲区配置

```kconfig
# PCM 缓冲区时长（毫秒）
CONFIG_LISA_BT_AUDIO_PCM_RINGBUF_TIME_MS_DOWNLINK=300
CONFIG_LISA_BT_AUDIO_PCM_RINGBUF_TIME_MS_UPLINK=300

# 工作缓冲区时长（毫秒）
CONFIG_LISA_BT_AUDIO_WORK_BUFFER_TIME_MS=100

# 预填充时长（毫秒）
CONFIG_LISA_BT_AUDIO_PCM_PREFILL_TIME_MS=60
```

### 调试选项

```kconfig
CONFIG_LISA_BT_AUDIO_DEBUG=y  # 启用详细日志
```

---

## 支持的 Codec

| Codec | 采样率 | 比特率 | 应用场景 |
|-------|--------|--------|----------|
| **SBC** | 16/32/44.1/48 kHz | 可变 | A2DP 音乐播放 |
| **mSBC** | 16 kHz | 固定 | HFP 宽带语音 |
| **CVSD** | 8 kHz | 64 kbps | HFP 窄带语音 |
| **LC3** | 8-48 kHz | 可变 | LE Audio |

---

## 使用场景

### 场景 1：蓝牙音箱（Sink）

**需求**：接收手机音乐，通过扬声器播放

**配置**：
```kconfig
CONFIG_LISA_BT_AUDIO_FRAMEWORK=y
CONFIG_LISA_BT_AUDIO_INTERFACE_LOCAL=y  # 使用 lisa_audio 硬件
CONFIG_AUDIO_CODEC_SBC=y                # 启用 SBC 解码
```

**实现**：
```c
bt_audio_framework_init();
bt_sink_init();  // 自动处理蓝牙音频接收和播放
```

---

### 场景 2：蓝牙电话（Source）

**需求**：将麦克风音频发送到蓝牙耳机

**配置**：
```kconfig
CONFIG_LISA_BT_AUDIO_FRAMEWORK=y
CONFIG_LISA_BT_AUDIO_INTERFACE_VIRTUAL=y  # 虚拟接口（Source模式）
CONFIG_AUDIO_CODEC_MSBC=y                 # HFP 宽带语音
```

**实现**：
```c
bt_vintf_init();
vintf_profile_open(&open_info);
// 持续调用 vintf_playback_write 发送音频
```

---

### 场景 3：音频算法处理

**需求**：接收蓝牙音频，进行 AI 处理后播放

**方案 1：使用回调获取 PCM**
```c
// bt_sink 会通过回调传递解码后的 PCM
static int pcm_callback(const void *pcm, size_t size, void *user_data) {
    // 在这里进行 AI 处理
    ai_process(pcm, size);
    // 处理后写入硬件
    lisa_audio_write(pcm, size);
    return size;
}
```

**方案 2：使用 null_audio 接口**
```c
CONFIG_LISA_BT_AUDIO_INTERFACE_NULL=y  // 空接口，不自动播放
```

---

## API 参考

### 核心框架 API

```c
// bt_audio_framework.h
bt_audio_error_t bt_audio_framework_init(void);
bt_audio_error_t bt_audio_framework_deinit(void);
```

### Sink 场景 API

```c
// bt_sink/bt_sink.h
bt_audio_error_t bt_sink_init(void);
bt_audio_error_t bt_sink_set_audio_interface(const bt_audio_interface_ops_t *ops);
```

### Source 场景 API

```c
// bt_source/bt_audio_interface_virtual.h

// 初始化
bt_audio_error_t bt_vintf_init(void);
bt_audio_error_t bt_vintf_deinit(void);

// 打开/关闭音频流
bt_audio_error_t vintf_profile_open(vintf_open_info_t *info);
bt_audio_error_t vintf_profile_close(void);

// 发送音频数据
int vintf_playback_write(const void *buffer, size_t size);

// 等待已写入的播放数据排空，成功后再关闭可避免尾音丢失
bt_audio_error_t vintf_playback_drain(uint32_t timeout_ms);

// 安全关闭播放流示例
if (vintf_playback_drain(5000) == BT_AUDIO_OK) {
    vintf_profile_close();
}

// 模式配置
bt_audio_error_t bt_vintf_set_encode_mode(bool encode_pcm);
bt_audio_error_t bt_vintf_set_frame_params(uint32_t duration_us, size_t size_bytes);
```

### Session API（高级用法）

```c
// core/bt_audio_session.h

// 创建和销毁
bt_audio_error_t bt_audio_session_create(
    const bt_audio_session_config_t *config,
    bt_audio_session_handle_t *session);
bt_audio_error_t bt_audio_session_destroy(bt_audio_session_handle_t session);

// 启动和停止
bt_audio_error_t bt_audio_session_start(
    bt_audio_session_handle_t session,
    const bt_audio_codec_config_t *codec_config);
bt_audio_error_t bt_audio_session_stop(bt_audio_session_handle_t session);

// 数据接口
bt_audio_error_t bt_audio_session_playback_write(
    bt_audio_session_handle_t session,
    const void *data, size_t size);
bt_audio_error_t bt_audio_session_capture_write_pcm(
    bt_audio_session_handle_t session,
    const void *pcm_data, size_t size);
bt_audio_error_t bt_audio_session_capture_read_frame(
    bt_audio_session_handle_t session,
    void *buffer, size_t buffer_size, size_t *frame_size);
bt_audio_error_t bt_audio_session_capture_is_drained(
    bt_audio_session_handle_t session,
    bool *drained);
```

### Adapter API

```c
// bt_audio_adapter.h
bt_audio_error_t bt_audio_adapter_init(void);
os_task_cb_t *bt_audio_adapter_get_os_task_cb(void);
int bt_audio_adapter_send_frames(uint8_t conidx, const uint8_t *data, 
                                  size_t frame_count, size_t total_size);
```

---

## 文件结构

```
lisa_bt_audio_framework/
├── 📄 README.md                       # 本文档
├── 📄 Kconfig                         # 配置选项
├── 📄 CMakeLists.txt                  # 构建配置
│
├── 📄 bt_audio_adapter.c/.h           # 蓝牙协议栈适配器
│
├── 📁 core/                           # 🎯 核心框架
│   ├── bt_audio_types.h               # 基础类型定义
│   ├── aud_common.h                   # 音频通用定义
│   ├── bt_audio_framework.c/.h        # 框架初始化
│   ├── bt_audio_session_mgr.c         # 会话管理（编解码+缓冲+线程）
│   ├── bt_audio_session.h             # 会话 API
│   └── bt_audio_codec_mgr.c           # Codec 注册管理
│
├── 📁 codecs/                         # 🎵 编解码器
│   ├── bt_audio_codec.h               # Codec 接口定义
│   ├── bt_audio_codec_sbc.c           # SBC 编解码
│   ├── bt_audio_codec_msbc.c          # mSBC 编解码
│   └── bt_audio_codec_lc3.c           # LC3 编解码
│
├── 📁 bt_sink/                        # 🔊 Sink 场景（接收音频）
│   ├── bt_sink.c/.h                   # Sink 适配器
│   └── CMakeLists.txt                 # 构建配置
│
├── 📁 bt_source/                      # 🎤 Source 场景（发送音频）
│   ├── bt_audio_interface_virtual.c/.h # 虚拟接口实现
│   └── CMakeLists.txt                 # 构建配置
│
└── 📁 interfaces/                     # 🔌 硬件接口抽象
    ├── bt_audio_interface.h           # 接口定义
    ├── bt_audio_interface_mgr.c       # 接口注册管理
    ├── lisa_audio_interface.c         # lisa_audio 硬件封装
    ├── null_audio_interface.c         # 空实现（测试用）
    └── CMakeLists.txt                 # 构建配置
```

### 目录说明

| 目录 | 职责 | 依赖关系 |
|------|------|----------|
| **core/** | 核心会话管理、编解码调度 | 不依赖硬件，通过回调交互 |
| **codecs/** | 各种 Codec 实现 | 独立模块，按需编译 |
| **bt_sink/** | Sink 场景适配器 | 依赖 core + interfaces |
| **bt_source/** | Source 场景适配器 | 依赖 core，无硬件依赖 |
| **interfaces/** | 硬件接口实现 | 可替换，支持多种实现 |

---

## 关键设计原则

### 1. 解耦设计
- **Session 独立于硬件**：通过回调机制（`playback_data_callback`, `capture_data_callback`）与外部交互
- **场景适配层**：bt_sink/bt_source 负责连接 session 和硬件
- **硬件抽象**：通过 `bt_audio_interface_ops_t` 支持多种硬件实现

### 2. 回调机制
```c
// Session 配置时注册回调
bt_audio_session_config_t config = {
    .playback_data_callback = on_pcm_data,  // Session → 外部
    .capture_data_callback = request_data,  // 外部 → Session
};
```

### 3. 零拷贝传输
- 使用队列传输编码帧（帧对齐保证）
- PCM 数据使用环形缓冲区
- 避免不必要的内存拷贝

### 4. 动态资源分配
- 缓冲区大小根据采样率动态计算
- 透传模式按需分配资源
- PSRAM 用于大缓冲区

---

## 常见问题

### Q: 如何选择 Sink 还是 Source？
- **Sink**：接收蓝牙音频（耳机、音箱）
- **Source**：发送音频到蓝牙（电话、对讲）

### Q: 编码模式 vs 透传模式？
- **编码模式**：框架自动编解码，适合标准应用
- **透传模式**：直接传输已编码数据，适合特殊需求

### Q: 如何获取解码后的 PCM？
在 session 配置中注册 `playback_data_callback`：
```c
static int on_pcm(const void *pcm, size_t size, void *user_data) {
    // 处理 PCM 数据
    return size;
}
```

### Q: 如何优化延迟？
1. 减小缓冲区时长（`PCM_RINGBUF_TIME_MS`）
2. 减小预填充时长（`PCM_PREFILL_TIME_MS`）
3. 提高线程优先级

---

## 性能指标

| 指标 | 典型值 | 说明 |
|------|--------|------|
| 启动延迟 | 60-100ms | 取决于预填充配置 |
| 内存占用 | 50-100KB | 包含缓冲区和线程栈 |
| CPU 占用 | 5-15% | SBC 解码，48kHz 立体声 |
| 线程数量 | 2-4个 | decode/encode/playback 线程 |

---

## 示例项目

- `samples/network/bt_audio_sink` - Sink 场景示例
- `samples/network/bt_audio_source` - Source 场景示例
- `samples/network/bt_audio_hfp_source` - HFP 全双工示例

---

**最后更新**：2026-02-10
