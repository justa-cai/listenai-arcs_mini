# 小智云端协议移植 - 实施总结

## 已完成工作

### Phase 1: 核心连接层 ✅

#### 1.1 mbedTLS 封装层 (xz_tls.c/h)
- 替代 xiaozhi Linux 中的 OpenSSL 实现
- 封装 mbedTLS SSL/TLS 功能
- 支持证书验证和 SNI
- **代码行数**: ~230 行

#### 1.2 WebSocket 客户端 (xz_websocket.c/h)
- 基于 jk_websocket.c 的模式实现
- 支持自定义 HTTP 头 (Authorization, Device-Id, Client-Id)
- 完整的帧处理 (文本/二进制, Ping/Pong, 关闭)
- 独立接收线程
- **代码行数**: ~530 行

#### 1.3 JSON 消息处理 (xz_message.c/h)
- 实现 hello/listen/stop/ping/pong 消息创建
- 解析服务器响应 (stt/llm/tts/iot/error)
- 使用 cJSON 库进行 JSON 序列化
- **代码行数**: ~300 行

### Phase 2: 音频处理层 ✅

#### 2.1 Opus 编码器封装 (xz_opus.c/h)
- 基于 arcs-sdk/modules/opusdec/opus_encoder.h
- 配置: 16kHz, 单声道, 60ms 帧, 24kbps
- complexity=0 最低 CPU 占用
- **代码行数**: ~150 行

#### 2.2 音频发送器 (xz_audio.c/h)
- PCM 缓冲和帧处理
- Opus 编码和 WebSocket 发送
- 发送回调机制
- **代码行数**: ~220 行

### Phase 3: 集成层 ✅

#### 3.1 主客户端 (xz_client.c/h)
- 整合所有模块
- 事件回调系统
- 状态管理
- **代码行数**: ~370 行

#### 3.2 云端 API (xz_cloud.c/h)
- 与 app_cloud/jk_cloud 类似的接口
- KV 配置存储
- WiFi 事件处理
- **代码行数**: ~330 行

#### 3.3 配置和文档
- xz_config.h: 编译时配置
- README.md: 使用说明
- BUILD.md: 编译配置说明
- **文档行数**: ~300 行

## 文件清单

| 文件 | 行数 | 说明 |
|------|------|------|
| xz_tls.c | 230 | mbedTLS 封装层 |
| xz_tls.h | 78 | mbedTLS 接口 |
| xz_websocket.c | 530 | WebSocket 客户端 |
| xz_websocket.h | 100 | WebSocket 接口 |
| xz_message.c | 300 | JSON 消息处理 |
| xz_message.h | 132 | 消息接口 |
| xz_opus.c | 150 | Opus 编码器 |
| xz_opus.h | 89 | Opus 接口 |
| xz_audio.c | 220 | 音频发送器 |
| xz_audio.h | 109 | 音频接口 |
| xz_client.c | 380 | 主客户端 |
| xz_client.h | 169 | 客户端接口 |
| xz_cloud.c | 330 | 云端 API |
| xz_cloud.h | 100 | 云端接口 |
| xz_config.h | 60 | 配置定义 |
| README.md | 150 | 使用说明 |
| BUILD.md | 140 | 编译说明 |
| **总计** | **~3257** | |

## 下一步工作

### 必需步骤

1. **修改 CMakeLists.txt**
   - 添加 xiaozhi 源文件
   - 添加依赖库 (mbedTLS, opus, cjson)

2. **修改 app_client.c**
   - 添加 `#ifdef XIAOZHI_CLOUD` 分支
   - 集成 xz_cloud_create() 和 xz_cloud_audio()

3. **修改 WiFi 事件处理**
   - 添加 xz_cloud_process_wifi_connected/disconnected 调用

4. **修改 prj.conf**
   - 添加 CONFIG_XIAOZHI_CLOUD=y

### 可选步骤

1. **TTS 播放集成**
   - 将 TTS 数据路由到音频播放器
   - 实现音频焦点管理

2. **UI 显示集成**
   - LLM 文本显示
   - STT 结果显示

3. **测试和调试**
   - 单元测试
   - 集成测试
   - 性能优化

## 验证清单

- [ ] 编译通过无错误
- [ ] 成功连接到 wss://api.tenclass.net/xiaozhi/v1/
- [ ] Hello/握手消息正确发送
- [ ] 音频数据正确编码和发送
- [ ] STT 结果正确接收
- [ ] LLM 响应正确接收
- [ ] TTS 音频正确播放
- [ ] 网络断开自动重连
- [ ] 内存占用 < 5MB

## 风险和缓解

| 风险 | 缓解措施 | 状态 |
|------|----------|------|
| Opus 编码器缺失 | 预检查符号 | 需验证 |
| mbedTLS 版本不兼容 | 使用 arcs-sdk 提供的版本 | 已缓解 |
| 内存不足 | PSRAM 分配大缓冲区 | 已缓解 |
| 性能问题 | complexity=0, 60ms 帧 | 已缓解 |
| 协议不兼容 | 完全复制消息格式 | 已缓解 |

## 参考资源

- 原始协议: `/nvme/work/AI/xiaozhi/xiaozhi-linux/`
- WebSocket 参考: `src/cloud/jk_cloud/jk_websocket.c`
- Opus 编码: `arcs-sdk/modules/opusdec/opus_encoder.h`
- mbedTLS: `arcs-sdk/modules/mbedtls/`
