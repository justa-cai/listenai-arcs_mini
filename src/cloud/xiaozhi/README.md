# 小智云端协议 (XiaoZhi Cloud)

## 概述

小智云端协议模块实现了与 xiaozhi 云端的 WebSocket 通信，支持完整的语音助手功能：
- ASR (语音识别)
- LLM (大语言模型对话)
- TTS (语音合成)
- IoT (设备控制)

## 文件结构

```
src/cloud/xiaozhi/
├── xz_tls.c/h          # mbedTLS 封装层
├── xz_websocket.c/h    # WebSocket 客户端
├── xz_message.c/h      # JSON 消息处理
├── xz_opus.c/h         # Opus 编码器封装
├── xz_audio.c/h        # 音频发送器
├── xz_client.c/h       # 主客户端逻辑
├── xz_cloud.c/h        # 云端 API (与 app_cloud/jk_cloud 对接)
├── xz_config.h         # 编译时配置
└── README.md           # 本文件
```

## 编译配置

### 1. 启用小智云端

在 `prj.conf` 中添加：

```ini
# 启用小智云端
CONFIG_XIAOZHI_CLOUD=y

# 或者在 CFLAGS 中定义
CONFIG_CFLAGS+=-DXIAOZHI_CLOUD
```

### 2. 修改 app_client.c

在 `src/app_client.c` 中添加小智云端支持：

```c
#ifdef XIAOZHI_CLOUD
#include "xz_cloud.h"
#endif

// 在 app_client_create 函数中
#ifdef XIAOZHI_CLOUD
    handle->cloud = xz_cloud_create(handle);
#endif

// 在 app_client_record 函数中
#ifdef XIAOZHI_CLOUD
    xz_cloud_audio(s_app_client->cloud, (const char*)s_record_rec_buf, LS_RECORD_ONE_CHNNEL_SIZE);
#endif
```

### 3. WiFi 事件处理

在 WiFi 连接/断开事件处理中添加：

```c
#ifdef XIAOZHI_CLOUD
    extern void xz_cloud_process_wifi_connected(xz_cloud_t);
    extern void xz_cloud_process_wifi_disconnected(xz_cloud_t);
#endif

// WiFi 连接时
#ifdef XIAOZHI_CLOUD
    xz_cloud_process_wifi_connected(client->cloud);
#endif

// WiFi 断开时
#ifdef XIAOZHI_CLOUD
    xz_cloud_process_wifi_disconnected(client->cloud);
#endif
```

## 运行时配置

### 设置服务器地址

```bash
# 通过 ADB 或串口设置 KV 配置
lisa_kv set xz.url "wss://api.tenclass.net/xiaozhi/v1/"
lisa_kv set xz.token "your_token_here"
lisa_kv set xz.device_id "device_unique_id"
lisa_kv set xz.client_id "client_identifier"
```

### 默认配置

| 配置项 | 默认值 | 说明 |
|--------|--------|------|
| xz.url | wss://api.tenclass.net/xiaozhi/v1/ | 服务器 URL |
| xz.token | default_token | 认证令牌 |
| xz.device_id | arcs_mini | 设备 ID |
| xz.client_id | default | 客户端 ID |

## 音频参数

| 参数 | 值 | 说明 |
|------|-----|------|
| 采样率 | 16000 Hz | 固定 |
| 声道数 | 1 (单声道) | 固定 |
| 帧时长 | 60 ms | 固定 |
| 比特率 | 24000 bps | Opus 编码 |
| 复杂度 | 0 | 最低 CPU 占用 |

## API 使用

### 唤醒语音助手

```c
xz_cloud_t cloud = xz_cloud_get_instance();
xz_cloud_wakeup(cloud);
```

### 发送文本消息

```c
xz_cloud_txt("你好");
```

### 检查连接状态

```c
if (xz_cloud_is_connected()) {
    // 已连接
}
```

## 日志输出

```
[xz_tls] TLS context created
[xz_ws] WebSocket created: wss://api.tenclass.net/xiaozhi/v1/
[xz_ws] Connecting to api.tenclass.net:443
[xz_ws] TLS connected
[xz_ws] WebSocket handshake completed
[xz_ws] WebSocket connected
[xz_msg] Created Hello message
[xz_client] Connected to xiaozhi cloud
[xz_cloud] Connected to xiaozhi cloud
```

## 调试

### 启用详细日志

```bash
# 在串口或 ADB shell 中
log set_tag_level xz_* 6
```

### 检查连接

```bash
# 检查 KV 配置
lisa_kv get xz.url
lisa_kv get xz.token

# 检查连接状态
# 需要在代码中添加 shell 命令或通过日志查看
```

## 故障排除

### 连接失败

1. 检查网络连接
2. 检查 token 是否正确
3. 检查服务器 URL 是否正确
4. 查看日志中的错误信息

### 音频无法发送

1. 检查 `xz_cloud_wakeup()` 是否被调用
2. 检查 `interacting` 状态
3. 查看音频数据流日志

### Opus 编码失败

1. 检查 `libopus` 静态库是否包含编码器
2. 运行 `nm libopus.a | grep opus_encode` 检查符号

## 性能优化

| 优化项 | 配置 | 说明 |
|--------|------|------|
| CPU 占用 | complexity=0 | 最低编码复杂度 |
| 帧长 | 60ms | 较长帧减少编码次数 |
| 内存 | PSRAM 分配 | 大缓冲区使用 PSRAM |

## 协议兼容性

与 xiaozhi Linux 版本完全兼容：
- WebSocket 消息格式相同
- Opus 编码参数相同
- API 行为一致
