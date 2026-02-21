# LLM Gateway WebSocket API 文档

## 1. 概述

LLM Gateway 是 ListenAI 语音助手系统的大语言模型网关服务，负责处理用户文本输入、调用 AI 模型、管理对话会话，并支持工具调用（MCP）功能。

### 1.1 服务信息

| 参数 | 值 |
|------|-----|
| 协议 | WebSocket |
| 地址 | `ws://{host}:9400` |
| 数据格式 | JSON |
| 异步框架 | asyncio + websockets |

### 1.2 架构定位

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│   ASR       │────>│ LLM Gateway │────>│    LLM      │
│  (语音识别)  │     │   (本服务)   │     │   (模型)    │
└─────────────┘     └──────┬──────┘     └─────────────┘
                           │
                   ┌───────┴──────────────────────┐
                   │                              │
              ┌────▼────┐                  ┌─────▼──────┐
              │   TTS   │                  │     工具系统   │
              │(语音合成) │                  │  ┌───────┐  │
              └─────────┘                  │  │ 服务端 │  │
                                           │  │  MCP  │  │
                   ┌───────────────────────┤  └───────┘  │
                   │   客户端              │  ┌───────┐  │
                   │  (嵌入式/Web)         │  │ 客户端 │  │
                   │                       │  │  工具  │  │
                   │  - 设备控制           │  └───────┘  │
                   │  - 传感器数据         │     ▲       │
                   │  - 本地功能           │     │       │
                   │                       │     │ 回调  │
                   └───────────────────────┴─────┴───────┘
```

#### 工具调用架构

LLM Gateway 支持两种类型的工具调用：

| 类型 | 位置 | 说明 | 示例 |
|------|------|------|------|
| **服务端工具** | LLM Gateway | 服务端内置或 MCP 提供的工具 | 获取天气、播放音乐 |
| **客户端工具** | 客户端环境 | 客户端注册的自定义工具 | 获取电量、控制设备、读取传感器 |

---

## 2. 连接

### 2.1 连接 URL

```
ws://<host>:9400
```

### 2.2 连接流程

```
Client                              Server
  │                                   │
  ├───── WebSocket Connect ──────────>│
  │                                   │
  │                             创建会话
  │                             分配 LLMClient
  │                                   │
  │<──── status:connected ─────────────┤
  │     {session_id: "..."}            │
  │                                   │
```

### 2.3 连接状态

连接建立后，服务端会立即发送连接状态消息：

```json
{
    "type": "status",
    "status": "connected",
    "data": {
        "session_id": "生成的会话ID"
    },
    "timestamp": "2025-02-21T10:30:00.000Z"
}
```

---

## 3. 消息类型

### 3.1 客户端消息类型

| type | 说明 | 必需参数 |
|------|------|----------|
| `text_input` | 文本输入请求 | `text` |
| `configure` | 配置参数 | 配置项 |
| `start_session` | 开始/恢复会话 | `session_id`(可选) |
| `end_session` | 结束会话 | 无 |
| `register_tools` | 注册客户端工具 | `tools` |
| `tool_result` | 返回工具执行结果 | `call_id`, `result` |
| `ping` | 心跳检测 | 无 |

### 3.2 服务端消息类型

| type | 说明 | 触发条件 |
|------|------|----------|
| `status` | 状态通知 | 连接、会话状态变化 |
| `llm_response` | LLM 文本响应 | 处理文本输入 |
| `tool_call` | 服务端工具调用通知 | 服务端工具执行完成 |
| `tool_callback` | 回调客户端工具 | LLM 请求客户端工具时 |
| `tools_registered` | 工具注册确认 | 客户端注册工具成功 |
| `error` | 错误消息 | 任何错误发生 |
| `pong` | 心跳响应 | 收到 ping |

---

## 4. 客户端请求

### 4.1 文本输入请求

最常用的请求类型，发送用户文本给 LLM 处理。

```json
{
    "type": "text_input",
    "text": "今天天气怎么样？",
    "session_id": "可选-会话ID，用于保持上下文",
    "timestamp": "2025-02-21T10:30:00.000Z"
}
```

| 字段 | 类型 | 必需 | 说明 |
|------|------|------|------|
| `type` | string | 是 | 固定值 `"text_input"` |
| `text` | string | 是 | 用户输入的文本内容 |
| `session_id` | string | 否 | 会话ID，用于恢复上下文 |
| `timestamp` | string | 否 | ISO 8601 格式时间戳 |

### 4.2 配置请求

动态调整 LLM 参数。

```json
{
    "type": "configure",
    "temperature": 0.7,
    "max_tokens": 2048,
    "enable_context": true
}
```

| 字段 | 类型 | 必需 | 默认值 | 说明 |
|------|------|------|--------|------|
| `type` | string | 是 | - | 固定值 `"configure"` |
| `temperature` | float | 否 | 0.7 | 温度参数 (0.0-1.0) |
| `max_tokens` | int | 否 | 2048 | 最大输出 tokens |
| `enable_context` | bool | 否 | false | 是否启用上下文历史 |

### 4.3 开始会话

创建新会话或恢复已有会话。

```json
{
    "type": "start_session",
    "session_id": "已有会话ID（可选）"
}
```

| 字段 | 类型 | 必需 | 说明 |
|------|------|------|------|
| `type` | string | 是 | 固定值 `"start_session"` |
| `session_id` | string | 否 | 指定会话ID进行恢复 |

### 4.4 结束会话

```json
{
    "type": "end_session"
}
```

### 4.5 心跳请求

用于保持连接活跃和检测连接状态。

```json
{
    "type": "ping"
}
```

### 4.6 注册客户端工具

客户端将自己的工具能力注册到服务器，使 LLM 可以调用这些客户端工具。

```json
{
    "type": "register_tools",
    "tools": [
        {
            "name": "get_device_info",
            "description": "获取设备信息，包括型号、系统版本、电量等",
            "parameters": {
                "type": "object",
                "properties": {
                    "info_type": {
                        "type": "string",
                        "enum": ["all", "battery", "system", "hardware"],
                        "description": "要获取的信息类型"
                    }
                },
                "required": []
            }
        },
        {
            "name": "control_device",
            "description": "控制设备功能，如开关灯、调节温度等",
            "parameters": {
                "type": "object",
                "properties": {
                    "action": {
                        "type": "string",
                        "enum": ["turn_on", "turn_off", "set_value"],
                        "description": "控制动作"
                    },
                    "device": {
                        "type": "string",
                        "description": "设备名称，如 light、ac、tv"
                    },
                    "value": {
                        "type": "number",
                        "description": "设置值（当 action 为 set_value 时使用）"
                    }
                },
                "required": ["action", "device"]
            }
        }
    ]
}
```

| 字段 | 类型 | 必需 | 说明 |
|------|------|------|------|
| `type` | string | 是 | 固定值 `"register_tools"` |
| `tools` | array | 是 | 工具定义列表 |
| `tools[].name` | string | 是 | 工具名称（唯一标识） |
| `tools[].description` | string | 是 | 工具描述，LLM 会参考此描述 |
| `tools[].parameters` | object | 是 | JSON Schema 格式的参数定义 |

#### 工具参数格式 (JSON Schema)

客户端工具参数遵循 [JSON Schema](https://json-schema.org/) 规范：

```json
{
    "type": "object",
    "properties": {
        "param_name": {
            "type": "string|number|boolean|array|object",
            "description": "参数说明",
            "enum": ["可选值1", "可选值2"],
            "default": "默认值"
        }
    },
    "required": ["必需参数1", "必需参数2"]
}
```

### 4.7 返回工具执行结果

当客户端收到服务端的 `tool_callback` 请求后，执行工具并返回结果。

```json
{
    "type": "tool_result",
    "call_id": "uuid-string",
    "result": {
        "model": "SmartSpeaker X1",
        "battery": 85,
        "system_version": "2.1.0"
    },
    "success": true,
    "error": null
}
```

| 字段 | 类型 | 必需 | 说明 |
|------|------|------|------|
| `type` | string | 是 | 固定值 `"tool_result"` |
| `call_id` | string | 是 | 对应 tool_callback 中的调用ID |
| `result` | any | 否 | 工具执行结果（成功时） |
| `success` | boolean | 是 | 工具执行是否成功 |
| `error` | string | 否 | 错误信息（失败时） |

#### 执行失败示例

```json
{
    "type": "tool_result",
    "call_id": "uuid-string",
    "result": null,
    "success": false,
    "error": "设备连接超时"
}
```

---

## 5. 服务端响应

### 5.1 状态消息

#### 连接成功

```json
{
    "type": "status",
    "status": "connected",
    "data": {
        "session_id": "abc123-def456-ghi789"
    },
    "timestamp": "2025-02-21T10:30:00.000Z"
}
```

#### 处理中状态

```json
{
    "type": "status",
    "status": "processing",
    "data": {
        "message": "正在处理您的请求..."
    },
    "timestamp": "2025-02-21T10:30:01.000Z"
}
```

#### 等待工具结果状态

当 LLM 请求客户端工具时，服务端会发送此状态表示正在等待客户端返回工具执行结果：

```json
{
    "type": "status",
    "status": "waiting_for_tools",
    "data": {
        "pending_tools": 2
    },
    "timestamp": "2025-02-21T10:30:01.000Z"
}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `pending_tools` | int | 等待中的客户端工具数量 |

客户端收到此状态后应：
1. 准备接收后续的 `tool_callback` 消息
2. 执行对应的工具函数
3. 通过 `tool_result` 消息返回结果
4. 服务端收到所有工具结果后会继续生成最终 LLM 响应

| 状态值 | 说明 |
|--------|------|
| `connected` | 连接已建立 |
| `processing` | 正在处理请求 |
| `waiting_for_tools` | 等待客户端工具返回结果 |
| `idle` | 空闲状态 |

### 5.2 LLM 响应消息

AI 模型返回的文本响应。

```json
{
    "type": "llm_response",
    "content": "今天天气晴朗，温度约22度，适合外出活动。",
    "tool_calls": [],
    "is_final": true,
    "timestamp": "2025-02-21T10:30:02.500Z"
}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `type` | string | 固定值 `"llm_response"` |
| `content` | string | AI 回复的文本内容（已清理） |
| `tool_calls` | array | 关联的工具调用列表 |
| `is_final` | boolean | 是否为最终响应 |
| `timestamp` | string | ISO 8601 格式时间戳 |

#### 响应内容清理

服务端会对 LLM 原始响应进行清理，使其更适合语音播报：

- **移除表情符号**: 清除所有 emoji 字符
- **移除装饰符号**: 清理 ★☆◆◇●■□ 等符号
- **清理 Markdown**: 移除 **加粗**、*斜体* 等格式标记
- **口语化优化**: 转换为自然口语表达

### 5.3 工具调用消息 (服务端工具)

当 LLM 调用服务端内置工具时发送此通知。

```json
{
    "type": "tool_call",
    "tool_name": "get_weather",
    "arguments": {
        "city": "北京"
    },
    "result": {
        "temperature": 22,
        "condition": "Sunny",
        "humidity": 45
    },
    "success": true,
    "duration_ms": 125.5,
    "timestamp": "2025-02-21T10:30:01.500Z"
}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `type` | string | 固定值 `"tool_call"` |
| `tool_name` | string | 被调用的工具名称 |
| `arguments` | object | 传递给工具的参数 |
| `result` | object | 工具执行结果 |
| `success` | boolean | 工具执行是否成功 |
| `duration_ms` | float | 执行耗时（毫秒） |
| `timestamp` | string | ISO 8601 格式时间戳 |

### 5.4 工具回调消息 (客户端工具)

当 LLM 需要调用客户端注册的工具时，服务端发送此回调请求。

```json
{
    "type": "tool_callback",
    "call_id": "uuid-string",
    "tool_name": "get_device_info",
    "arguments": {
        "info_type": "battery"
    },
    "timestamp": "2025-02-21T10:30:01.500Z"
}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `type` | string | 固定值 `"tool_callback"` |
| `call_id` | string | 唯一调用ID，客户端需在返回结果时带上此ID |
| `tool_name` | string | 要调用的客户端工具名称 |
| `arguments` | object | 传递给工具的参数 |
| `timestamp` | string | ISO 8601 格式时间戳 |

客户端收到此消息后应：
1. 根据 `tool_name` 找到对应的处理函数
2. 使用 `arguments` 执行工具
3. 发送 `tool_result` 消息返回结果

### 5.5 工具注册确认消息

客户端注册工具成功后，服务端返回确认消息。

```json
{
    "type": "tools_registered",
    "count": 2,
    "tools": [
        {
            "name": "get_device_info",
            "status": "registered"
        },
        {
            "name": "control_device",
            "status": "registered"
        }
    ],
    "timestamp": "2025-02-21T10:30:00.000Z"
}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `type` | string | 固定值 `"tools_registered"` |
| `count` | int | 成功注册的工具数量 |
| `tools` | array | 注册的工具列表 |
| `tools[].name` | string | 工具名称 |
| `tools[].status` | string | 注册状态 (`registered` / `failed`) |
| `timestamp` | string | ISO 8601 格式时间戳 |

#### 注册失败示例

```json
{
    "type": "tools_registered",
    "count": 1,
    "tools": [
        {
            "name": "get_device_info",
            "status": "registered"
        },
        {
            "name": "control_device",
            "status": "failed",
            "error": "Tool name already exists"
        }
    ],
    "timestamp": "2025-02-21T10:30:00.000Z"
}
```

### 5.4 错误消息

```json
{
    "type": "error",
    "code": "LLM_ERROR",
    "message": "Failed to process request",
    "details": "Connection timeout",
    "timestamp": "2025-02-21T10:30:00.000Z"
}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `type` | string | 固定值 `"error"` |
| `code` | string | 错误代码（见下表） |
| `message` | string | 错误描述 |
| `details` | string | 详细错误信息 |
| `timestamp` | string | ISO 8601 格式时间戳 |

#### 错误代码定义

| 错误代码 | 说明 | HTTP 等效 |
|----------|------|----------|
| `INVALID_MESSAGE` | 消息格式无效 | 400 |
| `UNKNOWN_MESSAGE_TYPE` | 未知的消息类型 | 400 |
| `LLM_ERROR` | LLM 处理错误 | 500 |
| `SESSION_ERROR` | 会话错误 | 500 |
| `TIMEOUT` | 请求超时 | 504 |
| `INTERNAL_ERROR` | 内部错误 | 500 |
| `TOOL_NOT_FOUND` | 请求的工具不存在 | 404 |
| `TOOL_EXECUTION_FAILED` | 工具执行失败 | 500 |
| `INVALID_TOOL_PARAMETERS` | 工具参数无效 | 400 |
| `TOOL_RESULT_TIMEOUT` | 等待工具结果超时 | 504 |
| `TOOL_REGISTRATION_FAILED` | 工具注册失败 | 400 |

### 5.6 心跳响应

```json
{
    "type": "pong",
    "timestamp": "2025-02-21T10:30:00.000Z"
}
```

---

## 6. 会话管理

### 6.1 会话生命周期

```
┌─────────┐     ┌─────────┐     ┌─────────┐     ┌─────────┐
│  创建   │────>│  激活   │────>│  空闲   │────>│  清理   │
└─────────┘     └─────────┘     └─────────┘     └─────────┘
                      │               │
                      ▼               │
                 ┌─────────┐          │
                 │  恢复   │──────────┘
                 └─────────┘
```

### 6.2 会话特性

| 特性 | 值 |
|------|-----|
| 会话超时 | 3600 秒（1小时）|
| 历史消息 | 最多 10 条 |
| 会话恢复 | 支持（通过 session_id） |
| 自动清理 | 每 60 秒 |

### 6.3 上下文控制

通过 `enable_context` 配置控制是否使用历史消息：

**启用上下文** (`enable_context: true`):
```python
messages = [
    {"role": "user", "content": "我叫什么名字？"},
    {"role": "assistant", "content": "你是小明"},
    {"role": "user", "content": "我刚才问什么？"}  # 能回答
]
```

**禁用上下文** (`enable_context: false`, 默认):
```python
messages = [
    {"role": "user", "content": "我刚才问什么？"}  # 无法回答
]
```

---

## 7. 工具调用 (MCP)

### 7.1 工具类型

系统支持两种类型的工具：

| 类型 | 说明 | 执行位置 |
|------|------|----------|
| **服务端工具** | 由服务端提供的内置工具 | LLM Gateway / MCP |
| **客户端工具** | 由客户端注册的自定义工具 | 客户端环境 |

### 7.2 服务端内置工具

| 工具名称 | 说明 | 参数 |
|----------|------|------|
| `get_weather` | 获取天气信息 | `city` (城市名称) |
| `set_volume` | 设置音量 | `volume` (0-100) |
| `play_music` | 播放音乐 | `query` (歌曲名/歌手), `playlist` (可选) |

### 7.3 客户端工具注册

客户端可以在连接建立后注册自己的工具能力。这些工具将被添加到 LLM 的可用工具列表中。

#### 注册时机

```
Client                              Server
  │                                   │
  ├───── WebSocket Connect ──────────>│
  │                                   │
  │<──── status:connected ─────────────┤
  │     {session_id: "..."}            │
  │                                   │
  ├───── register_tools ──────────────>│
  │     {tools: [...]}                 │
  │                                   │
  │<──── tools_registered ─────────────┤
  │     {count: 2}                     │
  │                                   │
```

#### 工具存储

- 服务端为每个连接维护独立的客户端工具列表
- 工具在连接断开后自动清理
- 同一连接内工具名称不能重复

### 7.4 服务端工具调用流程

```
Client                    LLM Gateway                 MCP/Tool
  │                          │                          │
  ├─ text_input ────────────>│                          │
  │    "今天天气怎么样？"    │                          │
  │                          │                          │
  │                          ├─ LLM 检测需要工具 ──────>│
  │                          │   get_weather(city)      │
  │                          │                          │
  │                          │<── 返回天气结果 ──────────┤
  │                          │                          │
  │<─ tool_call ─────────────┤                          │
  │    {tool_name, result}   │                          │
  │                          │                          │
  │                          ├─ LLM 生成最终回复 ───────>│
  │                          │                          │
  │<─ llm_response ──────────┤                          │
  │    "今天北京天气晴朗..." │                          │
```

### 7.5 客户端工具调用流程

```
Client                    LLM Gateway                    LLM
  │                          │                          │
  ├─ text_input ────────────>│                          │
  │    "我的电量还剩多少？"  │                          │
  │                          │                          │
  │                          ├─ text_input ─────────────>│
  │                          │ {tools: [服务端工具,       │
  │                          │  客户端工具...]}           │
  │                          │                          │
  │                          │<─ tool_calls ────────────┤
  │                          │ {name: "get_device_info", │
  │                          │  args: {info_type: "battery"}}
  │                          │                          │
  │<─ tool_callback ─────────┤                          │
  │    {call_id, tool_name,  │                          │
  │     arguments}           │                          │
  │                          │                          │
  │  [执行本地工具]           │                          │
  │                          │                          │
  ├─ tool_result ───────────>│                          │
  │    {call_id, result:     │                          │
  │     {battery: 85}}       │                          │
  │                          │                          │
  │                          ├─ tool_result ───────────>│
  │                          │                          │
  │                          │<─ llm_response ──────────┤
  │                          │                          │
  │<─ llm_response ──────────┤                          │
  │    "您的设备电量还剩85%" │                          │
```

### 7.6 混合工具调用流程

当 LLM 需要同时调用服务端工具和客户端工具时：

```
Client                    LLM Gateway                    LLM
  │                          │                          │
  ├─ text_input ────────────>│                          │
  │    "播放音乐并调大音量"  │                          │
  │                          │                          │
  │                          ├─ text_input ─────────────>│
  │                          │                          │
  │                          │<─ tool_calls ────────────┤
  │                          │ 1. play_music (服务端)   │
  │                          │ 2. set_volume (客户端)    │
  │                          │                          │
  │                          │ [执行 play_music]         │
  │<─ tool_call ─────────────┤                          │
  │    play_music 完成       │                          │
  │                          │                          │
  │<─ tool_callback ─────────┤                          │
  │    set_volume 请求       │                          │
  │                          │                          │
  ├─ tool_result ───────────>│                          │
  │    set_volume 结果       │                          │
  │                          │                          │
  │                          ├─ 所有工具结果 ──────────>│
  │                          │                          │
  │                          │<─ 最终回复 ──────────────┤
  │                          │                          │
  │<─ llm_response ──────────┤                          │
  │    "已为您播放音乐..."   │                          │
```

### 7.7 工具超时处理

| 超时类型 | 默认时间 | 处理方式 |
|----------|----------|----------|
| 客户端工具执行 | 30 秒 | 返回 `TOOL_RESULT_TIMEOUT` 错误 |
| 服务端工具执行 | 10 秒 | 返回 `TOOL_EXECUTION_FAILED` 错误 |

---

## 8. 配置参数

### 8.1 服务器配置

| 参数 | 环境变量 | 默认值 | 说明 |
|------|----------|--------|------|
| `host` | `CLOUD_HOST` | `0.0.0.0` | 监听地址 |
| `port` | `CLOUD_PORT` | `9400` | WebSocket 端口 |
| `ping_interval` | `CLOUD_PING_INTERVAL` | `30` | 心跳间隔（秒） |
| `ping_timeout` | `CLOUD_PING_TIMEOUT` | `300` | 心跳超时（秒） |
| `max_connections` | `CLOUD_MAX_CONNECTIONS` | `100` | 最大连接数 |
| `session_timeout` | `CLOUD_SESSION_TIMEOUT` | `3600` | 会话超时（秒） |
| `log_level` | `CLOUD_LOG_LEVEL` | `INFO` | 日志级别 |
| `log_format` | `CLOUD_LOG_FORMAT` | `json` | 日志格式 |

### 8.2 LLM 配置

| 参数 | 环境变量 | 默认值 | 说明 |
|------|----------|--------|------|
| `base_url` | `LLM_BASE_URL` | `http://192.168.13.228:8000/v1/` | LLM API 地址 |
| `model` | `LLM_MODEL` | `Qwen3-30B-A3B` | 模型名称 |
| `api_key` | `LLM_API_KEY` | `null` | API 密钥 |
| `timeout` | `LLM_TIMEOUT` | `120` | 请求超时（秒） |
| `temperature` | `LLM_TEMPERATURE` | `0.7` | 温度参数 |
| `max_tokens` | `LLM_MAX_TOKENS` | `2048` | 最大输出 tokens |
| `enable_context` | `LLM_ENABLE_CONTEXT` | `false` | 是否启用上下文 |

### 8.3 MCP 配置

| 参数 | 环境变量 | 默认值 | 说明 |
|------|----------|--------|------|
| `enabled` | `MCP_ENABLED` | `true` | 是否启用 MCP |
| `server_name` | `MCP_SERVER_NAME` | `arcs-mini-mcp-server` | MCP 服务器名称 |
| `protocol_version` | `MCP_PROTOCOL_VERSION` | `2024-11-05` | 协议版本 |

### 8.4 客户端工具配置

| 参数 | 环境变量 | 默认值 | 说明 |
|------|----------|--------|------|
| `client_tools_enabled` | `CLIENT_TOOLS_ENABLED` | `true` | 是否启用客户端工具 |
| `client_tools_max_count` | `CLIENT_TOOLS_MAX_COUNT` | `32` | 每个连接最大工具数 |
| `client_tool_timeout` | `CLIENT_TOOL_TIMEOUT` | `30` | 客户端工具执行超时（秒） |
| `client_tool_result_queue_size` | `CLIENT_TOOL_RESULT_QUEUE_SIZE` | `10` | 工具结果队列大小 |

---

## 9. 消息流程示例

### 9.1 基础对话流程

```
Client                              Server
  │                                   │
  ├───── WebSocket Connect ──────────>│
  │                                   │
  │<──── status:connected ─────────────┤
  │     {session_id: "abc123"}         │
  │                                   │
  ├───── text_input ─────────────────>│
  │     {"text": "你好"}               │
  │                                   │
  │<──── llm_response ─────────────────┤
  │     {"content": "你好！有什么我可以帮助你的吗？"}
  │                                   │
```

### 9.2 服务端工具调用流程

```
Client                              Server
  │                                   │
  ├───── text_input ─────────────────>│
  │     {"text": "今天北京天气怎么样？"}
  │                                   │
  │<──── tool_call ────────────────────┤
  │     {tool_name: "get_weather",
  │      result: {temperature: 22}}
  │                                   │
  │<──── llm_response ─────────────────┤
  │     {"content": "今天北京天气晴朗，温度22度..."}
  │                                   │
```

### 9.3 客户端工具注册流程

```
Client                              Server
  │                                   │
  ├───── WebSocket Connect ──────────>│
  │                                   │
  │<──── status:connected ─────────────┤
  │     {session_id: "abc123"}         │
  │                                   │
  ├───── register_tools ──────────────>│
  │     {"tools": [                   │
  │       {                           │
  │         "name": "get_battery",    │
  │         "description": "获取电量", │
  │         "parameters": {...}       │
  │       }                           │
  │     ]}                            │
  │                                   │
  │<──── tools_registered ─────────────┤
  │     {"count": 1,                  │
  │      "tools": [{"name": "get_battery",
  │                "status": "registered"}]}
  │                                   │
```

### 9.4 客户端工具调用流程

```
Client                              Server                         LLM
  │                                   │                              │
  ├───── text_input ─────────────────>│                              │
  │     {"text": "我的电量还剩多少？"} │                              │
  │                                   │                              │
  │                                   ├───── text_input ─────────────>│
  │                                   │ {tools: [..., get_battery]}  │
  │                                   │                              │
  │                                   │<──── tool_calls ──────────────┤
  │                                   │ {get_battery: {}}             │
  │                                   │                              │
  │<──── tool_callback ───────────────┤                              │
  │     {call_id: "xyz",              │                              │
  │      tool_name: "get_battery",    │                              │
  │      arguments: {}}               │                              │
  │                                   │                              │
  │ [执行本地工具: get_battery()]      │                              │
  │                                   │                              │
  ├───── tool_result ─────────────────>│                              │
  │     {call_id: "xyz",              │                              │
  │      result: {level: 85,          │                              │
  │                charging: false},  │                              │
  │      success: true}               │                              │
  │                                   │                              │
  │                                   ├───── tool_result ────────────>│
  │                                   │                              │
  │                                   │<──── llm_response ────────────┤
  │                                   │                              │
  │<──── llm_response ─────────────────┤                              │
  │     {"content": "您的设备电量还剩85%"}
  │                                   │                              │
```

### 9.5 混合工具调用流程

```
Client                              Server                         LLM
  │                                   │                              │
  ├───── text_input ─────────────────>│                              │
  │     {"text": "播放音乐并设置音量50"}
  │                                   │                              │
  │                                   ├───── text_input ─────────────>│
  │                                   │                              │
  │                                   │<──── tool_calls ──────────────┤
  │                                   │ [play_music, set_volume]      │
  │                                   │                              │
  │                 [服务端执行 play_music]                          │
  │<──── tool_call ───────────────────┤                              │
  │     {tool_name: "play_music",     │                              │
  │      result: {status: "playing"}} │                              │
  │                                   │                              │
  │<──── tool_callback ───────────────┤                              │
  │     {call_id: "abc",              │                              │
  │      tool_name: "set_volume",     │                              │
  │      arguments: {value: 50}}      │                              │
  │                                   │                              │
  │ [执行本地工具: set_volume(50)]     │                              │
  │                                   │                              │
  ├───── tool_result ─────────────────>│                              │
  │     {call_id: "abc",              │                              │
  │      result: {volume: 50},        │                              │
  │      success: true}               │                              │
  │                                   │                              │
  │                                   ├───── 完成所有工具 ───────────>│
  │                                   │                              │
  │                                   │<──── llm_response ────────────┤
  │                                   │                              │
  │<──── llm_response ─────────────────┤                              │
  │     {"content": "已为您播放音乐并将音量设置为50"}
  │                                   │                              │
```

### 9.6 会话恢复流程

```
Client                              Server
  │                                   │
  ├───── start_session ──────────────>│
  │     {session_id: "abc123"}         │
  │                                   │
  │<──── status:connected ─────────────┤
  │     {session_id: "abc123"}         │
  │                                   │
  ├───── text_input ─────────────────>│
  │     {"text": "我刚才叫什么名字？"}  │
  │                                   │
  │<──── llm_response ─────────────────┤
  │     {"content": "你叫小明"}         │
  │                                   │
```

### 9.7 错误处理流程

```
Client                              Server
  │                                   │
  ├───── text_input ─────────────────>│
  │     {"text": ""}  ← 空输入         │
  │                                   │
  │<──── error ────────────────────────┤
  │     {code: "INVALID_MESSAGE",
  │      message: "Text cannot be empty"}
  │                                   │
```

### 9.8 工具执行超时流程

```
Client                              Server
  │                                   │
  ├───── text_input ─────────────────>│
  │     {"text": "打开车门"}           │
  │                                   │
  │<──── tool_callback ────────────────┤
  │     {call_id: "xyz",              │
  │      tool_name: "open_door",      │
  │      arguments: {}}               │
  │                                   │
  │                 [30秒超时，未返回]  │
  │                                   │
  │<──── error ────────────────────────┤
  │     {code: "TOOL_RESULT_TIMEOUT", │
  │      message: "Tool execution timeout"}
  │                                   │
```

---

## 10. 客户端实现示例

### 10.1 Python 客户端（带工具注册）

```python
import asyncio
import websockets
import json
from typing import Optional, Dict, Any, Callable
import uuid

class LLMGatewayClient:
    def __init__(self, url: str = "ws://localhost:9400"):
        self.url = url
        self.ws: Optional[websockets.WebSocketClientProtocol] = None
        self.session_id: Optional[str] = None
        self.pending_tool_calls: Dict[str, Any] = {}
        self.client_tools: Dict[str, Callable] = {}

    async def connect(self) -> None:
        """建立连接并获取 session_id"""
        self.ws = await websockets.connect(self.url)

        # 接收连接状态
        status_msg = json.loads(await self.ws.recv())
        if status_msg["type"] == "status":
            self.session_id = status_msg["data"]["session_id"]
            print(f"Connected. Session ID: {self.session_id}")

    async def register_tools(self, tools: list) -> None:
        """注册客户端工具"""
        request = {
            "type": "register_tools",
            "tools": tools
        }
        await self.ws.send(json.dumps(request))

        # 接收注册确认
        response = json.loads(await self.ws.recv())
        if response["type"] == "tools_registered":
            print(f"Registered {response['count']} tools")
            for tool in response["tools"]:
                if tool["status"] == "registered":
                    print(f"  - {tool['name']}: OK")
                else:
                    print(f"  - {tool['name']}: FAILED - {tool.get('error', 'Unknown error')}")

    def define_tool(self, name: str, description: str, parameters: dict):
        """装饰器：定义客户端工具"""
        def decorator(func):
            self.client_tools[name] = func
            return func
        return decorator

    async def send_text(self, text: str) -> str:
        """发送文本并获取响应"""
        if not self.ws:
            await self.connect()

        request = {
            "type": "text_input",
            "text": text,
            "session_id": self.session_id
        }
        await self.ws.send(json.dumps(request))

        # 接收响应
        while True:
            response = json.loads(await self.ws.recv())

            if response["type"] == "tool_call":
                # 服务端工具调用
                print(f"Server tool called: {response['tool_name']}")
                print(f"Result: {response['result']}")

            elif response["type"] == "tool_callback":
                # 客户端工具回调
                call_id = response["call_id"]
                tool_name = response["tool_name"]
                arguments = response.get("arguments", {})

                print(f"Client tool called: {tool_name} with {arguments}")

                # 执行工具
                try:
                    if tool_name in self.client_tools:
                        result = await self._execute_tool(tool_name, arguments)
                        await self._send_tool_result(call_id, result, success=True)
                    else:
                        await self._send_tool_result(
                            call_id,
                            success=False,
                            error=f"Tool '{tool_name}' not found"
                        )
                except Exception as e:
                    await self._send_tool_result(
                        call_id,
                        success=False,
                        error=str(e)
                    )

            elif response["type"] == "llm_response":
                return response["content"]

            elif response["type"] == "error":
                raise Exception(f"{response['code']}: {response['message']}")

    async def _execute_tool(self, tool_name: str, arguments: dict) -> Any:
        """执行客户端工具"""
        func = self.client_tools[tool_name]
        if asyncio.iscoroutinefunction(func):
            return await func(**arguments)
        else:
            return func(**arguments)

    async def _send_tool_result(self, call_id: str, result: Any = None,
                                success: bool = True, error: str = None) -> None:
        """发送工具执行结果"""
        message = {
            "type": "tool_result",
            "call_id": call_id,
            "success": success
        }
        if success:
            message["result"] = result
        else:
            message["error"] = error

        await self.ws.send(json.dumps(message))

    async def configure(self, **kwargs) -> None:
        """配置参数"""
        request = {"type": "configure", **kwargs}
        await self.ws.send(json.dumps(request))

    async def close(self) -> None:
        """关闭连接"""
        if self.ws:
            await self.ws.close()


# 使用示例
async def main():
    client = LLMGatewayClient()

    # 定义客户端工具
    tools_definition = [
        {
            "name": "get_battery",
            "description": "获取设备当前电量百分比",
            "parameters": {
                "type": "object",
                "properties": {},
                "required": []
            }
        },
        {
            "name": "set_volume",
            "description": "设置设备音量",
            "parameters": {
                "type": "object",
                "properties": {
                    "volume": {
                        "type": "integer",
                        "description": "音量值 0-100",
                        "minimum": 0,
                        "maximum": 100
                    }
                },
                "required": ["volume"]
            }
        }
    ]

    # 实现工具函数
    async def get_battery() -> dict:
        # 实际实现中这里会调用硬件接口
        return {"level": 85, "charging": False}

    async def set_volume(volume: int) -> dict:
        # 实际实现中这里会调用硬件接口
        return {"volume": volume, "status": "set"}

    try:
        await client.connect()
        await client.configure(enable_context=True)

        # 注册工具
        client.client_tools["get_battery"] = get_battery
        client.client_tools["set_volume"] = set_volume
        await client.register_tools(tools_definition)

        # 测试对话
        response = await client.send_text("我的电量还剩多少？")
        print(f"Response: {response}")

        response = await client.send_text("把音量调到50")
        print(f"Response: {response}")

    finally:
        await client.close()

if __name__ == "__main__":
    asyncio.run(main())
```

### 10.2 JavaScript 客户端（带工具注册）

```javascript
class LLMGatewayClient {
    constructor(url = 'ws://localhost:9400') {
        this.url = url;
        this.ws = null;
        this.sessionId = null;
        this.messageHandlers = new Map();
        this.clientTools = new Map();
    }

    connect() {
        return new Promise((resolve, reject) => {
            this.ws = new WebSocket(this.url);

            this.ws.onopen = () => {
                console.log('WebSocket connected');
            };

            this.ws.onmessage = async (event) => {
                const msg = JSON.parse(event.data);

                if (msg.type === 'status' && msg.status === 'connected') {
                    this.sessionId = msg.data.session_id;
                    console.log('Session ID:', this.sessionId);
                    resolve();
                } else if (msg.type === 'llm_response') {
                    const handler = this.messageHandlers.get('llm_response');
                    if (handler) handler(msg);
                } else if (msg.type === 'tool_call') {
                    console.log('Server tool:', msg.tool_name, msg.result);
                } else if (msg.type === 'tool_callback') {
                    await this.handleToolCallback(msg);
                } else if (msg.type === 'tools_registered') {
                    console.log(`Registered ${msg.count} tools`);
                } else if (msg.type === 'error') {
                    const handler = this.messageHandlers.get('error');
                    if (handler) handler(msg);
                }
            };

            this.ws.onerror = (error) => reject(error);
            this.ws.onclose = () => console.log('WebSocket closed');
        });
    }

    async handleToolCallback(msg) {
        const { call_id, tool_name, arguments: args } = msg;
        console.log(`Client tool called: ${tool_name}`, args);

        try {
            if (this.clientTools.has(tool_name)) {
                const toolFunc = this.clientTools.get(tool_name);
                const result = await toolFunc(args);
                this.sendToolResult(call_id, result, true);
            } else {
                this.sendToolResult(call_id, null, false, `Tool '${tool_name}' not found`);
            }
        } catch (error) {
            this.sendToolResult(call_id, null, false, error.message);
        }
    }

    sendToolResult(callId, result, success, error = null) {
        const message = {
            type: 'tool_result',
            call_id: callId,
            success: success
        };
        if (success) {
            message.result = result;
        } else {
            message.error = error;
        }
        this.ws.send(JSON.stringify(message));
    }

    registerTools(tools) {
        return new Promise((resolve, reject) => {
            const request = { type: 'register_tools', tools };

            const handler = (msg) => {
                console.log(`Registered ${msg.count} tools`);
                resolve(msg);
                this.messageHandlers.delete('tools_registered');
            };

            this.messageHandlers.set('tools_registered', handler);
            this.ws.send(JSON.stringify(request));
        });
    }

    registerTool(name, func) {
        this.clientTools.set(name, func);
    }

    sendText(text) {
        return new Promise((resolve, reject) => {
            const request = {
                type: 'text_input',
                text: text,
                session_id: this.sessionId
            };

            this.messageHandlers.set('llm_response', (msg) => {
                resolve(msg.content);
                this.messageHandlers.delete('llm_response');
            });

            this.messageHandlers.set('error', (msg) => {
                reject(new Error(`${msg.code}: ${msg.message}`));
                this.messageHandlers.delete('error');
            });

            this.ws.send(JSON.stringify(request));
        });
    }

    configure(options) {
        const request = { type: 'configure', ...options };
        this.ws.send(JSON.stringify(request));
    }

    close() {
        this.ws.close();
    }
}

// 使用示例
const client = new LLMGatewayClient();

await client.connect();

// 定义工具
const tools = [
    {
        name: 'get_battery',
        description: '获取设备当前电量百分比',
        parameters: {
            type: 'object',
            properties: {},
            required: []
        }
    },
    {
        name: 'set_volume',
        description: '设置设备音量',
        parameters: {
            type: 'object',
            properties: {
                volume: {
                    type: 'integer',
                    description: '音量值 0-100',
                    minimum: 0,
                    maximum: 100
                }
            },
            required: ['volume']
        }
    }
];

// 注册工具函数
client.registerTool('get_battery', async (args) => {
    // 实际实现中这里会调用硬件接口
    return { level: 85, charging: false };
});

client.registerTool('set_volume', async (args) => {
    // 实际实现中这里会调用硬件接口
    return { volume: args.volume, status: 'set' };
});

// 注册工具到服务器
await client.registerTools(tools);

await client.configure({ enable_context: true });

// 测试对话
const response1 = await client.sendText('我的电量还剩多少？');
console.log('Response:', response1);

const response2 = await client.sendText('把音量调到50');
console.log('Response:', response2);

client.close();
```

### 10.3 C 嵌入式客户端（带工具注册）

```c
#include <websockets/websockets.h>
#include <cjson/cJSON.h>
#include <string.h>
#include <pthread.h>

typedef struct {
    websockets_t *ws;
    char session_id[64];
    pthread_mutex_t lock;
} llm_client_t;

// 工具函数指针类型
typedef cJSON* (*tool_func_t)(cJSON* arguments);

typedef struct {
    const char *name;
    const char *description;
    cJSON *parameters;  // JSON Schema
    tool_func_t func;
} client_tool_t;

// 工具注册表
static client_tool_t registered_tools[16];
static int tool_count = 0;

// 工具函数实现：获取电量
static cJSON* tool_get_battery(cJSON *arguments) {
    cJSON *result = cJSON_CreateObject();
    cJSON_AddNumberToObject(result, "level", 85);
    cJSON_AddBoolToObject(result, "charging", 0);
    return result;
}

// 工具函数实现：设置音量
static cJSON* tool_set_volume(cJSON *arguments) {
    cJSON *volume = cJSON_GetObjectItem(arguments, "volume");
    int vol = volume ? volume->valueint : 50;

    // 实际实现中这里会调用硬件接口

    cJSON *result = cJSON_CreateObject();
    cJSON_AddNumberToObject(result, "volume", vol);
    cJSON_AddStringToObject(result, "status", "set");
    return result;
}

// 注册工具
void llm_register_tool(const char *name, const char *description,
                       const char *parameters_json, tool_func_t func) {
    if (tool_count >= 16) return;

    registered_tools[tool_count].name = name;
    registered_tools[tool_count].description = description;
    registered_tools[tool_count].parameters = cJSON_Parse(parameters_json);
    registered_tools[tool_count].func = func;
    tool_count++;
}

// 发送工具注册请求
int llm_send_register_tools(llm_client_t *client) {
    cJSON *request = cJSON_CreateObject();
    cJSON_AddStringToObject(request, "type", "register_tools");

    cJSON *tools_array = cJSON_CreateArray();

    for (int i = 0; i < tool_count; i++) {
        cJSON *tool = cJSON_CreateObject();
        cJSON_AddStringToObject(tool, "name", registered_tools[i].name);
        cJSON_AddStringToObject(tool, "description", registered_tools[i].description);
        cJSON_AddItemToObject(tool, "parameters",
                              cJSON_Duplicate(registered_tools[i].parameters, 1));
        cJSON_AddItemToArray(tools_array, tool);
    }

    cJSON_AddItemToObject(request, "tools", tools_array);

    char *json_str = cJSON_Print(request);
    websockets_send_text(client->ws, json_str);

    free(json_str);
    cJSON_Delete(request);
    return 0;
}

// 处理工具回调
void handle_tool_callback(llm_client_t *client, cJSON *msg) {
    const char *call_id = cJSON_GetObjectItem(msg, "call_id")->valuestring;
    const char *tool_name = cJSON_GetObjectItem(msg, "tool_name")->valuestring;
    cJSON *arguments = cJSON_GetObjectItem(msg, "arguments");

    printf("Tool callback: %s\n", tool_name);

    cJSON *result = NULL;
    int success = 1;
    char *error = NULL;

    // 查找并执行工具
    for (int i = 0; i < tool_count; i++) {
        if (strcmp(registered_tools[i].name, tool_name) == 0) {
            result = registered_tools[i].func(arguments);
            break;
        }
    }

    if (result == NULL) {
        success = 0;
        error = "Tool not found";
    }

    // 发送工具结果
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "type", "tool_result");
    cJSON_AddStringToObject(response, "call_id", call_id);
    cJSON_AddBoolToObject(response, "success", success);

    if (success) {
        cJSON_AddItemToObject(response, "result", result);
    } else {
        cJSON_AddStringToObject(response, "error", error);
    }

    char *json_str = cJSON_Print(response);
    websockets_send_text(client->ws, json_str);

    free(json_str);
    cJSON_Delete(response);
}

// 发送文本输入
int llm_send_text(llm_client_t *client, const char *text) {
    cJSON *request = cJSON_CreateObject();
    cJSON_AddStringToObject(request, "type", "text_input");
    cJSON_AddStringToObject(request, "text", text);
    cJSON_AddStringToObject(request, "session_id", client->session_id);

    char *json_str = cJSON_Print(request);
    websockets_send_text(client->ws, json_str);

    free(json_str);
    cJSON_Delete(request);
    return 0;
}

// 解析响应
void on_message(llm_client_t *client, const char *data, size_t len) {
    cJSON *msg = cJSON_ParseWithLength(data, len);
    const char *type = cJSON_GetObjectItem(msg, "type")->valuestring;

    if (strcmp(type, "status") == 0) {
        const char *status = cJSON_GetObjectItem(msg, "status")->valuestring;
        if (strcmp(status, "connected") == 0) {
            cJSON *data_obj = cJSON_GetObjectItem(msg, "data");
            const char *sid = cJSON_GetObjectItem(data_obj, "session_id")->valuestring;
            strncpy(client->session_id, sid, sizeof(client->session_id) - 1);
            printf("Connected. Session ID: %s\n", client->session_id);

            // 连接成功后注册工具
            llm_send_register_tools(client);
        }
    } else if (strcmp(type, "tools_registered") == 0) {
        int count = cJSON_GetObjectItem(msg, "count")->valueint;
        printf("Registered %d tools\n", count);
    } else if (strcmp(type, "llm_response") == 0) {
        const char *content = cJSON_GetObjectItem(msg, "content")->valuestring;
        printf("LLM: %s\n", content);
    } else if (strcmp(type, "tool_call") == 0) {
        const char *tool_name = cJSON_GetObjectItem(msg, "tool_name")->valuestring;
        printf("Server tool: %s\n", tool_name);
    } else if (strcmp(type, "tool_callback") == 0) {
        handle_tool_callback(client, msg);
    } else if (strcmp(type, "error") == 0) {
        const char *code = cJSON_GetObjectItem(msg, "code")->valuestring;
        const char *message = cJSON_GetObjectItem(msg, "message")->valuestring;
        printf("Error [%s]: %s\n", code, message);
    }

    cJSON_Delete(msg);
}

// 使用示例
int main() {
    llm_client_t client = {0};
    pthread_mutex_init(&client.lock, NULL);

    // 连接 WebSocket
    client.ws = websockets_connect("ws://localhost:9400");

    // 注册工具
    llm_register_tool("get_battery", "获取设备电量",
                       "{\"type\":\"object\",\"properties\":{},\"required\":[]}",
                       tool_get_battery);

    llm_register_tool("set_volume", "设置音量",
                       "{\"type\":\"object\",\"properties\":{\"volume\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":100}},\"required\":[\"volume\"]}",
                       tool_set_volume);

    // 消息循环
    while (1) {
        char *data;
        size_t len;
        if (websockets_recv(client.ws, &data, &len)) {
            on_message(&client, data, len);
            free(data);
        }

        // 发送测试消息
        llm_send_text(&client, "我的电量还剩多少？");
        sleep(5);
    }

    pthread_mutex_destroy(&client.lock);
    return 0;
}
```

---

## 11. 错误处理

### 11.1 连接层错误

| 场景 | 处理方式 |
|------|----------|
| 最大连接数已达 | 服务端返回 1013 状态码关闭连接 |
| 心跳超时 | 服务端主动关闭连接 |
| 网络中断 | 客户端应自动重连（间隔 1-5 秒） |

### 11.2 消息层错误

| 场景 | 错误代码 |
|------|----------|
| JSON 格式错误 | `INVALID_MESSAGE` |
| 未知消息类型 | `UNKNOWN_MESSAGE_TYPE` |
| 缺少必需参数 | `INVALID_MESSAGE` |

### 11.3 LLM 层错误

| 场景 | 错误代码 |
|------|----------|
| LLM API 超时 | `TIMEOUT` |
| LLM API 错误 | `LLM_ERROR` |
| 模型未加载 | `LLM_ERROR` |

### 11.4 会话层错误

| 场景 | 错误代码 |
|------|----------|
| 会话不存在 | `SESSION_ERROR` |
| 会话已过期 | `SESSION_ERROR` |

### 11.5 客户端工具层错误

| 场景 | 错误代码 | 说明 |
|------|----------|------|
| 工具名称重复 | `TOOL_REGISTRATION_FAILED` | 注册时工具名已存在 |
| 工具数量超限 | `TOOL_REGISTRATION_FAILED` | 超过最大工具数量 |
| 工具参数格式错误 | `INVALID_TOOL_PARAMETERS` | 参数定义不符合 JSON Schema |
| 工具执行超时 | `TOOL_RESULT_TIMEOUT` | 客户端 30 秒内未返回结果 |
| 工具不存在 | `TOOL_NOT_FOUND` | LLM 请求了不存在的客户端工具 |
| 工具执行失败 | `TOOL_EXECUTION_FAILED` | 客户端返回 success=false |
| call_id 不匹配 | `INVALID_MESSAGE` | 返回的 call_id 无效或已过期 |

---

## 12. 性能与限制

### 12.1 连接限制

| 限制项 | 值 |
|--------|-----|
| 最大并发连接 | 100 |
| 单个消息大小 | 1 MB |
| 心跳间隔 | 30 秒 |
| 心跳超时 | 300 秒（5 分钟）|

### 12.2 会话限制

| 限制项 | 值 |
|--------|-----|
| 会话超时 | 3600 秒（1 小时）|
| 历史消息数 | 10 条 |
| 会话清理间隔 | 60 秒 |

### 12.3 LLM 限制

| 限制项 | 值 |
|--------|-----|
| 请求超时 | 120 秒（2 分钟）|
| 最大输出 tokens | 2048 |
| 温度范围 | 0.0 - 1.0 |

---

## 13. 版本历史

| 版本 | 日期 | 变更 |
|------|------|------|
| 1.2.0 | 2025-02-21 | **客户端工具注册协议**：添加 `register_tools` 客户端消息；添加 `tool_callback` 服务端回调；添加 `tool_result` 客户端响应；添加 `tools_registered` 确认消息；支持双向工具调用（服务端 MCP + 客户端自定义） |
| 1.1.0 | 2025-02-21 | 添加 `enable_context` 上下文控制；响应内容清理优化；添加会话恢复功能 |
| 1.0.0 | 2025-01-XX | 初始版本 |

### 1.2.0 新增消息类型

| 方向 | type | 说明 |
|------|------|------|
| 客户端→服务端 | `register_tools` | 注册客户端工具 |
| 客户端→服务端 | `tool_result` | 返回工具执行结果 |
| 服务端→客户端 | `tool_callback` | 回调客户端执行工具 |
| 服务端→客户端 | `tools_registered` | 工具注册确认 |

---

## 14. 附录

### 14.1 系统提示词

默认系统提示词定义了 AI 的行为和回复风格：

```python
DEFAULT_SYSTEM_PROMPT = """
你是一个名为"听语AI"的语音助手。请遵循以下准则：

1. 回复简洁明了，适合语音播报
2. 避免使用表情符号、特殊符号（如★☆◆◇●■□等）
3. 不要使用Markdown格式（如**加粗**、*斜体*）
4. 使用自然口语化表达
5. 信息准确，如有不确定请说明

当前时间：{current_time}
"""
```

### 14.2 相关端口

| 服务 | 端口 | 协议 |
|------|------|------|
| LLM Gateway | 9400 | WebSocket |
| ASR Service | 9200 | WebSocket |
| TTS Service | 9300 | WebSocket |
| TTS Web UI | 9301 | HTTP |

### 14.3 测试工具

服务端提供了 Web 测试客户端：`cloud/test_client.html`

在浏览器中打开即可测试 WebSocket 连接和消息交互。
