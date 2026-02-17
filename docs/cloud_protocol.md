# 云端通信协议文档

## 目录

1. [WebSocket 连接协议](#1-websocket-连接协议)
2. [AIUI 消息协议](#2-aiui-消息协议)
3. [MCP 协议](#3-mcp-协议)
4. [HTTP API 协议](#4-http-api-协议)
5. [错误码](#5-错误码)

---

## 1. WebSocket 连接协议

### 1.1 连接 URL

```
ws://{host}/v1/interaction?param={base64_params}
```

#### 1.1.1 主机地址

| 环境 | 主机地址 |
|------|---------|
| 生产环境 (mode=0) | `ws://api.listenai.com` |
| 测试环境 (mode=1) | `ws://staging-api.listenai.com` |
| 研发环境 (mode=2) | `ws://integration-api.listenai.com` |

#### 1.1.2 连接参数 (param)

参数为 JSON 对象的 Base64 编码字符串：

```json
{
  "scene": "main",
  "mcp": true,
  "tool_protocol_version": "v2",
  "type": "fullduplex",  // 全双工模式
  "firmware_info": {
    "type": "arcs-mini",
    "version": "1.7.0"
  }
}
```

**参数说明:**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `scene` | string | 是 | 场景名称，固定为 "main" |
| `mcp` | boolean | 是 | 是否启用 MCP 协议 |
| `tool_protocol_version` | string | 否 | MCP 协议版本，默认 "v2" |
| `type` | string | 否 | 交互类型："fullduplex"(全双工) 或 "oneshot"(单次) |
| `firmware_info.type` | string | 否 | 固件类型，固定 "arcs-mini" |
| `firmware_info.version` | string | 否 | 固件版本 |

**Base64 编码示例:**

```c
// 生成参数 JSON
cJSON *params = cJSON_CreateObject();
cJSON_AddStringToObject(params, "scene", "main");
cJSON_AddBoolToObject(params, "mcp", true);
cJSON_AddStringToObject(params, "type", "fullduplex");

// 添加固件信息
cJSON *firmware_info = cJSON_CreateObject();
cJSON_AddStringToObject(firmware_info, "type", "arcs-mini");
cJSON_AddStringToObject(firmware_info, "version", "1.7.0");
cJSON_AddItemToObject(params, "firmware_info", firmware_info);

// Base64 编码
char *params_json = cJSON_PrintUnformatted(params);
char *params_base64 = aiui_base64_encode(params_json);

// 构建完整 URL
char *ws_path = sprintf("/v1/interaction?param=%s", params_base64);
```

---

### 1.2 WebSocket 连接事件

| 事件 | 说明 | 回调函数 |
|------|------|---------|
| `LISA_WS_ON_CONNECTED` | 连接成功 | `aiui_ws_connected_cb()` |
| `LISA_WS_ON_DISCONNECTED` | 连接断开 | `aiui_ws_disconnect_cb()` |
| `LISA_WS_ON_MESSAGE` | 收到消息 | `aiui_ws_onmessage_cb()` |

---

## 2. AIUI 消息协议

### 2.1 消息格式

所有 WebSocket 消息均为 JSON 格式：

```json
{
  "action": "string",
  "tag": "string",
  "fid": "string",
  "cid": "string",
  "rid": "string",
  "data": { ... }
}
```

**通用字段说明:**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `action` | string | 是 | 消息动作类型 |
| `tag` | string | 否 | 会话标识符 (session ID) |
| `fid` | string | 否 | 帧标识符 (frame ID)，全双工模式下使用 |
| `cid` | string | 否 | 内容标识符 (content ID) |
| `rid` | string | 否 | 请求标识符 (request ID) |
| `data` | object | 否 | 消息数据内容 |

---

### 2.2 客户端 -> 云端 消息

#### 2.2.1 启动交互 (action="started")

```json
{
  "action": "started"
}
```

#### 2.2.2 音频数据

通过 WebSocket 二进制帧发送，格式为原始音频流。

#### 2.2.3 文本数据

直接发送文本字符串。

---

### 2.3 云端 -> 客户端 消息

#### 2.3.1 交互启动确认 (action="started")

```json
{
  "action": "started",
  "fid": "frame_uuid",
  "tag": "session_uuid"
}
```

#### 2.3.2 交互结果 (action="result")

```json
{
  "action": "result",
  "tag": "session_uuid",
  "fid": "frame_uuid",
  "cid": "content_uuid",
  "data": {
    "sub": "string",
    "text": "string",
    "is_last": boolean,
    "intent": { ... },
    "theme": { ... },
    "mp_guide": { ... },
    "sub": "string",
    "nlp_origin": "string",
    "content": "string"
  }
}
```

**data.sub 字段类型:**

| 值 | 说明 |
|----|------|
| `"iat"` | 语音识别结果 |
| `"nlp"` | 自然语言处理结果 |
| `"tts"` | 语音合成结果 |
| `"vad"` | 语音活动检测结果 |

##### 2.3.2.1 语音识别结果 (sub="iat")

```json
{
  "action": "result",
  "data": {
    "sub": "iat",
    "text": "识别的文本",
    "result_id": 1
  }
}
```

##### 2.3.2.2 NLP 结果 (sub="nlp")

```json
{
  "action": "result",
  "data": {
    "sub": "nlp",
    "intent": {
      "rc": 0,
      "service": "musicX",
      "semantic": [
        {
          "intent": "PAUSE",
          "slots": [
            {
              "name": "action",
              "value": "pause"
            }
          ]
        }
      ]
    }
  }
}
```

**service 类型:**

| service | 说明 |
|---------|------|
| `musicX` | 音乐控制 |
| `weather` | 天气查询 |
| `scheduleX` | 闹钟管理 |

**intent 类型:**

| intent | 说明 |
|--------|------|
| `PAUSE` | 暂停 |
| `REPLAY` | 重播 |
| `RESUME_PLAY` | 恢复播放 |
| `CHOOSE_NEXT` | 下一首 |
| `CHOOSE_PREVIOUS` | 上一首 |
| `VOLUME_PLUS` | 音量增大 |
| `VOLUME_MINUS` | 音量减小 |
| `MUTE` | 静音 |
| `UNMUTE` | 取消静音 |
| `VOLUME_MAX` | 最大音量 |
| `VOLUME_MIN` | 最小音量 |
| `VOLUME_MID` | 中等音量 |
| `VOLUME_QUERY` | 查询音量 |
| `VOLUME_SET` | 设置音量 |

##### 2.3.2.3 音乐列表 (service="musicX")

```json
{
  "action": "result",
  "data": {
    "sub": "nlp",
    "intent": {
      "rc": 0,
      "service": "musicX",
      "data": {
        "result": [
          {
            "uni_url": "音频URL",
            "allRate": "比特率",
            "itemid": "歌曲ID",
            "name": "歌曲名称"
          }
        ]
      }
    }
  }
}
```

##### 2.3.2.4 TTS 结果 (sub="tts")

```json
{
  "action": "result",
  "data": {
    "sub": "tts",
    "is_last": true,
    "content": "Base64编码的音频URL"
  }
}
```

##### 2.3.2.5 表情更新 (nlp_origin="emoji")

```json
{
  "action": "result",
  "data": {
    "nlp_origin": "emoji",
    "data": {
      "emo_id": "表情ID"
    }
  }
}
```

##### 2.3.2.6 文生图结果 (nlp_origin="image_generation")

```json
{
  "action": "result",
  "data": {
    "nlp_origin": "image_generation",
    "data": {
      "result": [
        {
          "url": "图片URL"
        }
      ]
    }
  }
}
```

**错误情况:**

```json
{
  "action": "result",
  "data": {
    "nlp_origin": "image_generation",
    "data": {
      "result": [
        {
          "error": {
            "code": "OutOfLimit",
            "message": "超出限制"
          }
        }
      ]
    }
  }
}
```

##### 2.3.2.7 待机引导文本 (theme.frontend.banner)

```json
{
  "action": "result",
  "data": {
    "theme": {
      "frontend": {
        "banner": {
          "resources": [
            {
              "text": "引导文本",
              "url": "资源URL"
            }
          ]
        }
      }
    }
  }
}
```

##### 2.3.2.8 设备配置 (mp_guide.role_config)

```json
{
  "action": "result",
  "data": {
    "mp_guide": {
      "role_config": {
        "角色配置字段": "值"
      }
    }
  }
}
```

#### 2.3.3 交互完成 (action="finish")

```json
{
  "action": "finish",
  "rid": "request_id"
}
```

#### 2.3.4 交互错误 (action="error")

```json
{
  "action": "error",
  "code": "401",
  "message": "Token invalid"
}
```

#### 2.3.5 VAD 事件 (action="vad")

```json
{
  "action": "vad"
}
```

#### 2.3.6 连接确认 (action="connected")

```json
{
  "action": "connected"
}
```

---

## 3. MCP 协议

### 3.1 MCP 消息格式

所有 MCP 消息通过 AIUI 消息的 `action="mcp"` 字段传递：

```json
{
  "action": "mcp",
  "data": {
    "id": "request_id",
    "method": "string",
    "params": { ... }
  }
}
```

---

### 3.2 客户端 -> 云端 MCP 消息

#### 3.2.1 初始化请求 (method="initialize")

**请求:**

```json
{
  "action": "mcp",
  "data": {
    "id": "init_request_id",
    "method": "initialize",
    "params": {
      "protocolVersion": "2024-11-05",
      "capabilities": {
        "roots": {
          "listChanged": true
        },
        "sampling": {}
      },
      "clientInfo": {
        "name": "arcs-mini",
        "version": "1.7.0"
      },
      "capabilities": {
        "vision": {
          "url": "vision_api_url",
          "token": "vision_api_token"
        }
      }
    }
  }
}
```

**响应:**

```json
{
  "id": "init_request_id",
  "action": "mcp",
  "method": "initialize",
  "result": {
    "protocolVersion": "2024-11-05",
    "capabilities": {
      "tools": {
        "listChanged": true
      }
    },
    "serverInfo": {
      "name": "arcs-mini-mcp-server",
      "version": "1.0.0"
    },
    "instructions": "ARCS Mini MCP 服务器，提供设备控制、音乐播放、图片识别等功能"
  }
}
```

---

#### 3.2.2 工具列表请求 (method="tools/list")

**请求:**

```json
{
  "action": "mcp",
  "data": {
    "id": "list_request_id",
    "method": "tools/list",
    "params": {}
  }
}
```

**响应:**

```json
{
  "id": "list_request_id",
  "action": "mcp",
  "method": "tools/list",
  "result": {
    "tools": [
      {
        "name": "ls.built_in.exit",
        "description": "退出当前技能",
        "inputSchema": {
          "type": "object",
          "properties": {},
          "required": []
        }
      },
      {
        "name": "ls.built_in.play_control",
        "description": "播放控制",
        "inputSchema": {
          "type": "object",
          "properties": {
            "action": {
              "type": "string",
              "enum": ["play", "pause", "next", "prev"],
              "description": "播放动作"
            }
          },
          "required": ["action"]
        }
      }
    ],
    "nextCursor": null
  }
}
```

---

#### 3.2.3 工具调用 (method="tools/call")

**请求:**

```json
{
  "action": "mcp",
  "data": {
    "id": "call_request_id",
    "method": "tools/call",
    "params": {
      "name": "ls.built_in.volume_control",
      "arguments": {
        "volume": 80
      }
    }
  }
}
```

**成功响应:**

```json
{
  "id": "call_request_id",
  "action": "mcp",
  "method": "tools/call",
  "result": {
    "content": [
      {
        "type": "text",
        "text": "音量已设置为 80"
      }
    ]
  }
}
```

**错误响应:**

```json
{
  "id": "call_request_id",
  "action": "mcp",
  "method": "tools/call",
  "error": {
    "code": -1,
    "message": "Invalid parameter"
  }
}
```

---

### 3.3 已注册的 MCP 工具

| 工具名称 | 描述 | 参数 |
|---------|------|------|
| `ls.built_in.exit` | 退出当前技能 | 无 |
| `ls.built_in.music_random` | 随机播放音乐 | 无 |
| `ls.built_in.play_control` | 播放控制 | `action`: string (play/pause/next/prev) |
| `ls.built_in.photo_recognition` | 拍照识别 | 无 |
| `ls.built_in.alarm_control` | 闹钟控制 | `action`, `time` |
| `ls.built_in.volume_control` | 音量控制 | `volume`: int (0-100) |
| `ls.built_in.brightness_control` | 亮度控制 | `brightness`: int (0-100) |
| `ls.built_in.led_control` | LED 控制 | `state`: boolean |
| `ls.built_in.show_qrcode` | 显示二维码 | 无 |
| `ls.built_in.show_image` | 显示图片 | `url`: string |
| `ls.built_in.kuwo_music` | 酷我音乐 | `action`, `keyword` |
| `ls.built_in.emotion_control` | 表情控制 | `emoji_id`: string |
| `ls.built_in.switch_full_duplex` | 切换全双工 | 无 |
| `ls.built_in.version_info` | 版本信息 | 无 |
| `ls.built_in.audio_url` | 音频 URL | `url`: string |
| `ls.built_in.local_music` | 本地音乐 | 无 |
| `ls.built_in.text2img_kolors` | 文本生成图片 | `prompt`: string |

---

## 4. HTTP API 协议

### 4.1 获取 Token

#### 4.1.1 请求

**URL:**

```
POST http://{host}/v1/auth/tokens
```

**Headers:**

```
Content-Type: application/json
```

**Body:**

```json
{
  "productId": "product_id_string",
  "deviceId": "device_id_hex",
  "curtime": 1234567890,
  "checksum": "md5_hash"
}
```

**checksum 计算方法:**

```
checksum = MD5(secret_id + device_id + curtime)
```

#### 4.1.2 响应

**成功:**

```json
{
  "token": "access_token_string"
}
```

**失败:**

```json
{
  "error": "error_message"
}
```

---

### 4.2 酷我音乐激活

**URL:**

```
POST http://{host}/v1/kuwo/active
```

---

### 4.3 酷我音乐追踪链接

**URL:**

```
POST http://{host}/v1/kuwo/tranklink
```

---

## 5. 错误码

### 5.1 WebSocket 错误

| 错误码 | 说明 | 处理方式 |
|--------|------|---------|
| `401` | Token 无效 | 更新 Token 并重连 |
| `403` | 权限不足 | 检查 product_id 和 secret_id |
| `500` | 服务器错误 | 等待后重连 |

### 5.2 MCP 错误码

| 错误码 | 说明 |
|--------|------|
| `0` | 成功 |
| `1` | 执行错误 |
| `2` | 参数无效 |
| `3` | 工具未找到 |
| `4` | 执行超时 |
| `5` | 系统忙碌 |

### 5.3 AIUI 错误码 (rc)

| rc | 说明 |
|----|------|
| `0` | 成功 |
| 非 0 | 失败 |

---

## 6. 消息时序图

### 6.1 语音交互流程

```
客户端                          云端
   |                              |
   |------ WebSocket Connect ----->|
   |<----- Connected --------------|
   |                              |
   |------ Started -------------->|
   |<----- Started (fid,tag) -----|
   |                              |
   |====== 音频流 ===============>|
   |                              |
   |<----- Result (iat) ----------|
   |  (识别文本)                   |
   |                              |
   |<----- Result (nlp) ----------|
   |  (意图识别)                   |
   |                              |
   |<----- Result (tts) ----------|
   |  (TTS 音频)                   |
   |                              |
   |------ Finish ---------------->|
   |<----- Finish (rid) ----------|
   |                              |
```

### 6.2 MCP 工具调用流程

```
客户端                          云端
   |                              |
   |------ Initialize (action=mcp)|>|
   |<----- Initialize Response ----|
   |                              |
   |------ Tools/list ----------->|
   |<----- Tools List Response ---|
   |                              |
   |------ Text Input ----------->|
   |                              |
   |<----- Tool Call (action=mcp) |
   |      (tools/call)             |
   |                              |
   |------ Tool Response --------->|
   |<----- Result (tts) ----------|
   |                              |
```

---

## 7. 配置和存储

### 7.1 KV 存储键

| 键名 | 说明 | 示例值 |
|------|------|--------|
| `AIUI_TOKEN_KEY` | Token | `"eyJhbGc..."` |
| `AIUI_PID_KEY` | Product ID | `"product_id"` |
| `AIUI_SID_KEY` | Secret ID | `"secret_id"` |
| `INTERACTIVE_MODE_KEY` | 交互模式 | `0` (单次) 或 `1` (全双工) |
| `STAGING_MODE_KEY` | 设备环境模式 | `0` (生产), `1` (测试), `2` (研发) |
| `USER_DEVICE_ID` | 设备 ID | `"00112233..."` |

---

## 8. 参考实现

### 8.1 WebSocket 连接示例

```c
// 构建连接参数
cJSON *params = cJSON_CreateObject();
cJSON_AddStringToObject(params, "scene", "main");
cJSON_AddBoolToObject(params, "mcp", true);
cJSON_AddStringToObject(params, "type", "fullduplex");

char *params_json = cJSON_PrintUnformatted(params);
char *params_base64 = aiui_base64_encode(params_json);

// 构建连接 URL
char ws_url[256];
snprintf(ws_url, sizeof(ws_url), 
         "ws://%s/v1/interaction?param=%s",
         AIUI_HOST, params_base64);

// 连接 WebSocket
lisa_ws_config_t config = {
    .url = ws_url,
    .on_connected = ws_connected_cb,
    .on_disconnected = ws_disconnected_cb,
    .on_message = ws_message_cb
};
lisa_ws_connect(&config);
```

### 8.2 Token 生成示例

```c
char *generate_token(const char *pid, const char *sid, const char *dev_id) {
    // 获取当前时间戳
    char curtime[12];
    sprintf(curtime, "%lld", get_current_timestamp());
    
    // 计算 MD5
    char *data = sprintf("%s%s%s", sid, dev_id, curtime);
    char checksum[33];
    md5(data, checksum);
    
    // 构建请求
    cJSON *req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "productId", pid);
    cJSON_AddStringToObject(req, "deviceId", dev_id);
    cJSON_AddNumberToObject(req, "curtime", atol(curtime));
    cJSON_AddStringToObject(req, "checksum", checksum);
    
    // 发送 HTTP 请求
    char *body = cJSON_PrintUnformatted(req);
    http_post("http://api.listenai.com/v1/auth/tokens", body);
    
    // 解析响应获取 token
    // ...
}
```

---

## 9. 更新日志

- 2026-02-14: 初始版本，整理 AIUI 和 MCP 协议
