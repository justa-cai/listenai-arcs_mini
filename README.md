# ARCS-MINI Lua 桌面系统（Lua/master）

基于 master（mini-v3.0.2-m3）实现的**可离线运行的 Lua 桌面**：把一套完整的图形
桌面、16 个内置应用（含 5 个游戏）打进**单个 Lua 文件**，由设备的小应用运行时在
PSRAM 中加载运行；操作既可以用设备功能键，也可以用手柄（BLE / 网络）。

原产品说明见 [README-original.MD](README-original.MD)。

## 为什么是"单文件桌面"

小应用运行环境**一次只加载一个 Lua 文件**——沙箱里没有 `load`/`dofile`、没有文件
系统、没有 `require`。所以"桌面 + 全部应用 + 全部游戏"只能拼成一个文件：源码按
应用拆分在 `tests/miniapp/desktop/`，由 `./lua.sh bundle 20-desktop` 拼成
`tests/miniapp/20-desktop.lua`（约 106 KiB）再推给设备。

| 约束 | 值 | 影响 |
| --- | --- | --- |
| 每帧矩形 | ≤ 128 | 画布 240×240，超一个就报错 |
| 每帧文字 | ≤ 8 段，每段 ≤ 63 字节 | 16 px 固定字体，中文一个字 3 字节 |
| 源码上限 | 131,072 字节（本分支的应用级覆盖） | 默认 65,536 |
| 源码块预算 | 500,000 指令 / **2000 ms** | 覆盖"解析 + 顶层执行"整段 |
| 回调预算 | 250,000 指令 / 500 ms | 不含解析 |
| Lua 堆 | 393,216 字节/实例 | 无 `pcall`，任何错误终止整个实例 |

源码块预算从默认的 150 ms 提到 2000 ms 是**必须的**，不是随手放宽：解析也落在
这个窗口里，而解析期间指令钩子不触发，耗时只在执行到第 1000 条指令的检查点被核算。
实测解析速度约 **130 字节/ms**（65,458 字节耗时 504 ms、108,905 字节耗时 816 ms），
128 KiB 的解析约 1 s，取 2 s 留约 2 倍余量。改动见
`apps/arcs-mini/miniapp/Kconfig`（help 里写明了这层耦合）与 `apps/arcs-mini/prj.conf`。

## 架构

```
tests/miniapp/20-desktop.lua                ARCS-MINI (CP 核, PSRAM 中的 Lua VM)
┌──────────────────────────────┐           ┌────────────────────────────────────┐
│ 桌面 Shell (00-shell.lua)     │           │ miniapp.lua 任务                    │
│  ├ 应用注册表 ui.register{}   │  lua.sh   │  └ luaL_loadbufferx → on_tick 20ms  │
│  ├ 焦点/命令栏/图标/notice    │ ────────▶ │     └ screen.rect / screen.text      │
│  ├ 存档中枢 (storage)         │  adb push │         └ LVGL 240×240               │
│  └ 主循环 on_tick / draw      │           │ miniapp.buzz 任务                    │
│                               │           │  └ buzzer.play_seq → 流式 PCM →      │
│ 16 个应用 ui.register{...}    │           │     app_player (16kHz 方波)          │
│  时钟/计时器/秒表/状态灯/待办  │           │ miniapp.http / miniapp.tts 任务      │
│  计算器/设置/关于/一言/朗读    │           │ gamepad 中间件                       │
│  蜂鸣器音乐 + 5 个游戏         │           │  └ miniapp_button_click(button_id)   │
└──────────────────────────────┘           └────────────────────────────────────┘
       ▲                                                        ▲
       └──────── 手柄: BLE(中心) / UDP / WebSocket ──────────────┘
```

按键模型（长按 3 秒被系统拿去退出小应用，脚本收不到）：

| 输入 | `button_id` | 语义 |
| --- | --- | --- |
| 设备键单击 | `function` | 下一个 |
| 设备键双击 / 手柄 A、START | `function_double` | 激活（进入应用、执行当前命令） |
| 手柄方向键 / 摇杆 | `up` `down` `left` `right` | 移动焦点（按住自动重复） |
| 手柄 叉/B | `back` | 返回桌面（任何应用里都生效，应用可不处理） |
| 手柄 SELECT | `settings` | 一步跳到"设置" |

没有手柄时功能键单击是唯一的移动方式。

## 快速开始

### 1. 构建、烧录

```bash
source ./arcs-sdk/env.sh
./auto.sh              # 增量构建 + adb 烧录 app 分区（默认只烧不重建，见脚本帮助）
./auto.sh build        # 强制重新构建后再烧
```

> 首次烧录经常报 `进入recovery失败`，但设备已被留在 recovery 态；**再跑一次即可**。

### 2. 推送桌面

```bash
./lua.sh bundle 20-desktop    # 把 tests/miniapp/desktop/ 拼成单文件
./lua.sh push tests/miniapp/20-desktop.lua   # 推送并立即运行
./lua.sh pull                 # 回捞当前运行应用的源码
./lua.sh log                  # 抓设备日志
```

`lua.sh push` 推送的**目录**决定应用 id：`tests/miniapp/20-desktop.lua` 对应
`local:20-desktop`。长按功能键 3 秒退出小应用。

### 3. 用语音打开桌面

设备注册了 MCP 工具 `miniapp_open`：收到"打开桌面"时由云端调用，把**内存中已加载**
的小应用切到前台。它不下载——小应用退出即销毁、重启也不恢复，设备侧没有可重开的
副本；没有已加载的应用时返回提示，由云端改用 `ls.built_in.miniapp_install` 带上
完整安装包（`id`/`version`/`name`/`url`/`size`/`hash`）重新下发。详见
[docs/miniapp.md](docs/miniapp.md)。

## 内置应用

| 应用 | 说明 |
| --- | --- |
| 时钟 | 系统时间（未校时时显示 `--`），支持在线校时 |
| 计时器 / 秒表 | 倒计时与正计时，走 tick 节拍 |
| 状态灯 | 控制设备状态灯常亮/闪烁 |
| 待办 | 简单清单，存 `storage` |
| 计算器 | 四则与百分号，键盘布局随焦点 |
| 设置 / 关于 | 提示音开关、设备与运行时信息 |
| 一言 / 朗读 | HTTP 取随机句子 / TTS 播报文本 |
| 蜂鸣器音乐 | 9 首经典旋律，整曲流式播放，左右换曲、上下调速 |
| 贪吃蛇 / Flappy / 打砖块 / 2048 | 经典街机玩法 |
| 推箱子 | **99 关**，关卡由反向拉箱生成 + A* 逐关证明可解 |

推箱子的关卡数据是**生成的**（反向拉箱保证构造性可解，再用 A* 逐关验证；随机关卡
摆放无法保证难度与可解性），不要手改。X / 双击下一关，完成一关自动进入下一关，进度存
`sokoban.level`。

## 关键踩坑记录

- **音乐节奏"BPM 对了但不对"**：`buzzer.play` 是"一个音一个播放器"，每次发声都要
  停掉上一个、重建 WAV 缓冲区再起播，实机稳定 ~40 ms，加上任务 20 ms 一次的轮询
  ≈ **每音 50 ms 死区**，而且是逐音累加的——八分音符 250 ms 被拉成 300 ms、四分
  500 ms 变 550 ms，时值比例从 1:2 变成 1:1.83。改法是新增 `buzzer.play_seq` 整曲
  流式播放（运行时边合成 PCM 边推给播放器，音符之间没有缝）。
- **流式写入必须认返回值**：`app_player_write_stream` 返回**实际写入字节数**，会
  短写甚至返回 0；不认返回值就静默丢音频（6 秒的歌只写了 4 秒）。相位也只能按
  accepted 的采样数前进。
- **播完不能 reset**：整曲 PCM 会在约 0.2 s 内写完，此时播放器缓冲里还压着整首歌，
  `app_player_reset` 会把尾巴砍掉（6 秒的歌只剩 1 秒）。只能等它自然放完。
- **流式模式下的 stop 守卫**：流式不支持 `stop/pause/resume/seek`，但
  `app_player_reset` 漏清 `is_stream_mode`（会让之后所有单音被静默丢弃），
  `app_player_core_stop(_sync)` 漏了流式早退——而小应用的焦点策略是
  `on_background = STOP`，任何一次 TTS 抢占都会从 TTS 任务里调到它。
- **按键是"聚合多击"语义**：按键驱动静默 300 ms 后才发一个事件，用户自然的快速
  双击会合成 `DOUBLE_CLICK`，固件必须把它转发成 `function_double`，否则脚本收不到
  任何事件；方向/返回/设置是手柄的离散按键，不受这个窗口影响。
- **BLE 手柄回调只能"拷贝 + 入队"**：GAP 连接/断开回调跑在 BT task 上下文，绝不能在
  里面发起扫描或连接（连接要有独立任务串行处理），否则会卡死协议栈。
- **沙箱没有 `pcall`**：任何 Lua 错误都终止整个桌面实例，且若读过存档还会**清空**
  它。所有绘制与回调都必须自己守住边界（尤其是 `math.floor` 之后的下标）。
- **UI 线程安全**：焦点/导航切页必须投递到 UI 工作队列，从语音事件回调里直调会
  破坏样式链并死机。
- **判断"有没有丢音频"要靠播放器自己的位置**：收尾时轮询 `app_player_get_position`
  取最大值，结束时打一行 `player reports N ms played of M ms written`。那次"听起来
  少了一段"的真相是曲谱里本来就有一个 600 ms 的休止（`R:4`）落在曲子中间。

## 代码地图

| 路径 | 说明 |
| --- | --- |
| `tests/miniapp/desktop/00-shell.lua` | 桌面壳层：应用注册表、按键分发、绘制助手、存档中枢、主循环 |
| `tests/miniapp/desktop/1*.lua` | 10 个工具类应用 + 蜂鸣器音乐（`1A-music.lua`） |
| `tests/miniapp/desktop/5*.lua` | 5 个游戏，含 99 关推箱子（`54-sokoban.lua`） |
| `tests/miniapp/20-desktop.lua` | 打包产物（由 `./lua.sh bundle 20-desktop` 生成，勿手改） |
| `apps/arcs-mini/miniapp/miniapp_runtime.c` | Lua 运行时：VM 生命周期、按键/蜂鸣器/HTTP/TTS 桥接、整曲流式播放 |
| `apps/arcs-mini/miniapp/Kconfig` | 小应用配置（源码上限的 range 与应用级覆盖的耦合说明在这里） |
| `apps/arcs-mini/mcp-tools/mcp_tool_miniapp.c` | MCP 工具：`ls.built_in.miniapp_install` / `miniapp_open` / `ls.built_in.miniapp_exit` |
| `src/middleware/gamepad/` | 手柄中间件：BLE 中心（`ble_pad.c`）、UDP、WebSocket 三来源统一成按键事件 |
| `arcs-sdk/components/lisa_bluetooth/` | BLE 中心侧 ABI 与本地栈适配 |
| `arcs-sdk/components/app_player/` | 播放器（流式接口的 `is_stream_mode` 守卫修复在这里） |
| `lua.sh` | Lua 脚本 bundle / push / pull / log |
| `auto.sh` | 一键构建 + 烧录 |
| `docs/miniapp.md` | 小应用协议与接口文档（含 `buzzer.play_seq` 与 `miniapp_open`） |
