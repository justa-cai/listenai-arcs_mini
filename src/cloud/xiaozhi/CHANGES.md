# 小智云端启用 - 修改总结

## 已修改的文件

### 1. CMakeLists.txt (根目录)
**修改内容**: 添加 XIAOZHI_CLOUD 宏定义

```cmake
# Cloud provider selection
if(CONFIG_XIAOZHI_CLOUD)
    add_definitions(-DXIAOZHI_CLOUD)
    message(STATUS "Using XIAOZHI_CLOUD (Xiaozhi cloud service)")
elseif(CONFIG_MY_CLOUD)
    ...
endif()
```

### 2. src/cloud/CMakeLists.txt
**修改内容**: 添加小智云端源文件和包含目录

```cmake
# Xiaozhi cloud (Xiaozhi cloud service)
xiaozhi/xz_tls.c
xiaozhi/xz_websocket.c
xiaozhi/xz_message.c
xiaozhi/xz_opus.c
xiaozhi/xz_audio.c
xiaozhi/xz_client.c
xiaozhi/xz_cloud.c

# Include directories
${CMAKE_CURRENT_SOURCE_DIR}/xiaozhi
```

### 3. src/app_client.c
**修改内容**:
- 添加头文件包含: `#ifdef XIAOZHI_CLOUD #include "xz_cloud.h" #endif`
- app_client_create(): 添加 `xz_cloud_create(handle)`
- app_client_record(): 添加 `xz_cloud_audio(s_app_client->cloud, ...)`
- _ls_sntp_synced_callback(): 添加 `xz_cloud_process_wifi_connected(s_app_client->cloud)`
- _ls_wifi_status_cb(): 添加 `xz_cloud_process_wifi_disconnected(s_app_client->cloud)`

### 4. src/cloud/app_cloud.c
**修改内容**:
- 添加头文件包含: `#ifdef XIAOZHI_CLOUD #include "xz_cloud.h" #endif`
- app_cloud_txt(): 添加 `xz_cloud_txt(txt)` 调用
- app_cloud_tts(): 添加 `xz_cloud_tts(text)` 调用
- app_cloud_is_connected(): 添加 `xz_cloud_is_connected()` 调用
- app_chat_start(): 添加 `xz_cloud_wakeup(client->cloud)` 调用

### 5. prj.conf
**状态**: 已设置 `CONFIG_XIAOZHI_CLOUD=y`

## 配置说明

### 编译配置
```bash
# 确保 prj.conf 中设置了
CONFIG_XIAOZHI_CLOUD=y
```

### 运行时配置
```bash
# 通过串口或 ADB 设置 KV 配置
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

## 编译和部署

### 编译命令
```bash
# 清理并编译
./build.sh -C

# 或使用 menuconfig
./build.sh -t menuconfig
```

### 验证编译
```bash
# 检查符号
nm build/aiui | grep xz_

# 应该看到以下符号:
# xz_client_*
# xz_ws_*
# xz_msg_*
# xz_opus_*
# xz_audio_*
# xz_tls_*
# xz_cloud_*
```

### 刷写
```bash
adb shell recovery
adb push build/aiui.bin /RAW/NAND/600000
adb shell reboot hard
```

## 调试

### 启用详细日志
```bash
# 在串口或 ADB shell 中
log set_tag_level xz_* 6
log set_tag_level xz_cloud 6
```

### 检查连接状态
```bash
# 查看日志，应该看到:
[xz_tls] TLS context created
[xz_ws] WebSocket created: wss://api.tenclass.net/xiaozhi/v1/
[xz_ws] Connecting to api.tenclass.net:443
[xz_ws] TLS connected
[xz_ws] WebSocket handshake completed
[xz_ws] WebSocket connected
[xz_cloud] Connected to xiaozhi cloud
```

## 测试流程

1. **连接测试**: 设备上电后，连接 WiFi，查看是否自动连接到小智云端
2. **语音交互测试**: 按下唤醒按钮，查看是否正确发送音频和接收响应
3. **文本交互测试**: 通过 shell 发送文本消息测试

## 故障排除

### 问题 1: 编译错误 "undefined reference to xz_*"
**原因**: 源文件未添加到 CMakeLists.txt
**解决**: 检查 src/cloud/CMakeLists.txt 是否包含 xiaozhi 源文件

### 问题 2: 连接失败
**原因**: Token 错误或网络问题
**解决**:
1. 检查 KV 配置: `lisa_kv get xz.token`
2. 检查网络连接
3. 查看日志中的错误信息

### 问题 3: 音频无法发送
**原因**: xz_cloud_wakeup() 未被调用
**解决**: 检查 app_chat_start() 是否正确实现

### 问题 4: Opus 编码失败
**原因**: Opus 编码器符号缺失
**解决**: 运行 `nm build/aiui | grep opus_encode` 检查符号

## 下一步工作

1. **TTS 播放集成**: 将 TTS 音频数据路由到播放器
2. **UI 显示集成**: 显示 LLM 响应文本
3. **IoT 控制集成**: 处理 IoT 指令
4. **性能优化**: 监控 CPU 和内存使用

## 回滚到其他云端

要切换回其他云端:

```bash
# 使用 MY_CLOUD
# prj.conf: CONFIG_XIAOZHI_CLOUD=n
# prj.conf: CONFIG_MY_CLOUD=y

# 使用 LISTEN_CLOUD
# prj.conf: CONFIG_XIAOZHI_CLOUD=n
# prj.conf: CONFIG_LISTEN_CLOUD=y
```
