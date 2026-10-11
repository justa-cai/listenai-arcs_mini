# Muse Gadget SDK → ARCS-MINI 移植设计

> 基于 `facebookincubator/muse-gadget-sdk`（Apache-2.0，2026-10-02 开源）的源码分析，
> 目标是把 ARCS-MINI（RISC-V CP 核 + 240×240 屏 + 双麦 + WiFi + BLE）变成一块
> **Muse Gadget**：接入 Muse 个人 AI 助手，支持配对、push-to-talk 语音对话与
> 文本回复显示。本文档同时定义与既有 listenai 云语音功能的共存与禁用策略。

## 1. Muse SDK 协议分析结论（源码级）

### 1.1 分层结构（esp32 工程）

```
app 层（main/，板级+业务）         ← 全部重写（换成 ARCS-MINI 板级与 UI）
├── ble_server.c          Muse App 配对服务（自定义 GATT）
├── link_pairing.c        配对握手：X25519(mbedtls ECP) + Ed25519 签名 + transcript
├── pairing_transcript.c  配对转录（版本/角色/公钥/nonce 的规范化字符串）
├── noise_control.cpp     Noise 会话宿主（主任务、请求复用）
├── noise_tunnel.cpp      家庭网络隧道（Muse 反向触达局域网设备）
└── vm_api.c              VM 命令执行（Linux 侧为主）

协议核心（components/，可移植层）
├── muse/                 业务协议：BLE 设置服务、chat 会话、音频、设置、UI(LVGL9)
│   ├── muse_chat_session.cpp   ★ 云端会话（~1500 行 C++，见 1.2）
│   ├── muse_voice.c            push-to-talk 音频管线（ADPCM 缓存/回放）
│   ├── muse_adpcm.c            IMA-ADPCM 编解码（~100 行，零依赖）
│   ├── muse_account_api.c      HTTPS api.muse.ai/fetch_vms 换 VM token
│   └── muse_settings/console/...  设置存储、调试命令（NVS 依赖需替换）
└── noise_core/           Noise 协议栈（21 文件 ~4.9K 行 C++）
    └── xplat/noise/core/{ClientSession, InitiatorHandshake, ServiceCodec,
       TransportFrameCodec, MbedtlsCryptoBackend, PsaCryptoBackend}
       —— 命名空间 musegadgets::noise::core，mbedtls 后端，平台无关设计
```

### 1.2 云端会话（muse_chat_session.cpp 的核心事实）

一条 **HTTP-over-Noise** 长连接承载全部业务：

1. **连接建立**：`TLS(mbedtls) + 443` → 手写 WebSocket Upgrade（`GET /v1/noise?vm_id=…`，`Authorization: Bearer <vm_token>`，无 Sec-WebSocket-Key 随机性——固定 sample key）→ **Noise 协议握手**（InitiatorHandshake）→ 之后在 WebSocket 帧里跑**多路复用请求流**（ServiceCodec：verb/path/headers/body 的二进制编码）
2. **默认主机**：`hatch.metaaivm.com`（可在设置覆盖）；VM 发现：`https://api.muse.ai/fetch_vms`（Bearer device_token → 返回 VM 列表，`wss://<id>.<domain>` URL）
3. **一次 push-to-talk 轮**（三个流并发）：
   - `POST /api/voice/dictation`（body 不关闭）：mic 以 **24 kHz PCM16** 直传（SDK 内部 16 kHz 采集→上行 24 kHz；DICT_CHUNK_BYTES=8192≈170 ms/块），NDJSON 回累计部分转录
   - `POST /chat/stream`（发最终转录）：回复走长连接的 `POST /chat/subscribe`（NDJSON 事件：message_start / text_append / message_done）
   - TTS 可选（"Muse doesn't speak gadget replies"——gadget 自选 TTS 或只显示文字）
4. **缓冲预算**（PSRAM，可借鉴）：入帧重组 SCRATCH 64 KB、NDJSON 行 16 KB（事件行可长到 256 KB）、MP3 缓冲 512 KB、mic 积压 8 s、回复 PCM 2 s
5. **保活**：20 s ping / 60 s 判死 / 10 min 空闲关闭 / 失败指数退避（5 s→120 s）

### 1.3 BLE 配对（Muse App ↔ gadget）

- GATT 服务 `ble_server.c`：手机 App 扫到 `MuseGadget*` 前缀设备 → 连接写入
  **provision 载荷**：`{ssid, password, access_token, refresh_token, username, ota_url, api_url, noise_host, session_generation}`
- 密钥学：**X25519 ECDH**（mbedtls ECP）+ Ed25519 签名（生产 eFuse / 社区 dev key 双策略，`pairing_signer_policy.c`），transcript 是规范化 KV 字符串（`hatch-link-pairing-v2` 格式）签名后互换
- **关键**：配对产物 = WiFi 凭据 + device access_token（长期）→ 之后所有云端交互只需 token

### 1.4 音频

- 采集 16 kHz 单声道（MUSE_AUDIO_CHUNK=320 帧=20 ms）
- **IMA-ADPCM**（muse_adpcm.c）只用于**本地环回缓存/暂停回放**（每 chunk 存编码前状态，可独立解码任意块）；**上行 dictation 是纯 PCM16**（24 kHz），无需编解码器
- 回复侧如有 MP3 用 **minimp3**（CC0，单文件，可直接编译）

### 1.5 移植性评估

| 层 | ESP-IDF 依赖 | 移植难度 |
|---|---|---|
| noise_core（Noise 协议） | 仅 mbedtls 后端 + C++17 | **低**：头文件自包含，换 OSAL 即可 |
| muse_chat_session | esp_tls/esp_timer/esp_random/FreeRTOS 队列 | **中**：TLS 换 ze_tls，其余 FreeRTOS 我们都有 |
| muse_adpcm / minimp3 | 无 | **零改动**直接编译 |
| muse_voice | esp_codec_dev 音频 | **重写**：接 ARCS-MINI audio0（已有 post-AEC tap 先例） |
| muse_account_api | esp_http_client | **中**：换 lisa_http/httpclient（HTTPS 已验证可用） |
| BLE 配对 | NimBLE GATT | **重写**：lisa_bluetooth 栈（BLE 配网 GATT 已有先例 app_net_cfg） |
| UI（muse_ui，LVGL9） | LVGL9 | **重写**：复用 miniapp/LVGL8 桌面与绘制体系 |
| 设置存储 | NVS | **低**：换 lisa_kv（KV 先例丰富） |

C++17 依赖：noise_core 与 chat_session 是 C++（optional/string_view/span）——
ARCS-MINI 工具链是 gcc（RISC-V，支持 C++17）但仓库至今纯 C。**noise_core 保持 C++ 编译**（独立库，不传染），胶合层用 C。

## 2. ARCS-MINI 侧资源核对

| 需求 | 现状 | 结论 |
|---|---|---|
| TLS 客户端 | mbedtls（CONFIG_MBEDTLS=y，SSL in 16 KB/out 4 KB）+ httpclient/ze_tls（电台 HTTPS 已跑通） | ✅ 可承载 Noise 的 TLS+WS |
| PSRAM | 6 MB 池（电台约用 1 MB） | ✅ Noise 会话预算 ~1.5 MB 可满足，但与电台并存需上限控制 |
| FreeRTOS 队列/流缓冲 | sip/电台大量先例 | ✅ |
| 随机数 | sip_osal xorshift（非加密）→ **配对/Noise 需要硬件熵**：SDK 侧有 mbedtls entropy（查 CONFIG）| ⚠️ 需确认 TRNG/熵源，不行则 seed 混合（地址+tick+ADC 噪声） |
| BLE GATT 外设 | lisa_bluetooth（配网服务先例）| ✅ 需按 ble_server 协议重写 GATT 表 |
| 麦克风 16 kHz | wakeup AEC 后 tap（sip 通话在用）| ✅ 复用 tap；**注意 AEC tap 是 post-AEC 单麦**，正好适合 |
| 扬声器 | app_player 通道制 + 焦点 | ✅ 新增 "muse" 通道 |
| 屏幕 | LVGL8 桌面 / miniapp | ✅ 文本对话 UI 用 miniapp 思路或独立页面 |
| C++17 | 工具链支持，仓库未用过 | ⚠️ 新增 .cpp 编译单元，链接 libstdc++（-O2 控体积） |

## 3. 与 listenai 云功能的共存/禁用设计（核心决策）

### 3.1 冲突面

| 资源 | listenai 云（现状） | Muse | 冲突 |
|---|---|---|---|
| 麦克风 | 唤醒引擎常开（AEC 前）+ 上云会话 | dictation 上行需要独占采集 | **麦克风互斥** |
| 扬声器/焦点 | tone/tts/music/miniapp 通道 | 回复 TTS/提示音 | 焦点表可共存（加通道） |
| 屏幕 | 桌面 + 语音动效 | Muse 对话 UI | **前台互斥**（复用 miniapp sticky 先例） |
| 云连接 | lschat 长连接（语音会话） | Noise 长连接 | 无直接冲突（不同服务器），但带宽/CPU 并存 |
| 唤醒 | "王道长" 常开 | 无要求 | 电台分支已示范 can_wakeup=0 |

### 3.2 编译级：单一固件双形态（推荐）

一个 Kconfig 开关定形态，出厂/调试都不需要两份代码：

```
CONFIG_MUSE_GADGET=y        # Muse 模式（本分支默认）
CONFIG_LISTENAI_VOICE=y     # listenai 云语音（master 默认，本分支关闭）
```

依赖关系：两者可同时编译（代码共存），**运行时互斥由"前台宿主"决定**（见 3.3）。
但本分支定位是 Muse 设备，**默认关 listenai**（与电台分支禁唤醒同思路）：

- `CONFIG_LISTENAI_VOICE=n` 时：不初始化 acomp 唤醒、不建 lschat 会话、
  唤醒词/资源分区不加载——省 ~1.4 MB PSRAM 与常驻 CPU
- 复用现成门控点：`app_datas.c can_wakeup=0`、`miniapp_ui` 不 model_voice_on、
  （更彻底）`main.c` 里 lschat/acomp init 条件化

### 3.3 运行级：宿主切换模型（若两云共存编译）

引入 **"语音宿主"（voice host）** 概念，同一时间只有一个云在跑：

```
voice_host_set("listenai" | "muse" | "none")
```

- 触发点：M muse 配对成功 → host=muse（停止唤醒引擎、断 lschat 空闲连接、
  桌面切 Muse UI）；Muse 解绑 → host=listenai（恢复）
- 麦克风仲裁：单一 `mic_arbiter`（新）：tap 注册制，host 持有者独占
  ——比现在 sip/唤醒分散抢占干净，sip 通话也可迁移
- 焦点表加 `muse` 通道（prio 与 tts 同级），TTS 互斥自动成立
- UI：Muse 对话页注册为 nav scr（如 miniapp），进入即 sticky（复用
  `lisa_ui_nav_set_sticky_home_block` 先例），语音/按键不切走

### 3.4 token 与安全

- device access_token / refresh_token / vm_token：存 lisa_kv（加密分区可选，
  与 wifi 凭据同级别）；KV 键前缀 `muse.`
- 配对签名私钥：社区 dev key 策略（SDK 允许，`PAIRING_SIGNER_COMMUNITY`），
  dev key 随固件（上游 `dev_signing_key.pem` 的处理方式照搬）

## 4. 移植架构（目标形态）

```
┌────────────────────────── ARCS-MINI CP 核 ──────────────────────────┐
│  muse_port（新中间件 src/middleware/muse/）                          │
│  ┌─────────────── 应用层（C，重写）────────────────┐                 │
│  │ muse_app.c     宿主状态机（未配对/配对中/在线） │                 │
│  │ muse_ui.c      对话页面（nav scr + LVGL8）      │                 │
│  │ muse_audio.c   mic tap(16k)→24k 上采样→PCM16   │                 │
│  │                回复文本显示 + 自选 TTS 播报     │                 │
│  │ muse_pair.c    BLE GATT 配对（lisa_bluetooth）  │                 │
│  └────────────────────────────────────────────────┘                 │
│  ┌──── 协议核心（C++，最小改动直接编入）────┐                        │
│  │ noise_core/   （+mbedtls 后端，原样）     │                        │
│  │ chat_session  （换 OSAL：esp_tls→ze_tls，│                        │
│  │               esp_timer→FreeRTOS，       │                        │
│  │               esp_random→mbedtls CTRBG） │                        │
│  │ adpcm/minimp3（原样）                    │                        │
│  └──────────────────────────────────────────┘                        │
│  OSAL（ports/muse_osal.c）：TLS(ze_tls)/UDP-TCP(lwip)/KV/时间/日志    │
└──────────────────────────────────────────────────────────────────────┘
        │ TLS 443                    │ BLE GATT
   hatch.metaaivm.com           Muse App (iOS/Android)
```

**目录与构建**：`src/middleware/muse/{core(上游拷贝),ports,device}`，仿 sip 中间件
（core 不动 + ports 换 OSAL）。C++ 文件只进 core 子目录，CMake
`add_library(muse_core STATIC …)` + `target_compile_features(cxx_std_17)`。

### 4.1 音频通路细节

- **上行**：AEC 后 tap（16 kHz）→ 线性插值上采样 24 kHz（sip 通话 8k→16k
  插值先例照搬）→ 8192 B/块（170 ms）直写 dictation 流
- **下行**：v1 只做文字（captions 滚动显示，SDK 同款"reading pace"）；
  TTS 二期：设备端 tts.speak 或 VOICE_TTS 通道播报
- push-to-talk 按键：设备键长按（或单击进入通话态）——沿用 miniapp 按键拦截先例

### 4.2 内存预算（PSRAM）

| 项 | 上限 |
|---|---|
| Noise 会话（SCRATCH 64K×2 + 行缓冲 16K+64K + 环境帧） | ~256 KB |
| mic 积压 4 s（缩自 8 s） | 128 KB |
| 文本/caption 缓冲 | 16 KB |
| TLS 上下文 + 证书 | ~48 KB |
| 预算总封顶 | **512 KB**（`MUSE_MEM_CAP` 编译期上限，超限降级：不入网只显示） |

## 5. 里程碑

| 阶段 | 内容 | 验证 |
|---|---|---|
| M0 协议打通（主机） | noise_core+chat_session 在 Ubuntu 编译，token 直连 hatch 拿一条对话回显 | 主机跑通一轮 push-to-talk（文件喂 PCM） |
| M1 设备 OSAL | ze_tls/lwip/KV 移植，设备上建立 Noise 会话（打印 subscribe 事件） | 串口看到 message_start/text_append |
| M2 BLE 配对 | GATT 服务 + X25519 握手 + token 入 KV | Muse App 扫到 MuseGadget-ARCS 并配对成功 |
| M3 语音闭环 | mic tap→24k→dictation；captions 上屏 | 按住说话→屏幕逐字出转录与回复 |
| M4 共存收尾 | listenai 门控/宿主切换/焦点通道/sticky UI；电台 app 与 Muse 并存策略 | 双形态编译均过；Muse 在线时唤醒不抢麦 |

## 6. 风险与待确认

1. **SDK token**：gadgets.muse.ai 申请（Muse App 开发者模式）；国内网络对
   `*.metaaivm.com`/`api.muse.ai` 的可达性（可能需代理或 DNS 直连测试）
2. **熵源**：确认 arcs SoC TRNG；无则 mbedtls entropy 种子混合（MAC+ADC 噪声+tick）
3. **mbedtls 4 KB out 缓冲**：Noise 握手帧/证书链是否放得下（SDK ESP 侧默认
   in 16K/out 4K——与我们一致，大概率 OK，M1 实测）
4. **C++ 异常/RTTI**：noise_core 用不用 exception（看源码 `noexcept` 风格，
   预计 `-fno-exceptions -fno-rtti` 可编，M0 验证）
5. **配对协议逆向完整性**：GATT 特征 UUID/句柄需从 ble_server.c 全量核对
   （本设计仅记录 provision 载荷字段，M2 前完成字段级比对）

## ⛔ 7. M0 实测阻塞（2026-10-11）

**Muse AI 服务仅对美/加地区开放**，中国大陆不可用：

| 验证项 | 结果 |
|---|---|
| SDK token 申请 | ✅ 能拿到（`mgst_` 前缀，48 字节） |
| `api.muse.ai/fetch_vms` 直连（中国出口） | ❌ HTTP 401 Authentication Error |
| `api.muse.ai/fetch_vms` 走旁路由（代理出口） | ❌ HTTP 401（同错误——**不是网络问题**） |
| `hatch.metaaivm.com` TLS | ✅ 可达（但无有效 token 无法 upgrade） |
| Muse App | 仅美/加 App Store / Google Play 上架 |

根因：**SDK token 与账号地区强绑定**。401 是服务端拒绝认证（账号归属
非美/加），与请求来源 IP 无关——代理能换 IP 但换不了账号归属。
未找到公开的开发者/测试通道绕过。

**技术侧结论（已完成部分不受影响）**：
- noise_core 4.9K 行 C++ 在 Ubuntu/mbedtls 2.28/C++17
  `-fno-exceptions -fno-rtti` 编译通过——移植技术路线已验证可行
- TLS→WS→Noise 握手链路代码就绪（tmp/muse-host/muse_probe.cpp，
  待有效 token 即可全链路验证）
- BLE 配对协议、ADPCM/minimp3、chat 会话协议分析全部完成

**恢复条件**（满足其一即可继续）：
- Muse 向更多地区开放（Meta 官方宣布）
- 获得美/加地区的账号 + SDK token（住宅 IP 更稳）
- 官方推出开发者/测试通道
