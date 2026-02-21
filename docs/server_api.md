# ListenAI V1.1 嵌入式设备 API 文档

## 概述

ListenAI 采用解耦架构，包含三个独立的 WebSocket 服务：
- **ASR Service** - 语音识别 (端口 9200)
- **LLM Gateway** - 大语言模型网关 (端口 9400)  
- **TTS Service** - 语音合成 (端口 9300)

---

## 1. ASR 语音识别服务

### 连接信息

| 参数 | 值 |
|------|-----|
| 协议 | WebSocket |
| 地址 | `ws://{host}:9200` |
| 数据格式 | 二进制 (PCM) / JSON |

### 客户端发送

#### 音频数据流

| 参数 | 值 |
|------|-----|
| 格式 | 二进制 PCM |
| 采样率 | 16000 Hz |
| 位深 | 16-bit signed integer (Little-Endian) |
| 声道 | 单声道 |
| 发送方式 | 实时流式发送音频块 |

```c
// 示例: 将 float32 转换为 int16 PCM
int16_t sample = (int16_t)(float_sample < 0 ? 
    float_sample * 0x8000 : float_sample * 0x7FFF);
```

### 服务端响应

JSON 格式消息:

```json
{
    "text": "识别的文本内容",
    "is_final": false
}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `text` | string | 识别出的文本 |
| `is_final` | boolean | `true` = 最终结果, `false` = 中间结果 |

---

## 2. LLM Gateway 大语言模型网关

### 连接信息

| 参数 | 值 |
|------|-----|
| 协议 | WebSocket |
| 地址 | `ws://{host}:9400` |
| 数据格式 | JSON |

### 客户端发送

#### 文本输入请求

```json
{
    "type": "text_input",
    "text": "用户输入的文本",
    "session_id": "可选-会话ID"
}
```

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `type` | string | 是 | 固定值 `text_input` |
| `text` | string | 是 | 用户文本输入 |
| `session_id` | string | 否 | 会话ID,用于保持上下文 |

#### 心跳请求

```json
{
    "type": "ping"
}
```

### 服务端响应

#### 状态消息

```json
{
    "type": "status",
    "status": "connected",
    "data": {
        "session_id": "生成的会话ID"
    }
}
```

#### LLM 响应

```json
{
    "type": "llm_response",
    "content": "AI 回复的文本内容"
}
```

#### 工具调用

```json
{
    "type": "tool_call",
    "tool_name": "工具名称",
    "arguments": { "参数": "值" },
    "result": { "结果": "值" }
}
```

#### 错误消息

```json
{
    "type": "error",
    "code": "ERROR_CODE",
    "message": "错误描述"
}
```

#### 心跳响应

```json
{
    "type": "pong"
}
```

---

## 3. TTS 语音合成服务

### 连接信息

| 参数 | 值 |
|------|-----|
| 协议 | WebSocket |
| 地址 | `ws://{host}:9300/tts` |
| 数据格式 | JSON (请求) / 二进制帧 (响应) |

### 客户端发送

#### TTS 合成请求

```json
{
    "type": "tts_request",
    "request_id": "唯一请求ID",
    "params": {
        "text": "要合成的文本",
        "mode": "streaming",
        "voice_id": "音色ID"
    }
}
```

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `type` | string | 是 | 固定值 `tts_request` |
| `request_id` | string | 是 | 唯一请求标识 (UUID) |
| `params.text` | string | 是 | 待合成文本 |
| `params.mode` | string | 是 | `streaming` (流式) |
| `params.voice_id` | string | 是 | 音色标识 |

#### 可用音色

| voice_id | 描述 |
|----------|------|
| `全部-湾湾小何` | 湾湾小何 (默认) |
| `通用场景-甜美悦悦` | 甜美悦悦 |
| `通用场景-阳光青年` | 阳光青年 |
| `通用场景-清新女声` | 清新女声 |
| `通用场景-温暖阿虎` | 温暖阿虎 |

### 服务端响应

#### 音频数据帧 (二进制)

**帧结构**:

```
+--------+--------+----------+----------------+-------------+---------------+------------+
| Magic  | MsgType | Reserve | Metadata Length|  Metadata   | Payload Len   | Audio Data |
| 2 bytes| 1 byte | 1 byte  |    4 bytes     |  N bytes    |   4 bytes     |  M bytes   |
+--------+--------+----------+----------------+-------------+---------------+------------+
     ^                                                ^                ^
     |                                                |                |
   0xAA55                                    JSON Metadata      PCM Audio
```

| 字段 | 大小 | 字节序 | 说明 |
|------|------|--------|------|
| Magic | 2 bytes | Big-Endian | 固定值 `0xAA55` |
| MsgType | 1 byte | - | 消息类型 |
| Reserved | 1 byte | - | 保留 |
| Metadata Length | 4 bytes | Big-Endian | 元数据长度 |
| Metadata | N bytes | UTF-8 | JSON 格式元数据 |
| Payload Length | 4 bytes | Big-Endian | 音频数据长度 |
| Audio Data | M bytes | - | PCM 音频数据 |

#### Metadata JSON 结构

```json
{
    "request_id": "请求ID",
    "is_final": false,
    "sample_rate": 16000
}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `request_id` | string | 对应请求的 ID |
| `is_final` | boolean | 是否为最后一帧 |
| `sample_rate` | int | 采样率 (通常 16000) |

#### 音频数据格式

| 参数 | 值 |
|------|-----|
| 格式 | PCM |
| 采样率 | 由 metadata.sample_rate 指定 (默认 16000 Hz) |
| 位深 | 16-bit signed integer (Little-Endian) |
| 声道 | 单声道 |

#### 状态消息 (JSON)

**合成完成**:
```json
{
    "type": "complete"
}
```

**进度更新**:
```json
{
    "type": "progress",
    "state": "processing",
    "message": "状态描述"
}
```

**错误**:
```json
{
    "type": "error",
    "error": {
        "message": "错误描述"
    }
}
```

---

## 4. 嵌入式端实现流程

### 初始化流程

```
1. 连接 ASR WebSocket  (ws://host:9200)
2. 连接 LLM WebSocket  (ws://host:9400)
3. 连接 TTS WebSocket  (ws://host:9300/tts)
4. 等待 LLM 返回 status.connected 获取 session_id
```

### 语音交互流程

```
┌─────────────┐    PCM音频     ┌─────────┐
│   麦克风    │ ─────────────> │   ASR   │
└─────────────┘                └────┬────┘
                                    │ JSON: {text, is_final}
                                    ▼
┌─────────────┐              ┌─────────────┐
│   扬声器    │ <─────────── │    TTS      │
└─────────────┘   PCM音频    └──────┬──────┘
                                 ▲  │
                                 │  │ JSON: {type:"tts_request", ...}
                                 │  ▼
                           ┌─────────────┐
                           │     LLM     │
                           └─────────────┘
                                 ▲
                                 │ JSON: {type:"text_input", ...}
                                 │
```

### 完整交互时序

```
┌────────┐     ┌─────┐     ┌─────┐     ┌─────┐
│ Device │     │ ASR │     │ LLM │     │ TTS │
└───┬────┘     └──┬──┘     └──┬──┘     └──┬──┘
    │             │           │           │
    │── Connect ──>│           │           │
    │── Connect ──────────────>│           │
    │── Connect ──────────────────────────>│
    │             │           │           │
    │  [用户说话] │           │           │
    │── PCM Audio ─>│           │           │
    │── PCM Audio ─>│           │           │
    │             │           │           │
    │<── {text, is_final:true} │           │
    │             │           │           │
    │── {type:"text_input", text} ────────>│
    │             │           │           │
    │<── {type:"llm_response", content} ───│
    │             │           │           │
    │── {type:"tts_request", text} ────────>│
    │             │           │           │
    │<── Audio Frame ──────────────────────│
    │<── Audio Frame ──────────────────────│
    │<── Audio Frame (is_final:true) ──────│
    │             │           │           │
    │  [播放音频] │           │           │
```

---

## 5. C 语言解析示例

### TTS 帧解析

```c
#include <stdint.h>
#include <string.h>

typedef struct {
    char request_id[64];
    int is_final;
    int sample_rate;
} tts_metadata_t;

typedef struct __attribute__((packed)) {
    uint16_t magic;
    uint8_t msg_type;
    uint8_t reserved;
    uint32_t metadata_len;
} tts_frame_header_t;

// Big-Endian to Host conversion
static inline uint16_t be16toh(uint16_t val) {
    return (val >> 8) | (val << 8);
}

static inline uint32_t be32toh(uint32_t val) {
    return ((val >> 24) & 0xff) |
           ((val >> 8) & 0xff00) |
           ((val << 8) & 0xff0000) |
           ((val << 24) & 0xff000000);
}

int parse_tts_frame(uint8_t* data, size_t len,
                    tts_metadata_t* meta,
                    int16_t** audio_out,
                    size_t* audio_len) {
    if (len < sizeof(tts_frame_header_t)) {
        return -1;  // Frame too short
    }
    
    tts_frame_header_t* header = (tts_frame_header_t*)data;
    
    // Check magic number (Big-Endian)
    if (be16toh(header->magic) != 0xAA55) {
        return -2;  // Invalid magic
    }
    
    uint32_t meta_len = be32toh(header->metadata_len);
    
    if (len < sizeof(tts_frame_header_t) + meta_len + 4) {
        return -3;  // Incomplete frame
    }
    
    // Parse metadata JSON (simplified - use a JSON library in production)
    char* meta_json = (char*)(data + sizeof(tts_frame_header_t));
    // parse_json(meta_json, meta_len, meta);
    
    // Get audio payload
    uint32_t payload_len = be32toh(*(uint32_t*)(data + sizeof(tts_frame_header_t) + meta_len));
    
    if (payload_len > 0) {
        *audio_out = (int16_t*)(data + sizeof(tts_frame_header_t) + meta_len + 4);
        *audio_len = payload_len / 2;  // 16-bit samples
    } else {
        *audio_out = NULL;
        *audio_len = 0;
    }
    
    return 0;
}
```

### PCM 音频录制与发送

```c
#define SAMPLE_RATE 16000
#define FRAME_SIZE  4096

void send_audio_to_asr(websocket_t* ws, int16_t* samples, size_t count) {
    // Send raw PCM data directly
    websocket_send_binary(ws, (uint8_t*)samples, count * sizeof(int16_t));
}

void on_asr_message(const char* data, size_t len) {
    // Parse JSON response
    // {"text": "...", "is_final": true/false}
    
    cJSON* json = cJSON_ParseWithLength(data, len);
    const char* text = cJSON_GetStringValue(cJSON_GetObjectItem(json, "text"));
    int is_final = cJSON_IsTrue(cJSON_GetObjectItem(json, "is_final"));
    
    if (is_final && text && strlen(text) > 0) {
        // Send to LLM
        send_text_to_llm(text);
    }
    
    cJSON_Delete(json);
}
```

---

## 6. 错误处理

| 场景 | 处理方式 |
|------|----------|
| WebSocket 断开 | 自动重连,重连间隔建议 1-5s |
| ASR 无响应 | 检查音频格式是否正确 (16kHz, 16bit, mono) |
| TTS request_id 不匹配 | 停止当前播放,处理新请求 |
| 音频播放中断 | 清空缓冲区,重置播放状态 |
| LLM 超时 | 设置合理超时 (建议 30-60s),超时后可重发 |

---

## 7. WebSocket 消息类型汇总

### LLM Gateway 消息类型

| 方向 | type | 说明 |
|------|------|------|
| 发送 | `text_input` | 发送文本给 LLM |
| 发送 | `ping` | 心跳请求 |
| 接收 | `status` | 连接状态 (含 session_id) |
| 接收 | `llm_response` | LLM 文本回复 |
| 接收 | `tool_call` | 工具调用及结果 |
| 接收 | `error` | 错误消息 |
| 接收 | `pong` | 心跳响应 |

### TTS 消息类型

| 方向 | type | 说明 |
|------|------|------|
| 发送 | `tts_request` | 请求语音合成 |
| 接收 | 二进制帧 | 音频数据 (含 metadata) |
| 接收 | `complete` | 合成完成 |
| 接收 | `progress` | 合成进度 |
| 接收 | `error` | 错误消息 |

---

## 8. 附录

### 默认端口配置

| 服务 | 端口 | 路径 |
|------|------|------|
| ASR | 9200 | `/` |
| LLM Gateway | 9400 | `/` |
| TTS | 9300 | `/tts` |

### 音频参数

| 参数 | ASR 输入 | TTS 输出 |
|------|----------|----------|
| 采样率 | 16000 Hz | 16000 Hz (可配置) |
| 位深 | 16-bit | 16-bit |
| 声道 | Mono | Mono |
| 格式 | PCM (Little-Endian) | PCM (Little-Endian) |
