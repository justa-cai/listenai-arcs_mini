# ARCS-MINI 在线视频播放器（H264/master）

基于 master（mini-v3.0.2-m3）实现的在线视频播放器分支：设备通过网络拉取
Ubuntu 流媒体服务器实时转码的 **MJPEG + AAC** 流，JPEG 硬件解码直出
RGB565 上屏，AAC 软件解码经扬声器播出，全程不落任何本地存储。

原产品说明见 [README-original.MD](README-original.MD)。

## 架构

```
Ubuntu (tools/h264-stream)                     ARCS-MINI (CP 核)
┌────────────────────────────┐                 ┌──────────────────────────┐
│ 媒体目录(任意分辨率 mp4 等) │                 │ vp_net  HTTP 拉流任务     │
│   │  ffmpeg x2 (-re)       │   HTTP         │   └→ PSRAM 环形缓冲 256KB │
│   ├─ mjpeg (可调 q/fps/wh) │ ───────────────▶│ vp_play 解码任务          │
│   └─ aac adts              │  TLV 帧流       │   ├─ TLV 解析             │
│ TLV: [A5][V/A][len][pts]   │                 │   ├─ JPEG 硬解→RGB565    │
└────────────────────────────┘                 │   │   └→ 双缓冲→LVGL 页  │
                                               │   └─ AAC→PCM→app_player │
                                               │ shell: vp play/stop/status│
                                               └──────────────────────────┘
```

## 快速开始

### 1. 启动流媒体服务器（Ubuntu）

```bash
python3 tools/h264-stream/h264_stream_server.py --media <视频目录>
# 依赖 ffmpeg（含 libx264/aac），无需 Python 第三方包
```

把任意分辨率/编码的 mp4 等文件拷进媒体目录即点即播。接口：

| 端点 | 说明 |
|---|---|
| `GET /list` | 媒体目录清单（JSON） |
| `GET /mjpeg/<文件>?w=240&h=240&fps=10&q=4` | MJPEG+AAC TLV 流（**设备用**） |
| `GET /play/<文件>?...` | H264 TS 流（保留，软解路线已弃用） |

参数全部可选：`q` 为 JPEG 质量 2..31（小=好，默认 4≈41dB 近无损），
`ar/ac/ab` 音频采样率/声道/码率，`loop` 循环，`speed` 实时/全速。

### 2. 构建/烧录固件

```bash
./auto.sh -b          # 构建并烧录 app（详见脚本头注释）
```

### 3. 播放（adb shell）

```
vp play http://<服务器IP>:8000/mjpeg/movie.mp4
vp play "http://<服务器IP>:8000/mjpeg/movie.mp4?fps=15&q=2"   # 高帧率高画质
vp status             # 帧数/解码耗时/音频/重连/重试 统计
vp stop
```

## 实测性能（240×240@10fps，RV32 300MHz）

| 环节 | 耗时 |
|---|---|
| JPEG 硬件解码（含 RGB565 pixel build） | ~16-20 ms/帧 |
| 帧预算（10fps） | 100 ms，余量 5 倍 |
| 码率（q=4） | ~93 KB/s |

曾评估 H264 软解（自研纯 C 解码器，见 `arcs-sdk/components/h264dec/`，
与 ffmpeg 逐像素一致），实测 62ms/帧贴死预算，最终切换为服务端转 MJPEG +
设备 JPEG 硬解路线，h264dec 组件保留备用。

## 关键踩坑记录（设备侧）

- **GPDMA 通道冲突**：JPEG 硬解默认 ch2/3 与音频 DAC(ch2)/双麦采集(ch1/3)
  相撞；解码通道改 ch0/ch5（prj.conf 配置），且 lisa_jpeg 失败路径不得
  全局 `GPDMA_Uninitialize`（会拆掉音频通道）。
- **音频流首写水量**：lisa_player 需 ≥6 个解码块才 prepared，首写 1 帧会
  互相等待死锁；攒 8 帧 preroll 一次性启动。
- **audiomgr 通道终身制**：无注销 API，player 实例必须终身复用，destroy
  后同名重注册必失败（焦点永远拿不到）。
- **焦点配置生命周期**：`focus_configs[]` 必须 static（app_player 只存
  指针，懒创建的播放器运行期会再读）。
- **直播沿重连**：环形缓冲高水位断线重连时必须清空积压并复位解复用
  状态，否则旧数据立刻再次触顶形成重连风暴。

## 代码地图

| 路径 | 说明 |
|---|---|
| `apps/arcs-mini/video/video_player.c` | 播放服务：双任务拉流/解码、TLV 解析、音频 preroll、焦点处理 |
| `arcs-sdk/components/h264dec/` | H264 Baseline 纯 C 软件解码器（保留备用） |
| `apps-ui/apps/llm/{views,presenters,models}/video_player_*` | LVGL 视频页 |
| `tools/h264-stream/` | 流媒体服务器 + README |
| `src/category/comm/voice_player_comm.c` | 音频焦点通道配置（video: priority 5） |

本分支同时**整机禁用了 SD 卡功能**（当前硬件无卡槽）：service/mcp/shell/
底层驱动全链路 Kconfig 门控（`CONFIG_MIDDLEWARE_SD_MUSIC`），USB MSC 回退
挂出 flash 盘。
