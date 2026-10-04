# VoxCPM TTS Server API 文档

## 概述

VoxCPM TTS 服务提供双协议接口，支持 WebSocket 流式传输和 HTTP REST API。

### 特性

- **双协议**：WebSocket 流式 + HTTP REST API
- **流式传输**：边生成边发送，实时播放，低延迟
- **语音克隆**：支持自定义参考音频（prompt_wav + prompt_text）
- **可控克隆**：支持 reference_wav 进行音色控制
- **声音设计**：支持自然语言描述生成音色（voice_design）
- **多音色源**：doubao / xunfei / aliyun / LibriTTS，共 2500+ 音色
- **多实例并发**：Worker Pool 并行处理多请求
- **中断控制**：支持中断正在进行的生成

### 服务规格

| 项目 | 规格 |
|------|------|
| 协议 | WebSocket + HTTP |
| 音频格式 | 16-bit PCM, mono |
| 模型输出采样率 | 48000 Hz（VoxCPM2 原生） |
| WebSocket 流式采样率 | 16000 Hz（节省带宽） |
| HTTP 下载采样率 | 48000 Hz（原生品质） |
| WebSocket 默认端口 | 8765 |
| HTTP 默认端口 | 9000 |
| 默认地址 | ws://192.168.1.169:8765 / http://192.168.1.169:9000 |

### 启动方式

```bash
# WebSocket + HTTP 同时运行（默认）
python VoxCPM_tts_Server.py --mode both

# 仅 HTTP API
python VoxCPM_tts_Server.py --mode http

# 仅 WebSocket
python VoxCPM_tts_Server.py --mode websocket
```

### 服务器参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `--host` | 0.0.0.0 | 绑定地址 |
| `--port` | 8765 | WebSocket 端口 |
| `--http-port` | 9000 | HTTP 端口 |
| `--mode` | both | 服务模式: websocket / http / both |
| `--output-dir` | output | 音频保存目录（需配合 --save-enabled） |
| `--save-enabled` | false | 是否保存音频到文件 |
| `--voice-root` | voice_clone | 音色资源根目录 |
| `--max-instances` | auto | 最大模型实例数（默认自动检测 GPU 容量） |
| `--no-auto-detect` | false | 禁用 GPU 自动检测 |
| `--reserved-gpu-mb` | 500 | 预留 GPU 显存（MB） |
| `--instance-memory-mb` | 8000 | 每实例预估显存（MB） |
| `--queue-size` | 100 | 请求队列大小 |
| `--request-timeout` | 30.0 | 请求超时（秒） |
| `--load-balance` | round_robin | 负载均衡: round_robin / least_used |
| `--sample-rate` | 48000 | 音频采样率: 16000 / 48000 |

---

## 一、HTTP REST API

基础地址: `http://host:9000`

交互式文档: `http://host:9000/docs`（Swagger UI）

### 1.1 健康检查

**GET** `/api/health`

响应：

```json
{
  "status": "healthy",
  "model_loaded": true,
  "voices_count": 2580,
  "model_instances": 2,
  "worker_stats": {
    "total_workers": 2,
    "idle_workers": 1,
    "busy_workers": 1,
    "pending_tasks": 0
  }
}
```

### 1.2 生成 TTS 音频（完整文件下载）

**POST** `/api/tts`

请求体：

```json
{
  "text": "你好，这是语音合成测试。",
  "voice": "湾湾小何",
  "voice_category": "趣味口音",
  "cfg_value": 2.0,
  "inference_timesteps": 10,
  "normalize": true,
  "retry_badcase": true,
  "retry_badcase_max_times": 3,
  "retry_badcase_ratio_threshold": 6.0
}
```

| 参数 | 类型 | 必填 | 说明 | 默认值 |
|------|------|------|------|--------|
| text | String | 是 | 要转换的文本 | - |
| voice | String | 否 | 音色名称（见音色列表） | 默认音色 |
| voice_category | String | 否 | 音色分类 | 自动匹配 |
| voice_design | String | 否 | 声音设计描述（自然语言） | - |
| reference_wav_path | String | 否 | 可控克隆参考音频路径 | - |
| cfg_value | Number | 否 | 引导强度，越高越符合提示 | 2.0 |
| inference_timesteps | Number | 否 | 推理步数 | 10 |
| normalize | Boolean | 否 | 文本规范化 | true |
| denoise | Boolean | 否 | 降噪（VoxCPM2 不需要） | false |
| retry_badcase | Boolean | 否 | 坏例重试 | true |
| retry_badcase_max_times | Number | 否 | 最大重试次数 | 3 |
| retry_badcase_ratio_threshold | Number | 否 | 坏例检测阈值 | 6.0 |

响应：WAV 音频文件下载（48kHz, 16-bit PCM, mono）

```
Content-Type: audio/wav
Content-Disposition: attachment; filename="tts_output_xxx.wav"
```

### 1.3 流式生成 TTS 音频（SSE）

**POST** `/api/tts/stream`

请求体同 `/api/tts`。

响应：Server-Sent Events (SSE) 流

```
Content-Type: text/event-stream
```

SSE 事件格式：

```
data: {"type": "start", "sample_rate": 16000}

data: {"type": "chunk", "index": 0, "data": "AQABAQAAAAIA..."}

data: {"type": "chunk", "index": 1, "data": "BQACAQAAAAIB..."}

data: {"type": "end", "total_chunks": 25}

data: {"type": "error", "message": "error description"}
```

| 事件 type | 说明 |
|-----------|------|
| start | 生成开始，包含采样率信息 |
| chunk | 音频块，data 为 base64 编码的 16-bit PCM (16kHz) |
| end | 生成结束，包含总块数 |
| error | 错误信息 |

### 1.4 预览音色

**POST** `/api/tts/preview`

请求体同 `/api/tts`，但文本限制 100 字符。用于快速试听音色效果。

### 1.5 获取音色列表

**GET** `/api/voices?category={category}`

| 参数 | 类型 | 说明 |
|------|------|------|
| category | String | 可选，按分类筛选 |

响应：

```json
{
  "doubao/趣味口音": {
    "湾湾小何": { "name": "湾湾小何", "category": "趣味口音", "source": "doubao", "exists": true },
    "东北老铁": { "name": "东北老铁", "category": "趣味口音", "source": "doubao", "exists": true }
  },
  "LibriTTS/train-clean-100": {
    "1034": { "name": "1034", "category": "train-clean-100", "source": "LibriTTS", "exists": true }
  }
}
```

### 1.6 获取音色详情

**GET** `/api/voices/{voice_name}?category={category}`

响应：

```json
{
  "name": "湾湾小何",
  "category": "趣味口音",
  "source": "doubao",
  "audio_path": "voice_clone/doubao/趣味口音/湾湾小何.mp3",
  "text_path": "voice_clone/doubao/趣味口音/湾湾小何.txt",
  "exists": true,
  "prompt_text": "今天天气真是太好了，阳光灿烂，心情超级棒。"
}
```

### 1.7 获取音色分类

**GET** `/api/categories`

响应：

```json
{
  "categories": ["doubao/趣味口音", "doubao/通用场景", "xunfei/通用场景", "aliyun/通用音色", "LibriTTS/train-clean-100", ...],
  "default_category": "趣味口音",
  "default_voice": "湾湾小何"
}
```

### 1.8 Worker Pool 统计

**GET** `/api/workers/stats`

响应：

```json
{
  "total_workers": 2,
  "idle_workers": 1,
  "busy_workers": 1,
  "pending_tasks": 0,
  "max_queue_size": 100
}
```

---

## 二、WebSocket 协议

地址: `ws://host:8765`

### 消息格式

所有消息遵循统一的 JSON 格式：

```json
{
  "type": "消息类型",
  "request_id": "请求ID（UUID）",
  "timestamp": 1678886400000,
  "data": { ... }
}
```

### 2.1 创建 TTS 请求

**tts.create** （客户端 → 服务端）

```json
{
  "type": "tts.create",
  "request_id": "req-123",
  "timestamp": 1678886400000,
  "data": {
    "session_id": "session-abc",
    "text": "你好，这是语音合成测试。",
    "voice": "湾湾小何",
    "voice_category": "趣味口音",
    "cfg_value": 2.0,
    "inference_timesteps": 10,
    "normalize": true
  }
}
```

#### 参数说明

| 参数 | 类型 | 必填 | 说明 | 默认值 |
|------|------|------|------|--------|
| session_id | String | 否 | 会话标识 | 自动生成 UUID |
| text | String | 是 | 要转换的文本 | - |
| voice | String | 否 | 音色名称 | 默认音色 |
| voice_category | String | 否 | 音色分类 | 自动匹配 |
| voice_design | String | 否 | 声音设计描述，如 "一个温柔的年轻女性" | - |
| prompt_wav_path | String | 否 | 直接指定参考音频路径（覆盖 voice） | - |
| prompt_text | String | 否 | 参考音频对应文本 | 随音色预设 |
| reference_wav_path | String | 否 | 可控克隆参考音频路径 | - |
| cfg_value | Number | 否 | 引导强度 | 2.0 |
| inference_timesteps | Number | 否 | 推理步数 | 10 |
| normalize | Boolean | 否 | 文本规范化 | true |
| retry_badcase | Boolean | 否 | 坏例重试 | true |
| retry_badcase_max_times | Number | 否 | 最大重试次数 | 3 |
| retry_badcase_ratio_threshold | Number | 否 | 坏例检测阈值 | 6.0 |

#### 三种音色模式

| 模式 | 参数组合 | 说明 |
|------|----------|------|
| 预设音色 | `voice` | 从音色库选择，自动加载对应音频和文本 |
| 声音设计 | `voice_design` | 用自然语言描述音色，模型自动生成，无需参考音频 |
| 自定义克隆 | `prompt_wav_path` + `prompt_text` | 直接指定参考音频和文本进行语音克隆 |

### 2.2 生成开始通知

**tts.start** （服务端 → 客户端）

```json
{
  "type": "tts.start",
  "request_id": "req-123",
  "data": {
    "session_id": "session-abc",
    "sample_rate": 16000,
    "model_sample_rate": 48000
  }
}
```

### 2.3 音频块推送

**tts.chunk** （服务端 → 客户端）

```json
{
  "type": "tts.chunk",
  "request_id": "req-123",
  "data": {
    "chunk": "AQABAQAAAAIA...",
    "chunk_index": 0,
    "session_id": "session-abc"
  }
}
```

音频格式：Base64 编码的 16-bit PCM, 16kHz, 单声道。

### 2.4 生成结束通知

**tts.end** （服务端 → 客户端）

```json
{
  "type": "tts.end",
  "request_id": "req-123",
  "data": {
    "session_id": "session-abc",
    "audio_duration": 4.25,
    "chunks_sent": 25,
    "total_samples": 68000
  }
}
```

### 2.5 查询音色列表

**voice.list** （客户端 → 服务端）

```json
{
  "type": "voice.list",
  "request_id": "req-voices-001",
  "data": {
    "category": "趣味口音"
  }
}
```

`category` 可选，不传则返回全部。

**voice.list.response** （服务端 → 客户端）

```json
{
  "type": "voice.list.response",
  "request_id": "req-voices-001",
  "data": {
    "doubao/趣味口音": {
      "湾湾小何": { "name": "湾湾小何", "category": "趣味口音", "source": "doubao", "exists": true }
    }
  }
}
```

### 2.6 查询音色详情

**voice.info** （客户端 → 服务端）

```json
{
  "type": "voice.info",
  "request_id": "req-info-001",
  "data": {
    "voice": "湾湾小何",
    "category": "趣味口音"
  }
}
```

**voice.info.response** （服务端 → 客户端）

```json
{
  "type": "voice.info.response",
  "request_id": "req-info-001",
  "data": {
    "name": "湾湾小何",
    "category": "趣味口音",
    "source": "doubao",
    "audio_path": "voice_clone/doubao/趣味口音/湾湾小何.mp3",
    "text_path": "voice_clone/doubao/趣味口音/湾湾小何.txt",
    "exists": true,
    "prompt_text": "今天天气真是太好了，阳光灿烂，心情超级棒。..."
  }
}
```

### 2.7 更新会话参数

**session.update** （客户端 → 服务端）

```json
{
  "type": "session.update",
  "request_id": "req-update-456",
  "data": {
    "session_id": "session-abc",
    "voice": "new_voice"
  }
}
```

### 2.8 中断生成

**session.interrupt** （客户端 → 服务端）

```json
{
  "type": "session.interrupt",
  "request_id": "req-interrupt-789",
  "data": {
    "session_id": "session-abc"
  }
}
```

**session.interrupted** （服务端 → 客户端）

```json
{
  "type": "session.interrupted",
  "request_id": "req-interrupt-789",
  "data": {
    "session_id": "session-abc"
  }
}
```

### 2.9 错误响应

**error** （服务端 → 客户端）

```json
{
  "type": "error",
  "request_id": "req-123",
  "data": {
    "type": "invalid_request_error",
    "message": "Text parameter is required",
    "code": "missing_param"
  }
}
```

| 错误代码 | 说明 |
|----------|------|
| missing_param | 缺少必填参数 |
| invalid_json | JSON 格式错误 |
| unknown_type | 未知消息类型 |
| voice_not_found | 音色不存在 |
| session_not_found | 会话不存在 |
| default_voice_not_found | 默认音色未配置 |
| generation_failed | TTS 生成失败 |
| streaming_failed | 流式传输失败 |
| server_error | 服务端内部错误 |

### 交互流程

```
┌─────────┐                    ┌─────────┐
│ Client  │                    │ Server  │
└────┬────┘                    └────┬────┘
     │                              │
     │────── tts.create ────────────>│
     │                              │
     │<────── tts.start ────────────┤
     │                              │
     │<────── tts.chunk (idx:0) ────┤
     │<────── tts.chunk (idx:1) ────┤
     │         ...                  │
     │<────── tts.chunk (idx:n) ────┤
     │                              │
     │<────── tts.end ──────────────┤
     │                              │
     │──── session.interrupt ──────>│  (可选中断)
     │<──── session.interrupted ────┤
     │                              │
```

---

## 三、音色管理

### 音色源

| 音色源 | 说明 | 音色数 |
|--------|------|--------|
| doubao | 豆包音色（趣味口音、通用场景、角色扮演等） | - |
| xunfei | 讯飞音色（专业解说、多语种、角色配音等） | - |
| aliyun | 阿里云音色（儿童、客服、有声书、方言等） | - |
| LibriTTS | 开源英文语音数据集 | ~2456 |

### 音色目录结构

```
voice_clone/
  <source>/              例: doubao, LibriTTS
    <category>/          例: 趣味口音, train-clean-100
      <name>.mp3         参考音频
      <name>.txt         参考文本
```

### 使用方式

```json
// 使用预设音色
{ "voice": "湾湾小何" }

// 指定分类（同名音色跨分类时）
{ "voice": "1034", "voice_category": "train-clean-100" }

// 声音设计（自然语言描述，无需参考音频）
{ "voice_design": "一个温柔甜美的年轻女性声音" }

// 自定义克隆（直接指定音频文件）
{ "prompt_wav_path": "/path/to/ref.wav", "prompt_text": "参考音频对应的文本" }
```

---

## 四、代码示例

### Python — HTTP REST API

```python
import requests
import json
import base64
import numpy as np
import soundfile as sf

BASE_URL = "http://localhost:9000"

# 1. 健康检查
resp = requests.get(f"{BASE_URL}/api/health")
print(resp.json())

# 2. 获取音色列表
resp = requests.get(f"{BASE_URL}/api/voices")
print(json.dumps(resp.json(), indent=2, ensure_ascii=False))

# 3. 生成音频（下载 WAV）
resp = requests.post(f"{BASE_URL}/api/tts", json={
    "text": "你好，这是语音合成测试。",
    "voice": "湾湾小何"
})
with open("output.wav", "wb") as f:
    f.write(resp.content)
print("音频已保存到 output.wav")

# 4. 使用声音设计
resp = requests.post(f"{BASE_URL}/api/tts", json={
    "text": "Hello, this is a voice design test.",
    "voice_design": "A warm middle-aged male voice with a slight British accent"
})
with open("voice_design_output.wav", "wb") as f:
    f.write(resp.content)

# 5. 流式生成（SSE）
import sseclient

resp = requests.post(f"{BASE_URL}/api/tts/stream", json={
    "text": "这是流式语音合成测试。",
    "voice": "湾湾小何"
}, stream=True)
client = sseclient.SSEClient(resp)

audio_chunks = []
for event in client.events():
    data = json.loads(event.data)
    if data["type"] == "start":
        print(f"开始生成, 采样率: {data['sample_rate']}Hz")
    elif data["type"] == "chunk":
        chunk = base64.b64decode(data["data"])
        audio_chunks.append(chunk)
    elif data["type"] == "end":
        audio_data = b''.join(audio_chunks)
        audio_array = np.frombuffer(audio_data, dtype=np.int16)
        sf.write("stream_output.wav", audio_array, 16000)
        print(f"完成, 共 {data['total_chunks']} 个块")
    elif data["type"] == "error":
        print(f"错误: {data['message']}")
```

### Python — WebSocket

```python
import asyncio
import websockets
import json
import base64
import uuid
import time
import numpy as np
import soundfile as sf

async def tts_streaming(text: str, voice: str = None, uri: str = "ws://localhost:8765"):
    async with websockets.connect(uri) as websocket:
        request = {
            "type": "tts.create",
            "request_id": str(uuid.uuid4()),
            "timestamp": int(time.time() * 1000),
            "data": {
                "text": text,
                "voice": voice
            }
        }
        await websocket.send(json.dumps(request))

        audio_chunks = []
        async for message in websocket:
            data = json.loads(message)
            msg_type = data.get("type")

            if msg_type == "tts.start":
                print("开始生成音频...")
            elif msg_type == "tts.chunk":
                chunk = base64.b64decode(data["data"]["chunk"])
                audio_chunks.append(chunk)
            elif msg_type == "tts.end":
                audio_data = b''.join(audio_chunks)
                audio_array = np.frombuffer(audio_data, dtype=np.int16)
                sf.write("output.wav", audio_array, 16000)
                print(f"完成！时长 {data['data']['audio_duration']:.2f}s")
                break
            elif msg_type == "error":
                print(f"错误: {data['data']['message']}")
                break

# 运行
asyncio.run(tts_streaming("你好，这是一个测试。", voice="湾湾小何"))
```

### JavaScript — HTTP Fetch

```javascript
// 生成音频并播放
async function ttsAndPlay(text, voice = null) {
    const body = { text };
    if (voice) body.voice = voice;

    const resp = await fetch('http://localhost:9000/api/tts', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(body)
    });

    const blob = await resp.blob();
    const url = URL.createObjectURL(blob);
    const audio = new Audio(url);
    audio.play();
}

// 流式生成（SSE）
async function ttsStream(text, voice = null) {
    const body = { text };
    if (voice) body.voice = voice;

    const resp = await fetch('http://localhost:9000/api/tts/stream', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(body)
    });

    const reader = resp.body.getReader();
    const decoder = new TextDecoder();

    while (true) {
        const { done, value } = await reader.read();
        if (done) break;
        const text = decoder.decode(value);
        // 解析 SSE data 行...
        const lines = text.split('\n').filter(l => l.startsWith('data: '));
        for (const line of lines) {
            const data = JSON.parse(line.slice(6));
            if (data.type === 'chunk') {
                // data.data 是 base64 编码的 16-bit PCM
                const binary = atob(data.data);
                // 转为 ArrayBuffer 用于 Web Audio API 播放...
            }
        }
    }
}
```

### JavaScript — WebSocket

```javascript
const ws = new WebSocket('ws://localhost:8765');
const audioChunks = [];

ws.onopen = () => {
    ws.send(JSON.stringify({
        type: 'tts.create',
        request_id: crypto.randomUUID(),
        timestamp: Date.now(),
        data: {
            text: '你好，这是浏览器端的测试。',
            voice: '湾湾小何'
        }
    }));
};

ws.onmessage = (event) => {
    const data = JSON.parse(event.data);

    switch (data.type) {
        case 'tts.start':
            console.log('开始生成, 采样率:', data.data.sample_rate);
            break;
        case 'tts.chunk':
            audioChunks.push(atob(data.data.chunk));
            break;
        case 'tts.end':
            console.log(`完成！时长: ${data.data.audio_duration}s`);
            // 使用 Web Audio API 播放 audioChunks
            break;
        case 'error':
            console.error('错误:', data.data.message);
            break;
    }
};

// 中断生成
function interrupt(sessionId) {
    ws.send(JSON.stringify({
        type: 'session.interrupt',
        request_id: crypto.randomUUID(),
        data: { session_id: sessionId }
    }));
}

// 查询音色列表
function listVoices() {
    ws.send(JSON.stringify({
        type: 'voice.list',
        request_id: crypto.randomUUID(),
        data: {}
    }));
}
```

### cURL 示例

```bash
# 健康检查
curl http://localhost:9000/api/health

# 获取音色列表
curl http://localhost:9000/api/voices

# 获取音色详情
curl "http://localhost:9000/api/voices/湾湾小何"

# 生成音频并保存
curl -X POST http://localhost:9000/api/tts \
  -H "Content-Type: application/json" \
  -d '{"text": "你好，这是语音合成测试。", "voice": "湾湾小何"}' \
  -o output.wav

# 使用声音设计生成
curl -X POST http://localhost:9000/api/tts \
  -H "Content-Type: application/json" \
  -d '{"text": "Hello world", "voice_design": "A warm male voice"}' \
  -o output.wav
```

---

## 五、故障排查

| 问题 | 排查方法 |
|------|----------|
| 连接失败 | 检查服务是否启动、端口是否开放、防火墙设置 |
| 音频断断续续 | 增加客户端缓冲区、降低 inference_timesteps |
| 语音质量差 | 提高 cfg_value (1.5-3.0)、增加 inference_timesteps (10-20) |
| 生成速度慢 | 降低 inference_timesteps、关闭 retry_badcase、确认 GPU 可用 |
| 音色不存在 | 检查 `GET /api/voices` 返回的音色列表，确认名称拼写 |
| Worker 忙碌 | 检查 `GET /api/workers/stats`，增加 --max-instances |
| GPU 显存不足 | 减小 --max-instances、增大 --reserved-gpu-mb |

