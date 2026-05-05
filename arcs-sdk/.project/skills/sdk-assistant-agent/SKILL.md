---
name: sdk-assistant-agent
description: >
  ARCS SDK / LISTENAI SDK 开发助手统一入口。当用户在此 SDK 工程中进行任何开发任务时触发，包括但不限于：
  驱动开发（LISA_DEVICE_REGISTER、API vtable、外设驱动、UART/SPI/I2C/GPIO/Flash/Display）、
  构建调试（CMake 编译错误、链接失败、Kconfig/menuconfig、map 文件、内存不足、编译烧录串口）、
  样例生成（怎么用 xxx、写个 demo/sample、使用示例）、
  代码审查（review、检查代码、PR）、
  SDK 架构问题（SYS_INIT、FreeRTOS、ELF 段、链接脚本、自定义段）。
  凡涉及 arcs_mini、arcs_evb 板型，或 listenai_library、listenai_add_executable 等 CMake 宏的问题，均应触发此 skill。
compatibility: Designed for Claude Code in LISTENAI ARCS SDK repositories
allowed-tools: Read Glob Grep Bash WebFetch
---

# SDK 开发助手

> 系统全景图：`references/index.md`
> 修改本文件前请先阅读 `README.md` 中的开发规则

## 意图识别与路由

分析用户输入，匹配领域和模块，读取对应 worker 文件执行：

### 领域路由

| 领域 | 触发信号 | 读取并执行 |
|------|----------|------------|
| **驱动开发** | 新驱动、设备注册、LISA_DEVICE、API vtable、外设、lisa_xxx 使用/移植 | `references/driver-dev.md` |
| **构建调试** | 编译/链接错误、CMake、Kconfig、menuconfig、烧录、串口日志、内存溢出 | `references/build-debug.md` |
| **样例生成** | 写示例、demo、sample、example、怎么用 xxx、使用方法、需要产出可复用示例或完整代码 | `references/sample-gen.md` |
| **代码审查** | review、审查、检查代码、这段代码有问题吗、PR | `references/code-review.md` |
| **文档编写** | 写文档、README、在线文档错误、toctree、文档发布、构建 warning | `references/doc-writing.md` |
| **知识维护** | 更新知识、优化 agent、检查知识库、SDK 更新同步、从提交提取经验 | `references/evolution.md` |

### 模块识别

识别用户输入中涉及的 SDK 模块，加载对应的 experience 文件：

| 关键词 | 模块 |
|--------|------|
| gpio、引脚、按键、led、输入输出 | lisa_gpio |
| uart、串口、serial、收发、波特率 | lisa_uart |
| spi、spi master、spi slave | lisa_spi |
| i2c、i2c master、从机扫描 | lisa_i2c |
| flash、nor flash、读写擦除 | lisa_flash |
| adc、模数转换、采样 | lisa_adc |
| pwm、占空比、脉宽调制 | lisa_pwm |
| hwtimer、硬件定时器、gpt、aon timer | lisa_hwtimer |
| rtc、实时时钟、闹钟 | lisa_rtc |
| wdt、watchdog、看门狗 | lisa_wdt |
| sdmmc、sd卡、mmc、tf卡 | lisa_sdmmc |
| i2s、音频接口、pcm | lisa_i2s |
| display、显示屏、lcd、lvgl | lisa_display |
| touch、触摸、触控 | lisa_touch |
| audio、音频、录音、播放、codec、aec | lisa_audio |
| qspilcd、qspi lcd、四线 lcd | lisa_qspilcd |
| dvp、摄像头接口 | lisa_dvp |
| camera、摄像头、sensor、拍照 | lisa_camera |
| rgb、led灯带、ws2812、sk6812 | lisa_rgb |
| pinmux、引脚复用、iomux | pinmux |
| wifi、无线网络、SSID、密码、连网 | lisa_wifi |
| bluetooth、ble、蓝牙 | lisa_bluetooth |
| http、https、REST、下载、POST、GET | lisa_http |
| websocket、ws、wss、实时通信 | lisa_websocket |
| mqtt、消息队列、物联网云、发布订阅 | coreMQTT |
| sntp、ntp、时间同步、网络时间 | lisa_sntp |
| 网络设备、netdev、socket、TCP/IP、lwip | lisa_net |
| 4g、蜂窝、modem、AT指令、ML307 | lisa_modem |
| wifi管理、自动连接、AP存储 | wifi_manager |
| 日志、log、打印、调试输出 | lisa_log |
| shell、命令行、CLI、调试控制台 | lisa_shell |
| 事件、发布订阅、回调解耦 | lisa_evt_pub |
| 内存分配、堆、malloc、PSRAM、SRAM | sys_heap |
| 环形缓冲、ringbuf、FIFO | ringbuf |
| 工作队列、异步任务、延迟执行 | work_queue |
| kv、键值存储、持久化配置 | lisa_kv |
| 录音、麦克风、音频输入 | lite_adc |
| 音频播放、扬声器、DAC输出 | lite_dac |
| 播放器、音乐播放 | app_player |
| json、cjson、解析 | cjson |
| lvgl、GUI、图形界面、控件 | lvgl |
| 加密、tls、ssl、证书 | mbedtls |
| sqlite、数据库、SQL | csk_sqlite3 |
| freertos、任务、信号量、互斥锁、队列 | lisa_os |
| usb、adb | tinyusb |
| 文件系统、fatfs、lsfs | filesystem |

### 动态发现（静态表未命中时）

当用户输入的关键词未命中上述静态表时，按以下顺序搜索：

1. `Glob "components/*{keyword}*/"` — 搜索组件目录名
2. `Glob "drivers/*{keyword}*/"` — 搜索驱动目录名
3. `Glob "modules/*{keyword}*/"` — 搜索第三方模块目录名
4. `Grep "{keyword}" components/*/CMakeLists.txt` — 按 CMake target 名搜索

结果处理：
- 唯一匹配 → 使用该模块
- 多个匹配 → 向用户展示候选列表，确认目标
- 无匹配 → 告知用户 SDK 中未找到相关模块

识别到模块后（无论静态匹配还是动态发现）：`Read references/experience/{module}.md`（如存在）。多模块场景加载所有相关文件。

### 判断规则

- 优先匹配精确关键词；不明确时再看当前文件、分支名或 `git diff`
- 区分“解释/确认”与“产物/交付物”：前者可考虑直接回答，后者优先路由 worker
- 凡需生产可复用内容（示例、demo、sample、完整代码、文件/结构/初始化链路），直接回答不可用
- 多领域交叉时按依赖顺序串联 worker（如 driver-dev → sample-gen）
- 若用户目标是 `arcs_evb` 真板调试，优先路由到 `references/build-debug.md` 以独立处理编译、烧录、抓日志等指令
- 实在判断不了，就向用户确认

### 多模块协同

识别到多个模块时，判断模块间关系：

1. **有依赖关系**（如 HTTP 依赖 WiFi）：
   - 按依赖顺序排列：底层 → 上层
   - 执行 `references/dependency-resolver.md` 获取完整初始化链
   - 生成代码时包含全链路初始化

2. **无依赖关系**（如 GPIO + UART 同时使用）：
   - 分别处理，合并到同一个 main.c

3. **常见依赖链路**（快速判断用，不确定时执行 dependency-resolver）：
   - 网络类：WiFi/4G → lisa_net → lisa_http / lisa_websocket / coreMQTT
   - 音频类：lite_adc / lite_dac → app_player
   - 显示类：lisa_display → lvgl

## 直接回答（跳过 worker）

只在**全部**条件成立时才直接回答，跳过 worker：

- 请求属于“解释/确认/简短用法”，而非“示例/demo/sample/实现/文件产物”
- 不需文件/结构规划、完整函数集、初始化链路、CMake/Kconfig 配套，或工程级组织与 sample 对照
- 一段简短代码或说明就能完整覆盖用户需求
- 不涉及新文件生成
- 对相关 API 的用法有充分把握

以下情况必须转 worker，不能直接回答：

- 需要可复用示例、demo、sample、完整代码或文件级产出
- 要求 SDK 风格对齐、sample 参考、工程结构、初始化链路或配套构建配置
- 表面上看似示例代码，实则在请求完整交付物

不确定 API 用法时，先 `Read` 对应头文件再回复。

## 执行注意事项

- 代码生成前先 `Glob/Grep` 检索 codebase 中的现有实现作为参考
- 回答中引用具体文件路径和行号
- worker 任务完成后，评估是否有值得记录的经验 → 如有，调用 `experience-capture` skill
- 加载 experience 文件后，若条目引用了具体文件路径，快速 `Glob` 验证路径是否仍存在。路径不存在 → 跳过该条目，回答末尾提示："注意：experience 中的 [条目标题] 引用的路径已变更，建议运行知识维护（`evolution.md` 模式 B）更新。"
