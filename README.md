# ARCS-MINI 异常声音检测客户端（SoundSense）

基于 LISTENAI ARCS SoC（RISC-V 双核 AP/CP）的婴儿房异常声音监测固件，
从产品主线 `master` 衍生（原产品说明见 [README-original.MD](README-original.MD)）。

设备作为 WebSocket 客户端把麦克风 PCM 实时推流给局域网 GPU 服务器
（PANNs Cnn14），检测 **哭声（baby_cry）/ 鼾声（snoring）**，
本固件是该方案的设备端。

## 功能总览

| 能力 | 说明 |
| --- | --- |
| 实时推流 | 16kHz/mono/pcm_s16le，1600B/50ms，断线 5s 起指数退避自动重连 |
| 检测面板（默认页） | 中文界面：各类事件计数、最近事件时间（设备本地墙钟）、实时概率条、连接状态分色 |
| 事件记录 | 环形缓冲保存最近事件（类/起止/时长/峰值概率），`ss events` 与 MCP 均可查 |
| MCP 工具组 | `soundsense_status` / `soundsense_events` / `soundsense_server`，云端 LLM 语音查询 |
| 自动息屏 | 白天 30s / 夜间（22:00-7:00）10s 关背光，监测后台持续；按键/语音/检测事件唤醒 |
| 按键行为 | 单击仅唤醒屏幕，不导航、不拉起语音会话（专用监测机） |
| 配置持久化 | 服务器地址 / 监测开关存 KV，重启恢复 |

## 模块结构

```
apps/arcs-mini/soundsense/
├── ss_core.c/h    协议客户端：POSIX socket 自实现 WebSocket（RFC 6455，
│                  SHA-1+Base64 握手、客户端掩码、分帧收发）、指数退避重连、
│                  事件环形缓冲、KV 配置
├── ss_audio.c/h   lisa_audio observer 分叉左声道 → PSRAM 环形缓冲（与唤醒引擎并存）
├── ss_panel.c/h   中文检测面板（lisa_ui nav 默认页，经 worq.ui 投递接管）
├── ss_screen.c/h  自动息屏管理（仅背光 PWM/IOMUX，不打断 LVGL 刷新）
└── ss_shell.c     ss 调试命令

apps/arcs-mini/mcp-tools/mcp_tool_soundsense.c   MCP 工具组
docs/SoundSense_PROTOCOL.md                      服务端协议契约 v1.0
docs/SoundSense_DEVELOPMENT.md                   开发记录
```

## 服务器

默认 `ws://192.168.1.169:8000/v1/stream`（可经 `ss server <url>` 修改）。
服务器需提供 PANNs Cnn14 推理（window 3s / hop 0.5s），
协议（hello/welcome/result/event/ping-pong，心跳 30s、空闲超时 60s）
见 [`docs/SoundSense_PROTOCOL.md`](docs/SoundSense_PROTOCOL.md)。

## 构建与烧录

```bash
source ./arcs-sdk/env.sh
./build.sh -S ./apps/arcs-mini -B build-ss -DBOARD=arcs_mini

# ADB 烧录（推荐只烧 CP app）
bash adb_download.sh -S res/arcs-mini app

# 日志
mkdir -p ./tmp && adb shell reboot && adb wait-for-device && timeout 10 adb shell 2>&1 > ./tmp/run-log.txt
```

## `ss` 调试命令（`adb shell "ss ..."`）

```bash
ss                     # 状态：开关/连接/时长/帧计数/丢帧/各类概率
ss on | ss off         # 启停监测（本次开机有效，开机默认自动连接）
ss server <url>        # 修改服务器地址（重连生效）
ss events [n] [class]  # 最近事件列表（默认 10 条，可按 snoring|baby_cry 过滤）
ss blank <0|1>         # 手动息屏/亮屏
```

## MCP 工具组（语音查询）

| 工具 | 参数 | 返回 |
| --- | --- | --- |
| `soundsense_status` | — | 开关/连接状态/服务器 URL/运行时长/累计事件/最近概率 |
| `soundsense_events` | `count`(默认 10) `class`(可选) | 事件列表（类/起止/时长/峰值概率） |
| `soundsense_server` | `query`(events/stats) | 透传服务端查询 + welcome 配置回显 |

协议层类名保持英文（`snoring`/`baby_cry`），仅设备面板展示层为中文。

## 分支说明

- 分支：`SoundSense/master`（GitHub 发布历史已剔除 200MB 的 SDK 样例资源
  `algo_emmc.bin`，业务代码与内部一致）
- 包含产品级修复：按键导航跨线程操作 LVGL 导致的随机死机
  （nav_to 必须投递 worq.ui 执行，详见 `fix(ui)` 提交说明）
