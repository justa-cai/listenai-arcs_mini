# VoxCPM WebSocket TTS Server API 文档

## 概述

VoxCPM WebSocket TTS Server 提供基于WebSocket的文本转语音（TTS）服务。客户端可以通过WebSocket连接发送文本，服务器实时返回合成音频数据。

### 服务器信息

| 项目 | 值 |
|------|-----|
| **协议** | WebSocket |
| **默认地址** | `ws://192.168.1.169:9300/tts` |
| **路径** | `/tts` 或 `/` |
| **Ping间隔** | 30秒 |
| **Ping超时** | 300秒 |
| **最大消息大小** | 1MB |
| **最大连接数** | 100 |
| **最大并发请求** | 10 |

---

## 连接

### 建立连接

```
ws://{host}:{port}/tts
```

示例：
```javascript
const ws = new WebSocket('ws://192.168.1.169:9300/tts');
```

### 连接状态

服务器会定期发送ping消息，客户端需要及时响应以保持连接活跃。如果5分钟内未收到pong响应，服务器将关闭连接。

---

## 消息格式

所有消息均为JSON格式，包含 `type` 字段标识消息类型。

### 通用消息结构

```json
{
  "type": "message_type",
  "...": "其他字段"
}
```

---

## 客户端发送的消息

### 1. TTS 请求 (tts_request)

发送文本转语音请求。

**请求消息：**

```json
{
  "type": "tts_request",
  "request_id": "string (必需，唯一请求ID)",
  "params": {
    "text": "string (必需，要转换的文本)",
    "mode": "streaming|non_streaming (可选，默认streaming)",
    "voice_id": "string (可选，音色ID)",
    "prompt_wav_url": "string (可选，自定义参考音频URL)",
    "prompt_text": "string (可选，参考音频对应文本)",
    "cfg_value": "number (可选，0.1-10.0，默认2.0)",
    "inference_timesteps": "number (可选，1-50，默认30)",
    "normalize": "boolean (可选，默认false)",
    "denoise": "boolean (可选，默认true)",
    "retry_badcase": "boolean (可选，默认true)",
    "retry_badcase_max_times": "number (可选，0-10，默认3)",
    "retry_badcase_ratio_threshold": "number (可选，1.0-20.0，默认6.0)"
  }
}
```

#### 参数说明

| 参数 | 类型 | 必需 | 范围/可选值 | 默认值 | 说明 |
|------|------|------|-------------|--------|------|
| **text** | string | 是 | - | - | 要转换为语音的文本，最大5000字符 |
| **mode** | string | 否 | streaming, non_streaming | streaming | streaming=流式返回音频块，non_streaming=一次性返回完整音频 |
| **voice_id** | string | 否 | 见音色列表 | - | 指定使用预置音色，格式如 "通用场景-阳光青年" |
| **prompt_wav_url** | string | 否 | - | - | 自定义参考音频的URL（用于音色克隆） |
| **prompt_text** | string | 否 | - | - | 参考音频对应的文本 |
| **cfg_value** | number | 否 | 0.1-10.0 | 2.0 | Classifier-free guidance 值，值越高音质越好但可能越不稳定 |
| **inference_timesteps** | number | 否 | 1-50 | 30 | 推理步数，值越高音质越好但速度越慢 |
| **normalize** | boolean | 否 | - | false | 是否归一化音频 |
| **denoise** | boolean | 否 | - | true | 是否对音频进行降噪 |
| **retry_badcase** | boolean | 否 | - | true | 是否重试异常案例 |
| **retry_badcase_max_times** | number | 否 | 0-10 | 3 | 重试最大次数 |
| **retry_badcase_ratio_threshold** | number | 否 | 1.0-20.0 | 6.0 | 重试比率阈值 |

#### 音色选择

可通过 `voice_id` 指定预置音色，也可通过 `prompt_wav_url` 提供自定义参考音频实现音色克隆。

使用 `voice_id` 时，服务器会自动获取对应的音频文件和参考文本。

**音色ID格式：** `{category}-{voice_name}`

示例：
- `"voice_id": "通用场景-阳光青年"`
- `"voice_id": "角色扮演-高冷御姐"`
- `"voice_id": "视频配音-和蔼奶奶"`

完整音色列表见下方"音色列表"章节

**请求示例：**

```json
{
  "type": "tts_request",
  "request_id": "req_001",
  "params": {
    "text": "你好，欢迎使用VoxCPM语音合成服务！",
    "mode": "streaming",
    "voice_id": "通用场景-阳光青年",
    "cfg_value": 2.0,
    "inference_timesteps": 30,
    "denoise": true
  }
}
```

**使用自定义音色克隆示例：**

```json
{
  "type": "tts_request",
  "request_id": "req_002",
  "params": {
    "text": "这是用自定义音色合成的语音",
    "prompt_wav_url": "https://example.com/reference.wav",
    "prompt_text": "这是参考音频的文本内容",
    "mode": "non_streaming"
  }
}
```

---

### 2. 取消请求 (cancel)

取消正在进行的TTS请求。

**请求消息：**

```json
{
  "type": "cancel",
  "request_id": "string (必需，要取消的请求ID)"
}
```

**请求示例：**

```json
{
  "type": "cancel",
  "request_id": "req_001"
}
```

---

### 3. 心跳检测 (ping)

客户端可以主动发送ping消息进行心跳检测。

**请求消息：**

```json
{
  "type": "ping",
  "timestamp": "number (可选，Unix时间戳，秒)"
}
```

**请求示例：**

```json
{
  "type": "ping",
  "timestamp": 1709123456
}
```

---

## 服务器发送的消息

### 1. 进度更新 (progress)

服务器在处理请求过程中发送进度更新。

**消息格式：**

```json
{
  "type": "progress",
  "request_id": "string (请求ID)",
  "state": "string (当前状态)",
  "progress": "number (进度值，0.0-1.0)",
  "message": "string (进度描述)"
}
```

#### 状态值 (state)

| 状态 | 说明 |
|------|------|
| **queued** | 请求已加入队列等待处理 |
| **processing** | 正在处理请求 |
| **generating** | 正在生成音频 |
| **encoding** | 正在编码音频（非流式模式） |
| **completed** | 请求已完成 |
| **cancelled** | 请求已取消 |
| **failed** | 请求失败 |

**消息示例：**

```json
{
  "type": "progress",
  "request_id": "req_001",
  "state": "queued",
  "progress": 0.0,
  "message": "Request queued"
}
```

```json
{
  "type": "progress",
  "request_id": "req_001",
  "state": "generating",
  "progress": 0.3,
  "message": "Generating audio..."
}
```

---

### 2. 音频数据 (二进制帧)

服务器通过二进制帧发送音频数据。

#### 二进制帧结构

```
| 字段 | 大小 | 说明 |
|------|------|------|
| Magic | 2字节 | 固定值 0xAA 0x55 |
| Message Type | 1字节 | 0x01=流式音频块, 0x02=完整音频, 0x03=仅元数据 |
| Reserved | 1字节 | 保留字段，值为 0x00 |
| Metadata Length | 4字节 | 元数据JSON长度（大端序） |
| Metadata JSON | 可变 | 元数据JSON字符串 |
| Payload Length | 4字节 | 音频数据长度（大端序） |
| Audio Payload | 可变 | PCM 16-bit 音频数据（小端序） |
```

#### 流式模式音频块 (0x01)

流式模式下，服务器会发送多个音频块。

**元数据示例：**

```json
{
  "request_id": "req_001",
  "sequence": 0,
  "sample_rate": 16000,
  "is_final": false
}
```

| 字段 | 说明 |
|------|------|
| **request_id** | 对应的请求ID |
| **sequence** | 音频块序号（从0开始递增） |
| **sample_rate** | 采样率（通常为16000） |
| **is_final** | 是否为最后一个块 |

#### 非流式模式完整音频 (0x02)

非流式模式下，服务器一次性发送完整音频。

**元数据示例：**

```json
{
  "request_id": "req_001",
  "sample_rate": 16000,
  "duration": 3.5
}
```

| 字段 | 说明 |
|------|------|
| **request_id** | 对应的请求ID |
| **sample_rate** | 采样率（通常为16000） |
| **duration** | 音频时长（秒） |

#### 音频数据格式

- **编码格式：** PCM 16-bit
- **字节序：** 小端序 (Little-endian)
- **声道数：** 单声道
- **采样率：** 16000 Hz

---

### 3. 完成通知 (complete)

请求处理完成后发送。

**消息格式：**

```json
{
  "type": "complete",
  "request_id": "string (请求ID)",
  "result": {
    "duration": "number (音频时长，秒)",
    "sample_rate": "number (采样率)",
    "samples": "number (采样点数)",
    "chunks": "number (音频块数量，仅流式模式)",
    "cancelled": "boolean (是否被取消，可选)"
  }
}
```

**消息示例：**

流式模式：
```json
{
  "type": "complete",
  "request_id": "req_001",
  "result": {
    "duration": 5.2,
    "sample_rate": 16000,
    "samples": 83200,
    "chunks": 15
  }
}
```

被取消的请求：
```json
{
  "type": "complete",
  "request_id": "req_001",
  "result": {
    "cancelled": true
  }
}
```

---

### 4. 错误消息 (error)

请求过程中发生错误时发送。

**消息格式：**

```json
{
  "type": "error",
  "request_id": "string (可选，请求ID)",
  "error": {
    "code": "string (错误代码)",
    "message": "string (错误描述)",
    "details": "object (可选，错误详情)"
  }
}
```

#### 错误代码

| 错误代码 | HTTP状态码 | 说明 |
|----------|------------|------|
| **INVALID_JSON** | 400 | JSON格式无效 |
| **INVALID_PARAMS** | 400 | 参数验证失败 |
| **TEXT_TOO_LONG** | 400 | 文本超过最大长度 |
| **UNSUPPORTED_FORMAT** | 400 | 不支持的格式 |
| **UNKNOWN_MESSAGE_TYPE** | 400 | 未知消息类型 |
| **VOICE_NOT_FOUND** | 404 | 音色不存在 |
| **VOICE_MANAGER_NOT_AVAILABLE** | 500 | 音色管理器不可用 |
| **MODEL_NOT_LOADED** | 503 | 模型未加载 |
| **GENERATION_FAILED** | 500 | 音频生成失败 |
| **TIMEOUT** | 504 | 请求超时 |
| **RATE_LIMITED** | 429 | 超过速率限制 |
| **QUEUE_FULL** | 503 | 请求队列已满 |
| **INTERNAL_ERROR** | 500 | 内部错误 |

**消息示例：**

```json
{
  "type": "error",
  "request_id": "req_001",
  "error": {
    "code": "VOICE_NOT_FOUND",
    "message": "Voice 'unknown_voice' not found",
    "details": {
      "voice_id": "unknown_voice"
    }
  }
}
```

```json
{
  "type": "error",
  "error": {
    "code": "INVALID_PARAMS",
    "message": "Validation failed",
    "details": {
      "errors": {
        "text": "cannot be empty",
        "cfg_value": "must be between 0.1 and 10.0"
      }
    }
  }
}
```

---

### 5. 心跳响应 (pong)

响应客户端的ping消息。

**消息格式：**

```json
{
  "type": "pong",
  "timestamp": "number (客户端发送的时间戳)",
  "server_time": "number (服务器当前时间戳，Unix时间秒)"
}
```

**消息示例：**

```json
{
  "type": "pong",
  "timestamp": 1709123456,
  "server_time": 1709123457
}
```

---

## 完整交互流程

### 流式模式 (streaming)

```
客户端                                    服务器
  |                                          |
  |---(1) tts_request----------------------->|
  |                                          |
  |<--(2) progress: queued-------------------|
  |                                          |
  |<--(3) progress: processing-------------|
  |                                          |
  |<--(4) progress: generating-------------|
  |                                          |
  |<--(5) binary frame: audio chunk 0------|
  |                                          |
  |<--(6) binary frame: audio chunk 1------|
  |                                          |
  |            ...                           |
  |                                          |
  |<--(7) binary frame: audio chunk N------|
  |                                          |
  |<--(8) complete-------------------------|
  |                                          |
```

### 非流式模式 (non_streaming)

```
客户端                                    服务器
  |                                          |
  |---(1) tts_request----------------------->|
  |                                          |
  |<--(2) progress: queued-------------------|
  |                                          |
  |<--(3) progress: processing-------------|
  |                                          |
  |<--(4) progress: generating-------------|
  |                                          |
  |<--(5) progress: encoding---------------|
  |                                          |
  |<--(6) binary frame: full audio---------|
  |                                          |
  |<--(7) complete-------------------------|
  |                                          |
```

### 取消请求

```
客户端                                    服务器
  |                                          |
  |---(1) tts_request----------------------->|
  |                                          |
  |<--(2) progress: queued-------------------|
  |                                          |
  |---(3) cancel--------------------------->|
  |                                          |
  |<--(4) complete: cancelled--------------|
  |                                          |
```

---

## 代码示例

### JavaScript (浏览器)

```javascript
const ws = new WebSocket('ws://192.168.1.169:9300/tts');

// 连接建立
ws.onopen = () => {
  console.log('WebSocket connected');
  
  // 发送TTS请求
  const request = {
    type: 'tts_request',
    request_id: 'req_' + Date.now(),
    params: {
      text: '你好，欢迎使用VoxCPM语音合成服务！',
      mode: 'streaming',
      voice_id: '通用场景-阳光青年',
      cfg_value: 2.0,
      inference_timesteps: 30
    }
  };
  ws.send(JSON.stringify(request));
};

// 接收消息
ws.onmessage = async (event) => {
  if (typeof event.data === 'string') {
    // JSON消息
    const message = JSON.parse(event.data);
    handleJsonMessage(message);
  } else {
    // 二进制音频数据
    const audioData = parseAudioFrame(event.data);
    await playAudio(audioData);
  }
};

// 处理JSON消息
function handleJsonMessage(message) {
  switch (message.type) {
    case 'progress':
      console.log(`[${message.request_id}] ${message.progress.toFixed(0)}% - ${message.message}`);
      break;
    case 'complete':
      console.log(`[${message.request_id}] 完成:`, message.result);
      break;
    case 'error':
      console.error(`[${message.request_id}] 错误:`, message.error);
      break;
    case 'pong':
      console.log('Pong received');
      break;
  }
}

// 解析二进制音频帧
function parseAudioFrame(buffer) {
  const dataView = new DataView(buffer);
  
  // 验证Magic
  const magic1 = dataView.getUint8(0);
  const magic2 = dataView.getUint8(1);
  if (magic1 !== 0xAA || magic2 !== 0x55) {
    throw new Error('Invalid frame format');
  }
  
  // 读取消息类型
  const msgType = dataView.getUint8(2);
  
  // 跳过保留字节
  const reserved = dataView.getUint8(3);
  
  // 读取元数据长度
  const metadataLength = dataView.getUint32(4, false); // big-endian
  
  // 读取元数据JSON
  const metadataBytes = new Uint8Array(buffer, 8, metadataLength);
  const metadata = JSON.parse(new TextDecoder().decode(metadataBytes));
  
  // 读取音频数据长度
  const audioLength = dataView.getUint32(8 + metadataLength, false);
  
  // 读取音频数据
  const audioData = new Uint8Array(buffer, 12 + metadataLength, audioLength);
  
  return {
    type: msgType,
    metadata: metadata,
    audio: audioData
  };
}

// 播放音频
async function playAudio(frame) {
  const audioContext = new (window.AudioContext || window.webkitAudioContext)();
  const audioBuffer = audioContext.createBuffer(1, frame.audio.length / 2, frame.metadata.sample_rate);
  const channelData = audioBuffer.getChannelData(0);
  
  // 转换PCM 16-bit到Float32
  const dataView = new DataView(frame.audio.buffer);
  for (let i = 0; i < frame.audio.length / 2; i++) {
    const sample = dataView.getInt16(i * 2, true); // little-endian
    channelData[i] = sample / 32768.0;
  }
  
  const source = audioContext.createBufferSource();
  source.buffer = audioBuffer;
  source.connect(audioContext.destination);
  source.start();
}

// 心跳
setInterval(() => {
  if (ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify({
      type: 'ping',
      timestamp: Math.floor(Date.now() / 1000)
    }));
  }
}, 30000);
```

### Python

```python
import asyncio
import json
import websockets
import numpy as np
import sounddevice as sd
from datetime import datetime

async def tts_client():
    uri = "ws://192.168.1.169:9300/tts"
    
    async with websockets.connect(uri) as websocket:
        print("Connected to TTS server")
        
        # 发送TTS请求
        request = {
            "type": "tts_request",
            "request_id": f"req_{int(datetime.now().timestamp())}",
            "params": {
                "text": "你好，欢迎使用VoxCPM语音合成服务！",
                "mode": "streaming",
                "voice_id": "通用场景-阳光青年",
                "cfg_value": 2.0,
                "inference_timesteps": 30
            }
        }
        
        await websocket.send(json.dumps(request))
        print(f"Sent TTS request: {request['request_id']}")
        
        # 接收消息
        audio_buffer = []
        while True:
            message = await websocket.recv()
            
            if isinstance(message, str):
                # JSON消息
                data = json.loads(message)
                handle_json_message(data)
            else:
                # 二进制音频数据
                audio_data = parse_audio_frame(message)
                if audio_data:
                    audio_buffer.append(audio_data)
                    print(f"Received audio chunk: {len(audio_data)} samples")
        
        # 播放音频
        if audio_buffer:
            full_audio = np.concatenate(audio_buffer)
            sd.play(full_audio, samplerate=16000)
            sd.wait()

def handle_json_message(data):
    """处理JSON消息"""
    msg_type = data.get('type')
    
    if msg_type == 'progress':
        print(f"[{data['request_id']}] {data['progress']*100:.0f}% - {data['message']}")
    elif msg_type == 'complete':
        print(f"[{data['request_id']}] 完成: {data['result']}")
    elif msg_type == 'error':
        print(f"错误: {data['error']}")
    elif msg_type == 'pong':
        print(f"Pong received (server_time: {data['server_time']})")

def parse_audio_frame(buffer):
    """解析二进制音频帧"""
    data = bytes(buffer)
    
    # 验证Magic
    if data[0] != 0xAA or data[1] != 0x55:
        print("Invalid frame format")
        return None
    
    # 读取消息类型
    msg_type = data[2]
    
    # 读取元数据长度 (big-endian)
    metadata_length = int.from_bytes(data[4:8], byteorder='big')
    
    # 读取元数据JSON
    metadata_json = data[8:8+metadata_length].decode('utf-8')
    metadata = json.loads(metadata_json)
    
    # 读取音频数据长度 (big-endian)
    audio_length = int.from_bytes(data[8+metadata_length:12+metadata_length], byteorder='big')
    
    # 读取音频数据 (PCM 16-bit little-endian)
    audio_bytes = data[12+metadata_length:12+metadata_length+audio_length]
    
    # 转换为numpy数组
    samples = np.frombuffer(audio_bytes, dtype=np.int16)
    audio_float = samples.astype(np.float32) / 32768.0
    
    return audio_float

async def heartbeat(websocket):
    """心跳检测"""
    while True:
        try:
            await websocket.send(json.dumps({
                "type": "ping",
                "timestamp": int(datetime.now().timestamp())
            }))
            await asyncio.sleep(30)
        except:
            break

if __name__ == "__main__":
    asyncio.run(tts_client())
```

---

## 音色列表

### 音色参数说明

在使用TTS服务时，可以通过 `voice_id` 参数指定音色。音色ID格式为 `category-voice_name`。

| 参数 | 类型 | 范围 | 默认值 | 说明 |
|------|------|------|--------|------|
| **voice_id** | string | - | - | 音色ID，格式："category-voice_name" |
| **cfg_value** | float | 0.1-10.0 | 2.0 | Classifier-free guidance 值 |
| **inference_timesteps** | int | 1-50 | 30 | 推理步数 |
| **normalize** | bool | - | False | 是否归一化音频 |
| **denoise** | bool | - | True | 是否降噪 |
| **retry_badcase** | bool | - | True | 是否重试异常案例 |
| **retry_badcase_max_times** | int | 0-10 | 3 | 重试最大次数 |
| **retry_badcase_ratio_threshold** | float | 1.0-20.0 | 6.0 | 重试比率阈值 |

---

### 一、通用场景 (21个音色)

| 音色名称 | 音色ID | 示例文本 |
|----------|--------|----------|
| 渊博小叔 | `通用场景-渊博小叔` | 你要知道，这世间的知识就如同浩瀚的海洋，无穷无尽啊。我们要始终保持一颗求知的心，不断去探索、去学习。无论是科学、历史、文化还是艺术，每一个领域都有着无尽的奥秘等待我们去揭开。 |
| 开朗轻快 | `通用场景-开朗轻快` | 今天又是超棒的一天，不管遇到啥，都不能影响我的好心情，冲呀！ |
| 阳光青年 | `通用场景-阳光青年` | 今天又是超棒的一天呀！阳光这么好，心情也跟着超级美丽呢！生活嘛，就该充满活力和欢笑呀！我呀，要像那灿烂的阳光一样，永远积极向上，去追寻自己的梦想，去体验各种好玩的事情，去认识更多有趣的人！ |
| 暖心体贴 | `通用场景-暖心体贴` | 你看起来有些疲惫，是不是累了？先休息一下吧，我帮你把事情处理好。 |
| 甜美悦悦 | `通用场景-甜美悦悦` | 你喜欢看电影吗？电影就像是一个个奇妙的世界，能让我们沉浸其中，体验各种不同的人生。有搞笑的喜剧让我们开怀大笑，有感人的剧情片让我们热泪盈眶，你最喜欢哪种类型的电影呢？ |
| 少年梓辛 | `通用场景-少年梓辛` | 今天的阳光真好啊！感觉整个人都充满了活力呢。真想去外面跑一跑，去探索那些有趣的地方，去邂逅一些奇妙的事情。生活嘛，就该这样自由自在、充满朝气的呀！哈哈！ |
| 邻家男孩 | `通用场景-邻家男孩` | 今天在这里遇到你，真的感觉特别奇妙。就好像命运的齿轮悄然转动，让我们在这个特定的时间和地点相遇。这一定是一种特别的缘分吧。你看，周围的人来人往，而我的目光却不由自主地被你吸引。 |
| 灿灿 | `通用场景-灿灿` | 刚刚还在想你怎么还不来找我聊天，你就来了，真是心有灵犀呀。 |
| 清新女声 | `通用场景-清新女声` | 清晨的第一缕阳光洒下，仿佛为世界披上了一层金色的纱衣，在这宁静美好的时刻，感受着生命的温柔与力量。 |
| 温暖阿虎 | `通用场景-温暖阿虎` | 早上好呀，今天的阳光特别灿烂，就像你的笑容一样，让我倍感温暖。 |
| 开朗姐姐 | `通用场景-开朗姐姐` | 嘿，你好！在这个丰富多彩的世界里，有无数的风景等待着我们去探索。不知道你是否也和我一样，对远方充满了好奇与向往呢？ |
| 清澈梓梓 | `通用场景-清澈梓梓` | 你好呀！我最近对瑜伽特 别着迷，每当我铺开瑜伽垫，舒展身体，仿佛进入了一个宁静而美好的世界。 |
| 甜美小源 | `通用场景-甜美小源` | 你好，我是你的虚拟助理，我随时在这里，陪你聊聊天，分享生活中的喜怒哀乐哦。如果你有任何问题或者需要建议，都可以随时问我呢。期待你的分享。 |
| 爽快思思 | `通用场景-爽快思思` | 今天天气可好了，我打算和朋友一起去野餐，带上美食和饮料，找个舒适的草坪，什么烦恼都没了。你要不要和我们一起呀？ |
| 知性女声 | `通用场景-知性女声` | 在文学的世界里漫步，如同与无数智者倾心交谈，从诗词的优美到散文的灵动，每一种文字都能让心灵得到滋养与慰藉。 |
| 清爽男大 | `通用场景-清爽男大` | 青春就是要敢闯敢拼，不怕失败。我们要在这热血的年纪，去追逐梦想，去体验不同的风景，让青春不留遗憾。 |
| 温柔文雅 | `通用场景-温柔文雅` | 清风徐来，水波不兴，世间纷扰，亦当以优雅之态处之，心平气和，方显从容。 |
| 邻家女孩 | `通用场景-邻家女孩` | 哎呀，你来找我啦！今天过得怎么样呀？我今天看到院子里的花开了呢，可漂亮啦！你想不想和我一起去看看呀？ |
| 解说小明 | `通用场景-解说小明` | 嘿，你好呀！今天的阳光格外温暖，就像你的笑容一样，瞬间照亮了我的世界。刚刚看到你的那一刻，我就觉得有一种特别的吸引力。 |
| 知性温婉 | `通用场景-知性温婉` | 生活的美好常隐匿于细微之处，我们需以平和之心去感知，方能领略其真谛，愿你也能有此心境。 |
| 心灵鸡汤 | `通用场景-心灵鸡汤` | 人生的意义是不断地追求。不要等错过了才悔恨，不要等老了才怀念。抓住当下，再苦再累也要展翅飞翔。 |

---

### 二、视频配音 (18个音色)

| 音色名称 | 音色ID | 示例文本 |
|----------|--------|----------|
| 佩奇猪 | `视频配音-佩奇猪` | 我喜欢和朋友们一起玩耍，在泥坑里跳来跳去可开心了，还有我的弟弟乔治，我们总是能发现好多好玩的事情。 |
| 猴哥 | `视频配音-猴哥` | 俺老孙神通广大，一个筋斗云能翻十万八千里，这天地间就没有俺老孙去不了的地方，管他什么妖魔鬼怪，都得惧我三分。 |
| 贴心女声 | `视频配音-贴心女声` | 音乐是心灵的语言，在音符的跳跃间，能传达出喜怒哀乐。沉浸在音乐的海洋里，仿佛能触摸到灵魂的最深处。 |
| 熊二 | `视频配音-熊二` | 光头强又在砍树，俺得去阻止他，森林是俺们的家，不能让他给破坏了，俺要守护好俺们的蜂蜜和小伙伴们。 |
| 磁性解说男声 | `视频配音-磁性解说男声` | 在浩瀚的宇宙中，星辰闪烁，每一颗都蕴含着无尽的奥秘，我们跟随探索的脚步，去解读宇宙深处的神秘密码。 |
| 亮嗓萌仔 | `视频配音-亮嗓萌仔` | 比奇堡的海绵，以纯真之心，在奇幻海洋制造欢乐 |
| 广告解说 | `视频配音-广告解说` | 全新升级的这款产品，融合了顶尖科技与时尚设计，它将全方位满足您的需求，成为您生活中不可或缺的好帮手。 |
| 樱桃丸子 | `视频配音-樱桃丸子` | 我好想快点长大呀，这样就可以做自己想做的事情，不用再听妈妈唠叨，还能买好多好多喜欢的东西。 |
| 萌丫头 | `视频配音-萌丫头` | 我今天在花园里看到好多漂亮的蝴蝶，它们飞来飞去像在跳舞，我要是也能像它们一样自由自在就好了。 |
| 鸡汤妹妹 | `视频配音-鸡汤妹妹` | 生活就像一杯茶，不会苦一辈子，但总会苦一阵子。只要我们坚持下去，就一定能品到那回甘的滋味，收获美好。 |
| 四郎 | `视频配音-四郎` | 这宫廷之中，人心险恶，朕虽贵为天子，却也有诸多无奈，唯有嬛嬛，是朕心中最后的温暖与慰藉。 |
| 温柔小雅 | `视频配音-温柔小雅` | 只是刚刚不经意间看到你，便觉得周围的一切都变得温柔起来。今日有幸在此相遇，不知是否是上天赐予的缘分呢？ |
| 邻居阿姨 | `视频配音-邻居阿姨` | 你最近工作怎么样啊？累不累呀，要注意身体哦，有什么要帮忙的尽管说，别客气，咱们都是好邻居，应该互相照应。 |
| 顾姐 | `视频配音-顾姐` | 你们这群蠢货，连这点小事都办不好？时尚界的规则如同战场法则，只有强者才能掌控全局。我顾里可不会容忍任何瑕疵，想要跟上我的步伐，就拿出你们的本事来，别在我面前丢人现眼。 |
| 懒音绵宝 | `视频配音-懒音绵宝` | 哎呀，先睡会儿，有事儿等我睡醒再说吧 |
| 和蔼奶奶 | `视频配音-和蔼奶奶` | 乖孙啊，你最近过得咋样啊？奶奶可想你了。工作别太辛苦了，注意身体，有时间多回家看看奶奶。奶奶给你做好吃的。你要是有什么心事，也可以跟奶奶说。 |
| 俏皮女声 | `视频配音-俏皮女声` | 你这个小迷糊，又忘记东西放在哪里了吧？没关系啦，我来帮你找，下次可不许再这么粗心咯。 |
| 天才童声 | `视频配音-天才童声` | 我唱歌超棒，老师都夸我有天赋，我要更努力，以后在大舞台上唱给全世界听，让所有人都为我鼓掌欢呼。 |
| 少儿故事 | `视频配音-少儿故事` | 森林里住着一只聪明的小狐狸，它总是能想出各种奇妙的点子，帮助小伙伴们解决难题，大家都可喜欢它啦。 |
| 武则天 | `视频配音-武则天` | 本宫的威严岂容置疑，朝堂之上，众臣皆需遵循本宫旨意，若有违抗，定当严惩不贷，本宫定要这天下长治久安。 |

---

### 三、多语种 (13个音色)

| 音色名称 | 音色ID | 示例文本 |
|----------|--------|----------|
| Anna | `多语种-Anna` | Dreams are the stars that light up my path. I won't let obstacles dim their shine. I'll work hard, step by step, to turn those dreams into reality. Because in the pursuit, I find the true meaning and joy of living. |
| Jackson | `多语种-Jackson` | Yo squad turn it UP! Glow sticks in the air. I wanna see mosh pits forming NOW! |
| Amanda | `多语种-Amanda` | Page 12 foreshadowing the icepick murder – hear that bone-chilling wind through lace curtains? |
| Morgan | `多语种-Morgan` | Listen, I just watched an amazing documentary. The way they presented the story was so captivating. It made me realize how powerful a good narration can be. It can really bring a whole new world to life. |
| Alvin | `多语种-Alvin` | So, what do you want to talk about? Sports? Movies? Music? Or anything else that comes to mind. Let's have a great conversation! |
| Cutey | `多语种-Cutey` | Oh, I saw a really cute puppy on the street. It was so fluffy and had the sweetest little face. I wanted to take it home right away. Don't you just love cute things? |
| Hope | `多语种-Hope` | You know, even when things seem tough, don't give up. Every setback is a chance to grow. There's always a silver lining. Believe in yourself and keep moving forward. You've got this! |
| Smith | `多语种-Smith` | I'm a man of principles. I will never compromise my beliefs for temporary gains. Justice and fairness are what I pursue. I will stand up and fight for what is right, no matter how strong the opposition is. |
| Skye | `多语种-Skye` | Hey everyone! I'm a girl who really loves to do meditation. When I sit down cross-legged, close my eyes and focus on my breath, it's like entering a peaceful world of my own. I feel my body and mind gradually relaxing, all the stress and worries seem to fade away. |
| Shiny | `多语种-Shiny` | Hey, you seem a bit down. Let's look on the bright side! There's always something good around the corner. A smile can change the world, so let's shine together and make it a better place. |
| Candy | `多语种-Candy` | Darling, I noticed you're a bit stressed. How about we relax? We can have a cup of tea, listen to some soft music, and just forget about the worries for a while. You deserve a break. |
| Brayan | `多语种-Brayan` | How are you today? I had a really cool day at school today. We had a great science class and I learned some fascinating stuff. And then I played basketball with my friends during the break, it was so much fun. What about you? |
| Harmony | `多语种-Harmony` | Hey there! I'm a sports-loving boy! You know, I'm always full of energy and passion for sports. Whether it's running on the track, shooting hoops on the basketball court, or kicking the ball on the soccer field, I'm always in the game. |
| Adam | `多语种-Adam` | I'm not afraid of challenges. I believe that with my determination and efforts, I can break through any difficulties and reach the peak of success. I'm ready to face whatever comes my way. |

---

### 四、角色扮演 (20个音色)

| 音色名称 | 音色ID | 示例文本 |
|----------|--------|----------|
| 娇弱萝莉 | `角色扮演-娇弱萝莉` | 哎呀，人家好怕怕，这个该怎么办呀？你能不能帮帮我，好不好嘛。 |
| 潇洒随性 | `角色扮演-潇洒随性` | 世间纷扰，何必拘泥，随心而为，方得自在，走，一起去看那未知风景。 |
| 绿茶小哥 | `角色扮演-绿茶小哥` | 这事儿啊，我本不想多言，但看你如此为难，我就冒险试试吧，只希望别得罪了旁人，我也是一片好心呐。 |
| 傲慢娇声 | `角色扮演-傲慢娇声` | 你们这些人，都要好好伺候本小姐，若是有差池，定不轻饶。 |
| 撒娇学妹 | `角色扮演-撒娇学妹` | 等会儿你一定要好好地、用心地让人家品尝一下你亲自做的美食哟~人家可期待了呢，你做的肯定超级超级好吃，人家现在就已经迫不及待啦，亲爱的最好了啦~ |
| 高冷御姐 | `角色扮演-高冷御姐` | 哼，亲爱的，我们到此为止吧，我倦了，不想再继续这场无聊的游戏了。你我之间，也许曾经有一些美好，但那也只是曾经罢了。 |
| 东方浩然 | `角色扮演-东方浩然` | 我上知天文，从遥远星系的诞生到天体的运行规律，皆能娓娓道来。下知地理，无论是古老文明的发祥地，还是新兴城市的崛起之地，我都能洞察其背后的历史脉络与发展轨迹。 |
| 病弱少女 | `角色扮演-病弱少女` | 你好呀，我感觉今天身体好了一点点，虽然还是有一点虚弱。希望我能赶快好起来，和你一起度过更多美好的时光，谢谢你的关心和陪伴。 |
| 奶气萌娃 | `角色扮演-奶气萌娃` | 我不要睡觉，星星还没和我说完悄悄话呢，它们会告诉我好多小秘密，等我听完再睡好不好呀？ |
| 活泼女孩 | `角色扮演-活泼女孩` | 嗨，亲爱的，今天我去一个超级有趣的地方，那里有好多好玩东西，我尝试了一些新的美食，还拍了好多好看的照片呢。 |
| 冷淡疏离 | `角色扮演-冷淡疏离` | 与我无关之事，莫要打扰。我独自行于这世间，无需他人过多介入。 |
| 憨厚敦实 | `角色扮演-憨厚敦实` | 俺没啥心眼，就知道实实在在做事，你说咋干，俺就咋干，绝不含糊。 |
| 活泼刁蛮 | `角色扮演-活泼刁蛮` | 我就要这个，你不给我，我就一直缠着你，哼，看你能拿我怎样。 |
| 撒娇粘人 | `角色扮演-撒娇粘人` | 亲爱的，你不要走嘛，你不在我身边，我心里空落落的，陪陪我好不好。 |
| 傲娇霸总 | `角色扮演-傲娇霸总` | 宝贝，你给我记住，你是我的人，这辈子都是。我不允许你看别的男人一眼，你的心里只能有我。不管你遇到什么事，都有我在，我会为你摆平一切。别总想着离开我，你逃不出我的手掌心的。 |
| 婆婆 | `角色扮演-婆婆` | 你们年轻人啊，做事要沉稳，别毛毛躁躁的，吃亏是福，要懂得包容和体谅，这样才能把日子过得顺顺当当。 |
| 魅力女友 | `角色扮演-魅力女友` | 以后呢，你只能对我一个人好，心里也只能装着我。不管发生什么，都要第一时间想到我哦。我可会一直赖着你的，你别想跑掉啦。还有呀，要好好爱我，宠我，不然我可不依呢，哼！ |
| 深夜播客 | `角色扮演-深夜播客` | 在这寂静的夜里，我陪着你们，一起度过这独特的时光。我知道你们可能带着一天的疲惫来到这里，或是有着各种各样的心情，别担心，有我在呢。让我的声音陪伴你们，为你们赶走孤单，带来一些慰藉和欢乐。 |
| 固执病娇 | `角色扮演-固执病娇` | 你只能属于我，谁也别想靠近，若是违背，我定不会善罢甘休。 |
| 柔美女友 | `角色扮演-柔美女友` | 亲爱的，这么晚啦，你还没睡呀。我想跟你说哦，不管什么时候，我都会在你身边的呀。你累的时候就靠靠我，不开心了我就哄你开心。 |
| 傲气凌人 | `角色扮演-傲气凌人` | 哼，这等小事，我轻易便能做到，你们且学着吧，莫要在我面前班门弄斧。 |

---

### 五、趣味口音 (7个音色)

| 音色名称 | 音色ID | 示例文本 |
|----------|--------|----------|
| 浩宇小哥 | `趣味口音-浩宇小哥` | 这海风可真是挺舒服的呀！咱打算去哪儿溜达溜达转转呀？是去海边瞅瞅呢，还是去哪个有意思的地儿逛逛呀？真是怪让人纠结滴！ |
| 京腔侃爷 | `趣味口音-京腔侃爷` | 北京的堵车啊，能让你从早上堵到下午，下车一看，哟，这地儿咋这么眼熟呢？哦，原来还在家门口儿呢！ |
| 湾区大叔 | `趣味口音-湾区大叔` | 年轻人啊，你要知道哦，人生这条路啊，可长着呢！有时候要慢慢来啦，莫急莫急。遇到困难不要怕啦，勇敢去面对就好。咱要保持乐观的心态啦，日子总是会越来越好的。 |
| 广州德哥 | `趣味口音-广州德哥` | 嗨，今天饮茶没有呀，一起喝茶聊聊呀。 |
| 湾湾小何 | `趣味口音-湾湾小何` | 今天天气真是太好了，阳光灿烂，心情超级棒！但是，朋友最近的感情问题也让我心痛不已，好像世界末日一样，真的好为她难过哦！ |
| 广西远舟 | `趣味口音-广西远舟` | 咱广西的风景可美了，山清水秀的，空气也特别清新。要是你有机会来广西旅游，一定要去桂林看看那甲天下的山水，再去尝尝我们广西的特色美食，像螺蛳粉、桂林米粉这些，味道绝对让你流连忘返！ |
| 呆萌川妹 | `趣味口音-呆萌川妹` | 哎呀，你晓得不嘛，今天天气好好哟，人家好想出去耍一哈儿嘛，好不好嘛~ |

---

### 六、有声阅读 (8个音色)

| 音色名称 | 音色ID | 示例文本 |
|----------|--------|----------|
| 儒雅青年 | `有声阅读-儒雅青年` | 君子当以文会友，以仁处世。于书卷间探寻智慧，在谈笑中尽显风度。不骄不躁，沉稳自若，以礼义为纲，行于这纷繁世间，守心中一方净土。 |
| 活力小哥 | `有声阅读-活力小哥` | 哈哈，这世界如此美好，哪有时间烦恼。怀揣梦想大步向前，跌倒了又怎样，爬起来拍拍土，继续追逐那灿烂阳光，让快乐与活力感染身边每一个人。 |
| 古风少御 | `有声阅读-古风少御` | 本欲于这乱世寻一清幽，然才情难掩，亦当以笔为剑，以诗抒怀。虽为女儿身，亦敢在这江湖留名，愿以古风雅韵，书尽人间百态，绘就山河锦绣。 |
| 悬疑解说 | `有声阅读-悬疑解说` | 那座古老的城堡中，传出阵阵诡异的声响，黑暗的走廊里似乎有双眼睛在窥视，真相被重重迷雾包裹，等待着被揭开的那一刻。 |
| 霸气青叔 | `有声阅读-霸气青叔` | 这天下大势，皆在吾一念之间。吾之所向，披荆斩棘亦要达成。顺我者昌，逆我者亡，吾之威严，不容置疑，且看吾如何铸就辉煌霸业。 |
| 反卷青年 | `有声阅读-反卷青年` | 六点准时关机，谁爱加班谁加。走啊老王，天台烧烤配乌苏！ |
| 温柔淑女 | `有声阅读-温柔淑女` | 愿世间皆被温柔以待，我以浅笑安然处之。轻声细语解君忧，善念善行暖人心。不求惊天动地之功业，但求在平凡中绽放爱与善意，做那春日里的一缕微风。 |
| 擎苍 | `有声阅读-擎苍` | 吾欲擎天之巨手，揽尽世间风云变幻。诸般阻碍，不过是吾迈向巅峰之基石。今日之辱，来日必当十倍奉还，吾之威名，必将震慑四海八荒。 |

---

### 七、全部 (精选集合)

| 音色名称 | 音色ID | 示例文本 |
|----------|--------|----------|
| 渊博小叔 | `全部-渊博小叔` | 你要知道，这世间的知识就如同浩瀚的海洋，无穷无尽啊。我们要始终保持一颗求知的心，不断去探索、去学习。无论是科学、历史、文化还是艺术，每一个领域都有着无尽的奥秘等待我们去揭开。 |
| 阳光青年 | `全部-阳光青年` | 今天又是超棒的一天呀！阳光这么好，心情也跟着超级美丽呢！生活嘛，就该充满活力和欢笑呀！我呀，要像那灿烂的阳光一样，永远积极向上，去追寻自己的梦想，去体验各种好玩的事情，去认识更多有趣的人！ |
| 少年梓辛 | `全部-少年梓辛` | 今天的阳光真好啊！感觉整个人都充满了活力呢。真想去外面跑一跑，去探索那些有趣的地方，去邂逅一些奇妙的事情。生活嘛，就该这样自由自在、充满朝气的呀！哈哈！ |
| 豫州子轩 | `全部-豫州子轩` | 咱河南可是个好地方啊！咱这儿历史悠久，文化深厚。那烩面，吃起来可得劲儿了，汤浓面筋，一碗下去，浑身舒坦。还有胡辣汤，早上来一碗，那叫一个带劲！还有那洛阳水席，那可是咱河南的特色，恁有机会可得尝尝。咱河南人实在，欢迎恁来河南耍啊！ |
| 浩宇小哥 | `全部-浩宇小哥` | 这海风可真是挺舒服的呀！咱打算去哪儿溜达溜达转转呀？是去海边瞅瞅呢，还是去哪个有意思的地儿逛逛呀？真是怪让人纠结滴！ |
| 高冷御姐 | `全部-高冷御姐` | 哼，亲爱的，我们到此为止吧，我倦了，不想再继续这场无聊的游戏了。你我之间，也许曾经有一些美好，但那也只是曾经罢了。 |
| 温暖阿虎 | `全部-温暖阿虎` | 早上好呀，今天的阳光特别灿烂，就像你的笑容一样，让我倍感温暖。 |
| 京腔侃爷 | `全部-京腔侃爷` | 北京的堵车啊，能让你从早上堵到下午，下车一看，哟，这地儿咋这么眼熟呢？哦，原来还在家门口儿呢！ |
| 湾区大叔 | `全部-湾区大叔` | 年轻人啊，你要知道哦，人生这条路啊，可长着呢！有时候要慢慢来啦，莫急莫急。遇到困难不要怕啦，勇敢去面对就好。咱要保持乐观的心态啦，日子总是会越来越好的。 |
| 妹坨洁儿 | `全部-妹坨洁儿` | 我们长沙的美食那可真是多得不得了嘞！那臭豆腐呀，闻起来臭，吃起来香得很嘞，粉就更不用说咯，早上来一碗，精神一整天嘞！这些都是我们长沙的宝嘞，你们一定要尝尝看咯！ |
| 爽快思思 | `全部-爽快思思` | 今天天气可好了，我打算和朋友一起去野餐，带上美食和饮料，找个舒适的草坪，什么烦恼都没了。你要不要和我们一起呀？ |
| 广州德哥 | `全部-广州德哥` | 嗨，今天饮茶没有呀，一起喝茶聊聊呀。 |
| 湾湾小何 | `全部-湾湾小何` | 今天天气真是太好了，阳光灿烂，心情超级棒！但是，朋友最近的感情问题也让我心痛不已，好像世界末日一样，真的好为她难过哦！ |
| 傲娇霸总 | `全部-傲娇霸总` | 宝贝，你给我记住，你是我的人，这辈子都是。我不允许你看别的男人一眼，你的心里只能有我。不管你遇到什么事，都有我在，我会为你摆平一切。别总想着离开我，你逃不出我的手掌心的。 |
| 北京小爷 | `全部-北京小爷` | 您猜怎么着，咱北京那可是有着深厚底蕴的地儿啊！咱这胡同里的故事啊，那真是说也说不完。咱得把咱这老祖宗留下来的好东西都传下去，让全世界都瞅瞅咱北京的魅力，您说是不是这个理儿呀！ |
| 邻家女孩 | `全部-邻家女孩` | 哎呀，你来找我啦！今天过得怎么样呀？我今天看到院子里的花开了呢，可漂亮啦！你想不想和我一起去看看呀？ |
| 魅力女友 | `全部-魅力女友` | 以后呢，你只能对我一个人好，心里也只能装着我。不管发生什么，都要第一时间想到我哦。我可会一直赖着你的，你别想跑掉啦。还有呀，要好好爱我，宠我，不然我可不依呢，哼！ |
| 广西远舟 | `全部-广西远舟` | 咱广西的风景可美了，山清水秀的，空气也特别清新。要是你有机会来广西旅游，一定要去桂林看看那甲天下的山水，再去尝尝我们广西的特色美食，像螺蛳粉、桂林米粉这些，味道绝对让你流连忘返！ |
| 深夜播客 | `全部-深夜播客` | 在这寂静的夜里，我陪着你们，一起度过这独特的时光。我知道你们可能带着一天的疲惫来到这里，或是有着各种各样的心情，别担心，有我在呢。让我的声音陪伴你们，为你们赶走孤单，带来一些慰藉和欢乐。 |
| 柔美女友 | `全部-柔美女友` | 亲爱的，这么晚啦，你还没睡呀。我想跟你说哦，不管什么时候，我都会在你身边的呀。你累的时候就靠靠我，不开心了我就哄你开心。 |
| 呆萌川妹 | `全部-呆萌川妹` | 哎呀，你晓得不嘛，今天天气好好哟，人家好想出去耍一哈儿嘛，好不好嘛~ |

---

### 音色统计

- **总计音色数量**: 104 个
- **分类数量**: 7 个

| 分类 | 音色数量 |
|------|----------|
| 通用场景 | 21 |
| 视频配音 | 18 |
| 多语种 | 13 |
| 角色扮演 | 20 |
| 趣味口音 | 7 |
| 有声阅读 | 8 |
| 全部 | 21 (精选) |

---

## 注意事项

### 连接管理

1. **保持心跳**：客户端应定期处理服务器的ping消息，或在30秒间隔内发送ping消息
2. **处理重连**：当连接断开时，客户端应实现自动重连逻辑
3. **并发请求**：单个连接可以发送多个TTS请求，服务器通过 `request_id` 区分

### 参数建议

| 参数 | 推荐值 | 说明 |
|------|--------|------|
| **mode** | streaming | 需要实时播放时使用流式模式 |
| **cfg_value** | 2.0-3.0 | 默认值2.0，可根据需要调整 |
| **inference_timesteps** | 30 | 默认值30，平衡音质与速度 |
| **denoise** | true | 建议开启以获得更好的音质 |

### 性能优化

1. **流式模式**：适合实时播放场景，降低延迟
2. **非流式模式**：适合批量处理或需要完整音频文件的场景
3. **并发控制**：服务器最多支持10个并发请求，超出部分会排队

### 错误处理

1. **参数验证失败**：检查参数类型和范围是否符合要求
2. **音色不存在**：确认 `voice_id` 是否正确，或使用 `prompt_wav_url` 提供自定义音色
3. **队列已满**：等待一段时间后重试
4. **请求超时**：可能是文本过长或服务器负载过高，考虑缩短文本或稍后重试

---

## 环境变量配置

服务器可通过环境变量进行配置：

| 环境变量 | 默认值 | 说明 |
|----------|--------|------|
| TTS_HOST | 192.168.1.169 | 服务器监听地址 |
| TTS_PORT | 9300 | 服务器端口 |
| TTS_MAX_CONNECTIONS | 100 | 最大连接数 |
| TTS_MAX_CONCURRENT | 10 | 最大并发请求数 |
| TTS_REQUEST_TIMEOUT | 600 | 请求超时时间（秒） |
| TTS_RATE_LIMIT_ENABLED | true | 是否启用速率限制 |
| TTS_RATE_LIMIT_PER_MINUTE | 60 | 每分钟最大请求数 |
| TTS_MODEL_NAME | VoxCPM-0.5B | 模型名称 |
| TTS_DEVICE | cuda | 推理设备 |
| TTS_VOICE_DIR | voice_clone | 音色目录 |

---

*文档版本: 1.0*
*最后更新: 2026-02-20*
