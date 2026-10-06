# H264 实时转码流媒体服务器

ARCS-MINI 在线视频播放器的 Ubuntu 侧配套服务器。把一个目录变成"即拷即播"
的流媒体源：任何分辨率/编码的 mp4 等文件拷进媒体目录，通过带参数的 HTTP
接口实时转码为设备友好的 H264 Baseline + AAC MPEG-TS 流，设备端
`video_player` 拉流播放。全程不落任何中间存储。

## 启动

```bash
python3 tools/h264-stream/h264_stream_server.py --media <目录> [--port 8000]
```

依赖：`ffmpeg`（含 libx264）。无需 Python 第三方包。

## 接口

| 端点 | 说明 |
| --- | --- |
| `GET /list` | 媒体目录文件清单（JSON） |
| `GET /play/<文件名>?<参数>` | 实时转码推流（默认 240x240@10fps + AAC 16k 单声道） |
| `GET /live.ts` | 测试源实时流（testsrc2 + 正弦音） |
| `GET /vod.ts` | 测试源整段全速下发（测下载极限） |

`/play` 参数（全部可选，均有安全钳位）：

| 参数 | 默认 | 说明 |
| --- | --- | --- |
| `w` / `h` | 240 / 240 | 目标分辨率（16..640，保持宽高比缩放 + 黑边填充） |
| `fps` | 10 | 目标帧率（5..30） |
| `vb` | 300k | 视频码率 |
| `ar` / `ac` / `ab` | 16000 / 1 / 32k | 音频采样率 / 声道（2 会自动混成单声道由设备处理） / 码率 |
| `loop` | 1 | 1=循环播放 |
| `speed` | 1 | 1=按目标帧率实时推流；0=尽快推（测极限） |

示例：

```bash
curl http://<ip>:8000/list
ffprobe http://<ip>:8000/play/movie.mp4?w=320&h=240&fps=15&vb=500k
```

## 设备侧

固件启用 `CONFIG_VIDEO_PLAYER` 后，adb shell：

```
vp play http://<ip>:8000/play/movie.mp4
vp play "http://<ip>:8000/play/movie.mp4?w=240&h=240&fps=15&vb=400k"
vp status     # 帧数/解码耗时/重连次数等
vp stop
```

## 转码模板

设备软解友好参数（编辑 `ENC_TPL` 可调）：

- H264 Constrained Baseline + CAVLC + zerolatency，无 B 帧、`ref=1`
- GOP = 2×fps（IDR 自愈断点间隔短）
- 音频 AAC-LC

> 传输为 close-delimited HTTP（无 Content-Length，断开即流结束），
> 对 `lisa_http` 等嵌入式客户端最稳，客户端主动断开 = 对齐直播沿。
