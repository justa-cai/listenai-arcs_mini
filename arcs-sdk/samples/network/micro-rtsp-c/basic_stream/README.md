# RTSP 服务器基础示例

## 功能说明

演示如何使用 micro-rtsp-c 库搭建基础的 RTSP 视频流服务器，通过 WiFi 网络传输预先准备的静态 JPEG 图片，支持标准 RTSP 客户端（如 VLC、ffplay）接入播放。

本示例实现了完整的 RTSP 服务器功能，包括 OPTIONS、DESCRIBE、SETUP、PLAY、TEARDOWN 命令处理，支持 TCP 和 UDP 两种传输模式，并自动处理 JPEG 量化表传输以保证图像质量。

## 硬件连接

本示例使用芯片内部 WiFi 外设，无需额外接线。

**串口输出：**
- 串口 TX: PA3
- 波特率：921600

## 示例内容

1. 初始化 WiFi 并连接到指定 AP
2. 通过 DHCP 获取 IP 地址
3. 在 8554 端口启动 RTSP 服务器
4. 等待客户端连接（支持 RTSP over TCP/UDP）
5. 循环发送两张静态 JPEG 图片（800x600），每秒 2 帧
6. 处理客户端的 RTSP 协议请求并响应

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 烧录

```{eval-rst}
.. include:: /sample_flash.rst
```

## 预期输出

**串口输出：**

```
[rtsp-test] RTSP Server Starting...
[rtsp-test] WiFi auto-connect started
[rtsp-test] Waiting for WiFi connection and IP address...
[rtsp-test] DHCP Success on VIF-0: IP=192.168.137.166
[rtsp-test] WiFi connected and IP obtained
[rtsp-test] Starting RTSP server on port 8554
+ UDP socket created successfully: sock=10 port=6970
+ UDP socket created successfully: sock=11 port=6971
+ init_udp_transport: SUCCESS - rtp_port=10 rtp_port=6970 rtcp_sock=11 rtcp_port=6971
[rtsp-test] client connected: 192.168.137.1

+ RTSP command: DESCRIBE
+ validate_stream_id: streamer(mjpeg/1) vs command(mjpeg/1) -> stream_id=0
+ RTSP command: SETUP
+ Video SETUP: client_port=54224-54225 server_port=6970-6971
+ RTSP command: PLAY
+ PLAY: enabled streaming (is_streaming=1, is_tcp=0, video_port=54224, audio_port=0)
```

**客户端播放：**

使用 ffplay 播放（UDP 模式）：
```bash
ffplay -rtsp_transport udp rtsp://192.168.32.236:8554/mjpeg/1 -fflags nobuffer -flags low_delayffplay -rtsp_transport udp rtsp://192.168.137.166:8554/mjpeg/1
```

使用 ffplay 播放（TCP 模式）：
```bash
ffplay -rtsp_transport tcp rtsp://192.168.32.236:8554/mjpeg/1 -fflags nobuffer -flags low_delay打开网络串流 -> rtsp://192.168.137.166:8554/mjpeg/1
```

预期看到两张图片交替显示，帧率约 2fps。

## 核心 API

| API | 说明 |
|-----|------|
| `rtsp_streamer_init()` | 初始化 RTSP 流媒体服务 |
| `rtsp_session_init()` | 初始化 RTSP 会话 |
| `rtsp_streamer_set_uri()` | 设置 RTSP 服务器 URI |
| `rtsp_streamer_add_session()` | 添加客户端会话 |
| `rtsp_streamer_start()` | 启动 RTSP 服务 |
| `rtsp_streamer_stream_frame()` | 发送视频帧数据 |
| `rtsp_session_start()` | 处理客户端 RTSP 命令 |

## 关键代码

**初始化 RTSP 服务器：**

```c
rtsp_streamer_t streamer;
rtsp_session_t session;

// 初始化流媒体服务，设置分辨率为 800x600
rtsp_streamer_init(&streamer, 800, 600);

// 初始化会话
rtsp_session_init(&session, client, &streamer);

rtsp_streamer_set_uri(&streamer, "0.0.0.0:8554", "mjpeg", "1");

// 添加会话到流媒体服务
rtsp_streamer_add_session(&streamer, &session);
```

**发送视频帧：**

```c
while (!session.is_stopped) {
    // 处理 RTSP 命令（非阻塞，1ms 超时）
    rtsp_streamer_start(&streamer, 1);
    
    if (session.is_streaming) {
        // 发送 JPEG 图片
        rtsp_streamer_stream_frame(&streamer, capture_buff, capture_buff_len, 100);
        
        // 帧间隔 500ms (2fps)
        lisa_thread_mdelay(500);
    }
}
```

## 配置说明

**WiFi 配置（main.c）：**

```c
#define TARGET_WIFI_SSID   "Your_SSID"  // WiFi SSID
#define TARGET_WIFI_PWD     "Your_Password"             // WiFi 密码
```

**RTSP 服务器配置：**

```c
// RTSP 监听端口（标准端口 8554）
serv_addr.sin_port = htons(8554);

// 视频分辨率（需与实际图片一致）
rtsp_streamer_init(&streamer, 800, 600);
```

**传输优化参数（streamer.c）：**

```c
#define MAX_FRAG_SIZE 950        // RTP 包片段大小（字节）
```

## 注意事项

1. **WiFi 配置**：需修改 `TARGET_WIFI_SSID` 和 `TARGET_WIFI_PWD` 为实际的 WiFi 信息
2. **图片分辨率**：示例中的静态图片为 800x600，需确保 `rtsp_streamer_init()` 的分辨率参数与之匹配
3. **量化表传输**：示例图片使用自定义量化表，已自动在第一个 RTP 包中传输，确保颜色准确
4. **UDP 丢包**：
   - 默认使用 UDP 传输，网络不稳定时可能丢包
   - 可在客户端使用 TCP 模式：`ffplay -rtsp_transport tcp rtsp://...`
   - 片段大小（950字节）和延迟（200μs）已优化以减少丢包
5. **防火墙**：确保 8554 端口（RTSP）和 6970-6971 端口（RTP/RTCP）未被防火墙阻止
6. **客户端兼容性**：
   - VLC：支持 TCP/UDP，推荐使用
   - ffplay：支持 TCP/UDP，可通过 `-rtsp_transport` 参数选择
   - Chrome 浏览器：不直接支持 RTSP，需使用插件或转码
7. **内存管理**：每个客户端连接会占用约 3KB 内存，同时支持多客户端时需注意内存使用

## 相关文档

- [micro-rtsp-c 库源码](../../../../modules/micro-rtsp-c/)
- [RTSP 协议规范 RFC 2326](https://www.rfc-editor.org/rfc/rfc2326)
- [RTP Payload Format for JPEG RFC 2435](https://www.rfc-editor.org/rfc/rfc2435)
- [WiFi Manager 使用文档](../../wifi/)
