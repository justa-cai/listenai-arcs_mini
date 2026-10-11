# ARCS-MINI SIP 视频通话（sip/master）

基于 H264/master 实现的**自研 SIP 协议栈**分支：局域网点对点音视频通话，
ARCS-MINI 设备与 Ubuntu 主机**共用同一套 C 代码**（core 纯 C 零 OS 依赖，
双端只换 OSAL 移植层），无任何第三方 SIP 库。

设备端完整 UI 拨号流程（四击入口/来电/接听/挂断/计时）+ 双向 G.711 音频 +
**双向硬件 JPEG 编解码视频** + 调试工具链（LCD 回传看屏 / SDL 窗口）。

原产品说明见 [README-original.MD](README-original.MD)；在线视频播放器见
`h264/master` 分支。

## 架构

```
Ubuntu (sip_peer)                                ARCS-MINI (sip_service)
┌────────────────────────────┐                  ┌───────────────────────────┐
│ sip_ua（同一份 core 代码）  │   UDP 5060 SIP   │ sip_sig  信令/UI 桥/铃声   │
│ peer_alsa  ALSA 全双工     │ ────────────────▶│ sip_aud   G.711 + 8k↔16k  │
│ peer_v4l2  V4L2→4:2:0 编码 │   UDP 5062 RTP   │   └ 64KB ring 收发解耦    │
│ peer_vidsink SDL 窗口(-W)  │   UDP 5064 RTP   │ sip_vid   Jpeg0 硬编(DMA2D)│
│ screen_sink LCD 回传接收    │   UDP 5070 调试  │ sip_vidrx Jpeg0 硬解→双缓冲│
└────────────────────────────┘                  │ sip_screen LCD 快照回传   │
                                                │ UI: 通话页/四击/按键拦截  │
                                                └───────────────────────────┘
```

## 实测性能（RV32 300MHz，双向并发）

| 环节 | 数值 |
|---|---|
| 上行视频（Jpeg0 硬编 240x320，DMA2D 路线） | **~200ms/帧 ≈ 3fps**（软编同幅 3.7s/帧） |
| 下行视频（Jpeg0 硬解 240x240@5fps） | 25~70ms/帧 |
| 双向音频（G.711 20ms/包） | 持续清晰，丢包个位数百分比 |
| Jpeg0 编/解码时分复用占空比 | ~50%（互斥锁 + 脏标志） |

## 快速开始

### 1. 主机侧构建

```bash
cmake -S src/middleware/sip/host -B tmp/sip-host
cmake --build tmp/sip-host
```

产物：`tmp/sip-host/sip_peer`（对端程序）、`screen_sink`（LCD 回传接收）。
依赖：libasound、libjpeg、SDL2（缺 SDL2 自动降级无窗口）。

### 2. 设备固件构建/烧录

```bash
./build.sh -S ./apps/arcs-mini -B build-h264 -DBOARD=arcs_mini
bash adb_download.sh -S res/arcs-mini -B build-h264 app
```

### 3. 通话

```bash
# 主机：守候 + USB 摄像头 + SDL 窗口显示设备上行画面
DISPLAY=:0 ./tmp/sip-host/sip_peer -w -d /dev/video0 -W

# 设备（adb shell，或设备上四击功能键进入通话页呼叫）
sip call <主机IP>
```

接通后：设备屏幕显示主机摄像头画面，主机 SDL 窗口显示设备摄像头画面，
双向语音通话；单击挂断。

### 调试工具

```bash
adb shell "sip screen <主机IP>"        # 设备 LCD 每秒回传到主机
./tmp/sip-host/screen_sink             # 主机接收 → tmp/sip_host/screen_NNN.jpg
```

lv_snapshot 抓的是 LVGL 渲染结果：回传图正常而实物异常 = flush/LCD 层
问题；回传也异常 = 数据链路问题——远程二分定位显示类问题，免拍照。

## 协议子集

- **SIP**（UDP 5060）：INVITE/ACK/BYE/CANCEL + 100/180/200/481/486/487/488，
  单对话状态机，T1=500ms 倍增重传，无 REGISTER（LAN 点对点直连）
- **SDP**：m=audio PCMU/PCMA + m=video PT26(JPEG)，answer 支持拒视频降级纯音频
- **RTP**：音频 20ms/160B；视频 RFC2435 子集，**Q=255 约定"载荷为完整 JFIF"**
  （双端同栈，不自研量化表重建），1400B 分片、位图判洞重组、对称 RTP
- **屏幕回传**：自定义 16B 头分片协议（magic/frame_id/w/h/total/offset，
  小端裸拷，无重传，丢片整帧作废）

## shell 命令（设备）

```
sip status        状态/时长/音视频统计/丢帧分项(frag/size/sampling/dec)
sip call <ip>     呼出          sip answer/reject/hangup/cancel
sip cfg [ip]      对端 KV 配置  sip ring   自测伪造来电
sip screen <ip>|off  LCD 回传   sip log <0-3>
```

## 关键踩坑记录（真机两晚联调）

- **JPEG 硬解不支持 4:4:4**：解出规则条纹却返回成功。ffmpeg 从 RGB 源、
  libjpeg JCS_RGB 输入默认都编 4:4:4——发送侧强制 4:2:0，设备端 SOF 采样
  白名单丢帧防御
- **硬编 DMA 通道**：GPDMA 六通道全占（解码 ch0/ch5、音频 ch1/2/3、DVP
  ch4），但 **DMA2D 只有 ch6~9 且被 LVGL blender 预订**——关掉 LVGL GPU
  （软渲染开销可忽略）把 DMA2D 让给编码，绕开死局
- **硬编配置**：`qt_index/ht_index` 表路由必须配（全 0 会让编码用的表与
  JFIF 头声明不符 = 色度错乱条纹）；ECS 读取前必须 Invalidate（Flush 会被
  cache 旧行遮蔽 = 首帧好后续花）；RGB565→YUV 的 5/6bit 分量必须先升 8bit
- **`app_player_write_stream` 语义**：成功返回写入字节数（非 0），节流
  超时返回 0（零写入，数据须保留重试）——只有 <0 是错误
- **SDL2 会接管 SIGTERM**：带窗口的进程对 pkill 免疫，残留进程与
  SO_REUSEADDR 叠加 = 多路流互相污染（假 40% 丢包），须显式 signal() 接管
- **libjpeg 默认错误处理会 exit 进程**：摄像头偶发坏帧即炸，所有 decode
  必须 setjmp 恢复
- **ALSA default 设备（PipeWire 层）默认秒级缓冲**：须显式
  set_buffer_size_near，否则语音延迟 10 秒以上
- tiny_jpeg 质量参数是 1/2/3 三档，传 libjpeg 式 1-100 直接拒绝

## 代码地图

| 路径 | 说明 |
|---|---|
| `src/middleware/sip/core/` | SIP/SDP/RTP/G.711/JPEG-RTP 纯 C 实现（双端共用） |
| `src/middleware/sip/ports/` | OSAL：FreeRTOS/lwip 与 glibc 两份移植 |
| `src/middleware/sip/device/sip_service.c` | 设备服务：任务/音频/相机/UI 桥/shell |
| `src/middleware/sip/device/sip_hw_jpeg.c` | Jpeg0 硬件编码（DMA2D 常驻路线） |
| `src/middleware/sip/device/sip_screen.c` | LCD 快照 UDP 回传 |
| `src/middleware/sip/host/` | sip_peer / ALSA / V4L2 / SDL 窗口 / screen_sink |
| `apps-ui/apps/llm/*/{sip_call_*,model_sip_call.c}` | LVGL 通话页三件套 |
| `apps/arcs-mini/button/app_button.c` | 四击入口 + 通话按键拦截 |
| `src/utils/img_jpeg.c` | RGB565→JPEG 软编（拍照共用，支持质量分档） |
