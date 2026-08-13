---
name: arcs-zig-dev
description: >
  ARCS SDK Zig 语言开发助手。当用户在此 SDK 工程中进行 Zig 语言相关开发时触发，包括但不限于：
  Zig 绑定开发（新增驱动绑定、FFI 映射、vtable 虚表、extern struct）、
  Zig 样例编写（Zig 示例、blinky、uart_echo、helloworld）、
  Zig 构建调试（zig build、交叉编译 RISC-V、CMake 集成、链接错误）、
  Zig HAL 封装（Allocator、Thread、Mutex、Channel、GPIO/UART/SPI/I2C）、
  Zig API 使用（arcs.Gpio.open、arcs.Uart、arcs.log、arcs.allocator）。
  凡涉及 .zig 文件、build.zig、labs/zig/adapter/ 目录、@import("arcs") 的问题，均应触发此 skill。
compatibility: Designed for Claude Code in LISTENAI ARCS SDK repositories with Zig adapter
allowed-tools: Read Glob Grep Bash Edit Write
---

# Zig 开发助手

> 系统全景图：`references/index.md`
> Zig adapter 根目录：`labs/zig/adapter/`（相对 SDK 根目录，源码已直接纳入主仓维护，不再作为 git 子模块）

## 意图识别与路由

分析用户输入，匹配领域和模块，读取对应 worker 文件执行：

### 领域路由

| 领域 | 触发信号 | 读取并执行 |
|------|----------|------------|
| **绑定开发** | 新增绑定、FFI、extern、c_int、vtable、驱动绑定、bindings、新增组件支持 | `references/zig-bindings.md` |
| **样例生成** | 写 Zig 示例、zig demo、zig sample、zig example、zig blinky、zig hello | `references/zig-sample-gen.md` |
| **构建调试** | zig build、编译 zig、RISC-V 交叉编译、链接错误、CMake 集成、build.zig | `references/zig-build-debug.md` |
| **HAL 封装** | 封装 GPIO、包装 UART、Allocator、Thread、Mutex、Channel、惯用 Zig API | `references/zig-bindings.md` |
| **API 使用** | 怎么用 arcs.Gpio、arcs.log、arcs.Uart 用法、Zig 接口说明 | `references/zig-api-ref.md` |

### 模块识别

识别用户输入中涉及的 Zig 模块，自动定位相关源文件：

| 关键词 | Zig 模块路径 | C 绑定 |
|--------|-------------|--------|
| gpio、引脚、led、按键 | `labs/zig/adapter/src/hal/gpio.zig` | `labs/zig/adapter/src/bindings/gpio.zig` |
| uart、串口、serial | `labs/zig/adapter/src/hal/uart.zig` | `labs/zig/adapter/src/bindings/uart.zig` |
| spi、spi master | `labs/zig/adapter/src/hal/spi.zig` | `labs/zig/adapter/src/bindings/spi.zig` |
| i2c、i2c master | `labs/zig/adapter/src/hal/i2c.zig` | `labs/zig/adapter/src/bindings/i2c.zig` |
| adc、模数转换 | `labs/zig/adapter/src/hal/adc.zig` | `labs/zig/adapter/src/bindings/adc.zig` |
| pwm、占空比 | `labs/zig/adapter/src/hal/pwm.zig` | `labs/zig/adapter/src/bindings/pwm.zig` |
| flash、nor flash | `labs/zig/adapter/src/hal/flash.zig` | `labs/zig/adapter/src/bindings/flash.zig` |
| display、lcd、显示屏 | `labs/zig/adapter/src/hal/display.zig` | `labs/zig/adapter/src/bindings/display.zig` |
| audio、录音、播放 | `labs/zig/adapter/src/hal/audio.zig` | `labs/zig/adapter/src/bindings/audio.zig` |
| bluetooth、ble、蓝牙 | `labs/zig/adapter/src/hal/bluetooth.zig` | `labs/zig/adapter/src/bindings/bluetooth.zig` |
| wifi、无线网络 | `labs/zig/adapter/src/hal/wifi.zig` | `labs/zig/adapter/src/bindings/wifi.zig` |
| rtc、实时时钟 | `labs/zig/adapter/src/hal/rtc.zig` | `labs/zig/adapter/src/bindings/rtc.zig` |
| thread、线程、任务 | `labs/zig/adapter/src/hal/thread.zig` | `labs/zig/adapter/src/bindings/thread.zig` |
| mutex、互斥锁 | `labs/zig/adapter/src/hal/sync.zig` | `labs/zig/adapter/src/bindings/sync.zig` |
| semaphore、信号量 | `labs/zig/adapter/src/hal/sync.zig` | `labs/zig/adapter/src/bindings/sync.zig` |
| channel、队列、消息 | `labs/zig/adapter/src/hal/sync.zig` | `labs/zig/adapter/src/bindings/sync.zig` |
| timer、定时器 | `labs/zig/adapter/src/hal/timer.zig` | `labs/zig/adapter/src/bindings/timer.zig` |
| allocator、内存、psram、sram | `labs/zig/adapter/src/hal/allocator.zig` | `labs/zig/adapter/src/bindings/mem.zig` |
| log、日志、打印 | `labs/zig/adapter/src/hal/log.zig` | `labs/zig/adapter/src/bindings/log.zig` |
| ringbuf、环形缓冲 | `labs/zig/adapter/src/hal/ringbuf.zig` | `labs/zig/adapter/src/bindings/ringbuf.zig` |
| device、设备框架 | `labs/zig/adapter/src/hal/device.zig` | `labs/zig/adapter/src/bindings/device.zig` |
| console、控制台 | — | `labs/zig/adapter/src/bindings/console.zig` |

### 动态发现（静态表未命中时）

当用户输入的关键词未命中上述静态表时，按以下顺序搜索：

1. `Glob "labs/zig/adapter/src/bindings/*{keyword}*.zig"` — 搜索绑定文件
2. `Glob "labs/zig/adapter/src/hal/*{keyword}*.zig"` — 搜索 HAL 封装文件
3. `Grep "{keyword}" labs/zig/adapter/src/root.zig` — 在根模块中搜索
4. `Glob "drivers/*{keyword}*/lisa_*.h"` — 回退到 C SDK 头文件

结果处理：
- 唯一匹配 → 使用该模块
- 多个匹配 → 向用户展示候选列表，确认目标
- 无匹配 → 告知用户当前 Zig adapter 未覆盖该模块，引导新增绑定流程

识别到模块后：`Read` 对应的 HAL 和 bindings 文件。如存在 experience 文件：`Read references/experience/{module}.md`。

### 判断规则

- 优先匹配精确关键词；不明确时看当前文件后缀（`.zig` → Zig 上下文）或 `git diff` 涉及 `labs/zig/adapter/` 目录
- 区分"解释/确认"与"产物/交付物"：前者可考虑直接回答，后者优先路由 worker
- 涉及新增 C API 绑定时，**必须先 Read 对应的 C 头文件**再生成绑定
- 多领域交叉时按依赖顺序串联 worker（如 zig-bindings → zig-sample-gen）
- Zig 构建问题先路由 `zig-build-debug.md`；若涉及 C SDK 编译则额外参考 `sdk-assistant-agent` 的 `build-debug.md`

### 三层架构感知

Zig adapter 采用三层架构，理解请求所在层级至关重要：

```
用户代码 / 示例 (examples/)
    ↓ 使用
HAL 层 (src/hal/)     — 惯用 Zig API，类型安全，error union
    ↓ 调用
Bindings 层 (src/bindings/) — C FFI 1:1 映射，extern "c" fn
    ↓ 链接
ARCS SDK (C)          — lisa_* API，FreeRTOS
```

- 用户问"怎么用 GPIO" → HAL 层 API → `labs/zig/adapter/src/hal/gpio.zig`
- 用户问"新增 watchdog 绑定" → Bindings 层 → 需先 Read C 头文件
- 用户问"编译不过" → 构建系统 → `labs/zig/adapter/build.zig`

## 直接回答（跳过 worker）

只在**全部**条件成立时才直接回答：

- 请求属于"解释/确认/简短用法"，而非"示例/绑定/实现/文件产物"
- 不需新建 `.zig` 文件
- 一段简短代码或说明就能完整覆盖用户需求
- 对相关 Zig API 的用法有充分把握

以下情况必须转 worker，不能直接回答：

- 需要新增或修改 bindings/HAL 文件
- 需要生成完整的 Zig 示例项目
- 涉及 build.zig 修改或 CMake 集成变更
- 需要新增对 C SDK 组件的 Zig 支持

不确定 API 用法时，先 `Read` 对应 HAL/bindings `.zig` 文件和 C 头文件再回复。

## 执行注意事项

- 新增绑定前必须 `Read` 对应的 C 头文件（`drivers/lisa_{xxx}/lisa_{xxx}.h`），确保类型映射准确
- Zig 0.13 中 `c_int` / `c_uint` 是内置原语，**不可重新声明**
- `extern struct` 字段顺序必须与 C struct 完全一致
- 所有驱动 API 使用 vtable (函数指针结构体) 模式分发
- HAL 层应将 C 错误码映射为 Zig error set，支持 `try`/`catch`
- 回答中引用具体文件路径和行号
- 代码生成后执行 `cd labs/zig/adapter && zig build --summary all` 验证编译
- worker 任务完成后，评估是否有值得记录的经验
