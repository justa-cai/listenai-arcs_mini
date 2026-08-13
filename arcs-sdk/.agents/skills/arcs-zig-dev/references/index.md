<!-- type: meta -->

# Zig 开发助手系统全景图

AI 读此文件即可了解整个 Zig skill 体系的结构、文件职责和加载时机。

---

## 文件角色定义

| 角色 | 说明 |
|------|------|
| `[ROUTER]` | 意图识别 + 模块识别 + 路由，自身不产生实质输出 |
| `[WORKER]` | 执行特定领域任务，产生代码/文档等输出 |
| `[KNOWLEDGE]` | 稳定的架构知识和 API 参考 |
| `[EXPERIENCE]` | 按模块积累的使用经验，由 worker 按需加载 |

---

## 入口层

| 文件 | 角色 | 职责 |
|------|------|------|
| `SKILL.md` | `[ROUTER]` | 意图识别 + 模块识别，路由到对应 worker |

---

## Worker 层

按需读取，不预加载。

| 文件 | 角色 | 职责 |
|------|------|------|
| `references/zig-bindings.md` | `[WORKER]` | 新增/修改 C FFI 绑定和 HAL 封装 |
| `references/zig-sample-gen.md` | `[WORKER]` | 生成 Zig 示例项目 |
| `references/zig-build-debug.md` | `[WORKER]` | Zig 构建系统配置和编译故障诊断 |

---

## Knowledge 层

| 文件 | 角色 | 职责 |
|------|------|------|
| `references/zig-api-ref.md` | `[KNOWLEDGE]` | Zig adapter 完整 API 参考和用法速查 |

---

## Experience 层

按模块一个文件，使用中持续积累。Worker 按需加载。

| 目录 | 角色 | 说明 |
|------|------|------|
| `references/experience/*.md` | `[EXPERIENCE]` | 如 `gpio.md`、`uart.md`，每条经验带场景、日期 |

---

## 关联 Skill

| Skill | 位置 | 关系 |
|-------|------|------|
| `sdk-assistant-agent` | `.agents/skills/sdk-assistant-agent/` | C SDK 开发助手，Zig 绑定需参考其驱动开发和构建调试知识 |

---

## Zig Adapter 目录结构

```
labs/zig/adapter/
├── build.zig                  # Zig 构建系统入口（RISC-V 交叉编译）
├── build.zig.zon              # 包管理配置
├── CMakeLists.txt             # CMake 集成（SDK 原有构建系统）
├── src/
│   ├── root.zig               # 模块根入口（pub const 导出所有 API）
│   ├── bindings/              # C FFI 1:1 映射层 (21 个模块)
│   │   ├── c_types.zig        # 基础类型、错误码、优先级
│   │   ├── mem.zig            # lisa_mem_alloc/free
│   │   ├── device.zig         # lisa_device_get/ready
│   │   ├── gpio.zig           # GPIO vtable + 辅助函数
│   │   ├── uart.zig           # UART vtable + 辅助函数
│   │   ├── i2c.zig            # I2C vtable
│   │   ├── spi.zig            # SPI vtable
│   │   ├── adc.zig            # ADC vtable
│   │   ├── pwm.zig            # PWM vtable
│   │   ├── flash.zig          # Flash vtable
│   │   ├── display.zig        # Display vtable + RGB565 颜色
│   │   ├── rtc.zig            # RTC vtable
│   │   ├── audio.zig          # Audio vtable
│   │   ├── bluetooth.zig      # BLE/Classic 回调和函数
│   │   ├── wifi.zig           # WiFi 初始化
│   │   ├── thread.zig         # lisa_thread_create/delete
│   │   ├── sync.zig           # Mutex/Semaphore/Queue
│   │   ├── timer.zig          # lisa_timer_create/start
│   │   ├── log.zig            # lisa_log 后端
│   │   ├── console.zig        # console_write + printk
│   │   └── ringbuf.zig        # ring_buf 操作
│   └── hal/                   # 惯用 Zig API 封装层 (19 个模块)
│       ├── allocator.zig      # std.mem.Allocator (psram/sram)
│       ├── log.zig            # scoped 日志 + console_write 输出
│       ├── thread.zig         # Thread.spawn (泛型参数传递)
│       ├── sync.zig           # Mutex + Semaphore + Channel(T)
│       ├── timer.zig          # Timer.periodic / Timer.once
│       ├── device.zig         # Device.get / forEach
│       ├── gpio.zig           # Gpio.open / configOutput / toggle
│       ├── uart.zig           # Uart.open / writer() / reader()
│       ├── i2c.zig            # I2c.open / readReg / writeReg
│       ├── spi.zig            # Spi.open / transferFull
│       ├── adc.zig            # Adc.open / read / toMillivolts
│       ├── pwm.zig            # Pwm.open / set / enable
│       ├── flash.zig          # Flash.open / read / write / erase
│       ├── display.zig        # Display.open / fillRect / setBrightness
│       ├── rtc.zig            # Rtc.open / getTime / setAlarm
│       ├── audio.zig          # Audio.open / startRecord / startPlay
│       ├── bluetooth.zig      # Bluetooth.init / startDiscovery
│       ├── wifi.zig           # WiFi.init
│       └── ringbuf.zig        # RingBuffer.init / put / get (零拷贝)
├── examples/                  # 示例程序
│   ├── helloworld.zig
│   ├── blinky.zig
│   └── uart_echo.zig
└── tests/                     # 单元测试（主机运行）
    ├── test_types.zig
    ├── test_gpio_flags.zig
    ├── test_adc_convert.zig
    ├── test_uart_config.zig
    └── test_struct_layout.zig
```

---

## 路径说明

所有相对路径均以 SDK 根目录为基准。Zig adapter 位于 `labs/zig/adapter/` 子目录，源码已直接纳入主仓维护。
