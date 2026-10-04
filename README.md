# ARCS-MINI 电子宠物（Anima 分支）

基于 LISTENAI ARCS SoC（RISC-V 双核 AP/CP）的电子宠物固件，从产品主线 `master` 衍生。
原产品说明见 [README-original.MD](README-original.MD)。

设备上电即进入全屏宠物页面：一只程序化自绘的团子小生物，从蛋孵化、成长进化，
用语音照料它，关机断电它也继续活着。

## 功能总览

| 能力 | 说明 |
| --- | --- |
| 墙钟养成 | 饱食/开心/清洁/精力四维数值按真实时间衰减，一天照料一两次即可 |
| 进化线 | 蛋（裂纹三段+晃动）→ 幼年（呆毛团子）→ 成年（圆耳朵+项圈），照顾质量决定进化 |
| 状态系统 | 便便（正餐后定时产生）、生病（任一维归零 6 小时）、昼夜睡眠（22:00-7:00 自动） |
| 语音控制 | 在线：任意自然语言（云端 LLM → `pet_care` 工具）；离线：13 条固定命令词（本地 ESR） |
| 断电续命 | 存档写 flash（动作即存/60s 节流/关机钩子），重新上电按真实经过时间补算（48h 上限 + 6 折衰减） |
| 状态提醒 | 饿/无聊/脏/困/想念 阈值提醒（30 分钟冷却，睡眠时段静默） |
| 轻养成不死 | 数值有下限无死亡，长时间放置回来也不会"惨状" |

## 语音控制

**唤醒词：`你好星宝`**（唤醒后约 20 秒内可说命令词，窗口时长可由 KV 配置）

| 命令词 | 动作 | | 命令词 | 动作 |
| --- | --- | --- | --- | --- |
| 喂它吃饭 | 喂主食（饱食+45） | | 它还好吗 | 播报状态 |
| 给它零食 | 喂零食（开心+8） | | 打开宠物 | 进入宠物页 |
| 打扫卫生 | 清洁+清便便 | | 回到主页 | 回设备主页 |
| 陪它玩耍 | 开心+32 精力-12 | | 你好呀 | 打招呼回应 |
| 喂它吃药 | 生病时恢复 | | 我爱你 | 满屏爱心 |
| 开灯 / 关灯 | 唤醒 / 入睡 | | 摸摸它 | 眯眼开心 |

- **联网时**：命令词被在线优先策略让位，云端 LLM 用 `pet_care` 工具理解任意说法（"给它弄点吃的"也行）
- **离线时**：本地 ESR 识别上述 13 条，无需联网
- 完整清单（含拼音）见 [`pet_commands.csv`](pet_commands.csv)

## `pet` 调试命令（设备 shell / `adb shell "pet ..."`）

不用等真实时间流逝即可验证全部功能：

```bash
pet                          # 查看状态（阶段/天数/心情/四维/便便/裂纹）
pet set  <sat|hap|cln|ene> <0-100>   # 设置数值（饱食/开心/清洁/精力）
pet add  <sat|hap|cln|ene> <-100..100>
pet stage <egg|baby|adult>            # 跳进化阶段
pet time <seconds>                    # ★ 快进状态机（衰减/便便/生病/日切/进化全推进）
pet act  <feed|snack|clean|play|light|wake|medicine|status|greet|love|pat>
pet evt  <eat|hatch|evolve|slept|rmiss|...>   # 注入反应动画/提醒事件
pet sleep <0|1> | pet sick <0|1> | pet poop <0-3>
pet eggage <s> | pet care <n> | pet age <d> | pet good <d>
pet save | pet reset                  # 强制存档 / 重开新蛋
```

常用验证场景：

| 目标 | 命令序列 |
| --- | --- |
| 看成年形象 | `pet stage adult` |
| 快进一天 | `pet time 86400` |
| 造饥饿提醒 | `pet set sat 30` → `pet evt rhungry` |
| 孵化全流程 | `pet reset` → `pet act feed` ×3 |
| 生病闭环 | `pet set ene 0` → `pet time 22000` → `pet act medicine` |

## 构建与烧录

```bash
source ./arcs-sdk/env.sh                                  # 环境自检（一次）
./auto.sh build          # 构建 + 烧 app（日常开发用这条）
./auto.sh                # 只烧不构建（固件已就绪时）
./auto.sh full           # 烧全部资源分区 + app（改 wake_word/tone 等资源后用）
./auto.sh log            # 重启抓 10 秒启动日志到 ./tmp/run-log.txt
DEVICE=<serial> ./auto.sh ...   # 多设备时显式指定
```

构建目录 `build-anima`（勿用仓库根 `build/`，那是旧 ble 示例的缓存）。

## 命令词资源更新流程

1. 在聆思**算法资源打包平台**配置唤醒词 + 命令词（对照 `pet_commands.csv`），下载资源包
2. 解压取 `Standard_product/stage1_install/res/{cae_esr.bin, wrap.json}` 放入 `res/arcs-mini/wake_word/`
3. 重打包：`python3 ./tools/romfs.py pack -i ./res/arcs-mini/wake_word -o ./res/arcs-mini/wake_word.bin`
4. `./auto.sh full` 烧录（含 wake_word 分区）

`prj.conf` 已开 `CONFIG_OTA_DISABLE_WAKEWORD_UPDATE=y`，防止云端自动更新覆盖自定义资源。

## 真机画面

以下截图全部来自实机 `pet shot`（WebSocket 屏幕截图通道，见下）：蛋期三阶段 → 幼年各心情与事件反应 → 成年形态。

| 蛋期 | | | |
| --- | --- | --- | --- |
| ![蛋](docs/images/pet_egg.png?v=4) | ![裂纹一](docs/images/pet_egg_crack1.png?v=4) | ![裂纹二·晃动](docs/images/pet_egg_crack2.png?v=4) | ![幼年](docs/images/pet_baby_normal.png?v=4) |

| 幼年·心情与事件 | | | |
| --- | --- | --- | --- |
| ![开心](docs/images/pet_baby_content.png?v=4) | ![饿了](docs/images/pet_baby_hungry.png?v=4) | ![睡觉](docs/images/pet_baby_sleep.png?v=4) | ![吃饭动画](docs/images/pet_baby_eat.png?v=4) |

| 成年 | | | |
| --- | --- | --- | --- |
| ![成年](docs/images/pet_adult_normal.png?v=4) | ![成年开心](docs/images/pet_adult_content.png?v=4) | ![成年睡觉](docs/images/pet_adult_sleep.png?v=4) | ![生病](docs/images/pet_baby_sick.png?v=4) |

完整 16 张（含难过/脏便便/玩耍/爱心等）在 [`docs/images/`](docs/images/)。

**抓取方法**：宿主起接收端 `python3 tmp/shot_server.py`，设备执行 `pet shot ws://<host-ip>:8899/shot`——
设备侧对当前屏幕做 LVGL 离屏快照（240×240 RGB565），经 WebSocket 二进制帧推给宿主转 PNG。
绕开了 adb shell 输出的 2KB 缓冲限制（`CONFIG_ADB_SHELL_BUFFER_SIZE`），单帧 115KB 稳定传输；
`bash tmp/pet_scenes.sh <serial>` 一键抓全套场景。

## 宠物台词清单（tone 分区，`pet tone <id>` 可试听）

26 条预合成台词（VoxCPM `voice_design`="软糯奶萌的小孩子声音，萌萌的"，16kHz mono mp3，
`res/arcs-mini/tone/`），ID 即 TONE_ID：

| ID | 台词 | 触发场景 |
| --- | --- | --- |
| 106 | 啊呜啊呜，真好吃！ | 喂主食成功 |
| 107 | 小零食最开心啦！ | 喂零食成功 |
| 108 | 我超级饱，一口都吃不下啦 | 饱食≥90 再喂 |
| 109 | 洗得香香软软的 | 打扫成功 |
| 110 | 我本来就很干净呀 | 已干净时打扫 |
| 111 | 耶！蹦蹦跳跳最开心了！ | 玩耍成功 |
| 112 | 呼……跑不动了，让我先睡一觉吧 | 精力<15 玩耍被拒 |
| 113 | 晚安，做个好梦 | 关灯入睡 |
| 114 | 早上好！ | 开灯唤醒 |
| 115 | 一点都不困，还想再玩会儿！ | 不困时关灯被拒 |
| 116 | 药苦苦的，可是感觉好多了！ | 喂药成功 |
| 117 | 我又没生病，不要吃药啦 | 未生病喂药被拒 |
| 118 | 嘘，我睡着啦 | 睡着时操作被拒 |
| 119 | 我的状态在屏幕上哦，快看看吧 | 查状态（离线） |
| 120 | 蛋轻轻晃了一下，好像在回应你 | 蛋期照料 |
| 121 | 你好呀，见到你真开心！ | "你好呀" |
| 122 | 我也爱你！ | "我爱你" |
| 123 | 好舒服呀 | "摸摸它" |
| 124 | 咔嚓，哇！我出生啦！ | 孵化（设备主动） |
| 125 | 哇！我长大了！ | 进化（设备主动） |
| 126 | 我肚子饿得咕咕叫了，快喂我吃饭吧 | 饥饿≤30 提醒 |
| 127 | 好无聊呀，谁来陪我玩一会儿嘛 | 开心≤30 提醒 |
| 128 | 我这里臭臭的，帮我打扫一下啦 | 清洁≤30 提醒 |
| 129 | 我困了，说声关灯我就睡觉咯 | 夜间精力低提醒 |
| 130 | 呜，我好像生病了，给我吃点药吧 | 生病提醒 |
| 131 | 好久没见到你了，我好想你呀 | 24h 未互动提醒 |

播放策略：离线命令词动作反馈 + 设备主动事件（提醒/孵化/进化）由设备播放（tone 通道
优先级最高，可打断云端 TTS）；在线动作反馈由云端 TTS 读带数值的动态文案，设备不重复播。
改台词/换音色：改 `pet_voice` 文案清单 → VoxCPM 重新合成 → 重打包 tone.bin → `./auto.sh full`。

## 代码结构

```
apps/arcs-mini/pet/
├── pet_core.c/h    纯逻辑状态机（四维衰减/进化/便便/睡眠/生病/时间锚点，mutex 保护，禁 lv_*）
├── pet_draw.c/h    lv_canvas 程序化绘制（蛋/幼年/成年形象、动画、粒子）
├── pet_ui.c/h      全屏默认页（状态条+画布+命令提示行+toast+双 lv_timer）
├── pet_save.c/h    lisa_kv 存档（versioned blob + CRC）
├── pet_buzzer.c/h  方波蜂鸣合成（16kHz WAV mem:// 播放）
├── pet_shell.c     pet 调试命令
└── pet.c/h         门面
apps/arcs-mini/offline_cammand.c      离线命令词 → pet_core_action
apps/arcs-mini/mcp-tools/mcp_tool_pet.c   云端 pet_care 工具（在线自然语言）
tmp/pet_sim/                          Ubuntu 宿主 LVGL 离屏渲染器（形象迭代用，bash build.sh 出图）
```

### 形象迭代

`tmp/pet_sim/` 是宿主端离屏渲染器，复用设备同款 lvgl8 源码与同一份 `pet_draw.c`：

```bash
cd tmp/pet_sim && bash build.sh     # 生成 pet_frames.png（21 帧状态×心情×事件矩阵）
```

改形象不用烧录，出图满意后再编进固件。

## 硬件相关说明

- 当前硬件**无 SD 卡卡座**，SDMMC/TF 音乐功能已整体禁用（`CONFIG_DISK_DRIVER_SDMMC` 关闭，
  有卡板型加回该配置即可恢复全部功能）
- 设备仅一个电源键：宠物页上单击=切换图标、双击=确认、三击=取消、四击=回主页（无键产品形态下语音为主）

## 分支与发布

- 分支：`Anima/master`（主线 `master`，产品原 README 见 `README-original.MD`）
- GitHub 镜像：`justa-cai/listenai-arcs_mini` 的 `Anima/master`（经大文件过滤的 `anima-gh` 分支同步）
