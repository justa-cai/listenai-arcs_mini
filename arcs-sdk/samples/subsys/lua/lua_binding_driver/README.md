# Lua 脚本引擎示例

## 功能说明

本示例集成了 Lua 5.4 脚本引擎，提供三种使用方式：

1. **交互式 REPL** — 通过串口 shell 输入 `lua` 命令进入交互解释器，逐行执行 Lua 代码
2. **脚本文件执行** — 通过 USB MSC 将 `.lua` 脚本拷贝到 SD 卡，再用 `luarun` 命令执行
3. **LISA 驱动 Lua C API 绑定** — Lua 脚本可直接调用 ARCS SDK 驱动接口（GPIO、PWM、ADC、UART 等），无需编写 C 代码

示例内附 `scripts/blink.lua`，演示通过 Lua 脚本控制 GPIO 翻转 LED。

## 硬件连接

- **SDMMC0 接口**：连接 SD 卡（用于存储 Lua 脚本文件）
- **USB 接口**：连接电脑（设备作为 USB 大容量存储出现，可直接拷贝脚本）
- **串口**：连接终端（用于 shell 交互和日志输出）
- **LED**（可选）：arcs_evb 板上 PB9 连接 LED，用于运行 `blink.lua` 示例

## 示例步骤

1. 编译并烧录固件
2. 通过 USB 将设备连接电脑，设备会识别为 U 盘
3. 将 `scripts/blink.lua` 拷贝到 U 盘根目录
4. 打开串口终端，连接设备
5. 在 shell 中执行脚本或进入交互模式

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 烧录

```{eval-rst}
.. include:: /sample_flash.rst
```

## Shell 命令

| 命令 | 说明 |
|------|------|
| `lua` | 进入交互式 Lua 解释器，输入 `exit` 或 Ctrl-D 退出 |
| `luarun <file>` | 执行 SD 卡上的 Lua 脚本，如 `luarun blink.lua` |
| `luals [dir]` | 列出 SD 卡上的文件 |

## 预期输出

### 列出文件并执行脚本

```
letter:/$ luals
Files in /SD:/:
  231322346  default.avi
       190  blink.lua
  <DIR>  roms

letter:/$ luarun blink.lua
Running /SD:/blink.lua (190 bytes)...
Blinking LED on PB9 ... (Ctrl-C to stop)
LED ON
LED OFF
LED ON
LED OFF
...
Done.
```

### 交互式 REPL

```
letter:/$ lua
Lua 5.4.4  Copyright (C) 1994-2022 Lua.org, PUC-Rio
Type 'exit' or Ctrl-D to quit.
> print("hello")
hello
> 1 + 2
3
> local dev = lisa.device("gpiob")
> lisa.gpio.configure(dev, 9, lisa.gpio.LISA_GPIO_OUTPUT)
0
> lisa.gpio.write_pin(dev, 9, 1)
0
> exit
```

## Lua 驱动 API 参考

所有驱动 API 通过全局表 `lisa` 访问。

### 通用函数

| 函数 | 说明 |
|------|------|
| `lisa.device(name)` | 获取设备句柄（lightuserdata），如 `lisa.device("gpiob")` |
| `lisa.device_ready(dev)` | 检查设备是否就绪，返回 `true`/`false` |
| `lisa.msleep(ms)` | 延时指定毫秒数（基于 FreeRTOS vTaskDelay） |

### 驱动模块

| 模块 | 主要函数 | 对应 C API |
|------|---------|-----------|
| `lisa.gpio` | `configure`, `read_pin`, `write_pin`, `enable_irq`, `disable_irq` | `lisa_gpio_*` |
| `lisa.pwm` | `configure`, `enable`, `disable`, `set`, `get_config` | `lisa_pwm_*` |
| `lisa.adc` | `channel_setup`, `read` | `lisa_adc_*` |
| `lisa.uart` | `configure`, `get_config`, `rx_enable`, `rx_disable`, `write_sync`, `read_sync`, `poll_in`, `poll_out` | `lisa_uart_*` |
| `lisa.spi` | `configure`, `transfer`, `write`, `read`, `get_config` | `lisa_spi_*` |
| `lisa.i2c` | `configure`, `transfer`, `write`, `read`, `get_config` | `lisa_i2c_*` |
| `lisa.flash` | `erase`, `sr_read`, `sr_write`, `get_parameters`, `page_layout` | `lisa_flash_*` |
| `lisa.wdt` | `setup`, `start`, `stop`, `feed`, `get_remaining_time`, `get_state` | `lisa_wdt_*` |
| `lisa.rtc` | `set_time`, `get_time`, `set_alarm`, `get_alarm`, `enable_alarm`, `set_periodic_int`, `get_capabilities` | `lisa_rtc_*` |
| `lisa.hwtimer` | `get_capabilities`, `set_frequency`, `start`, `stop`, `reset`, `get_value` | `lisa_hwtimer_*` |

### 调用约定

- 第一个参数始终是 `lisa.device()` 返回的设备句柄
- 返回值第一个是 C API 返回码（0 = 成功，负值 = 错误）
- 带输出参数的函数以多返回值形式返回：`ret, value = lisa.adc.read(dev, ch)`
- 结构体参数使用 Lua table 传入和返回：

```lua
-- 输入：table → C struct
lisa.wdt.setup(dev, {int_timeout_ms = 5000, rst_timeout_ms = 1000})

-- 输出：C struct → table
local ret, time = lisa.rtc.get_time(dev)
print(time.year, time.month, time.day, time.hour, time.minute, time.second)
```

- 每个模块表内附带对应的常量（枚举值、宏定义），如 `lisa.gpio.LISA_GPIO_OUTPUT`

## 示例脚本 — LED 闪烁

`scripts/blink.lua` 演示通过 Lua 操作 GPIO PB9 翻转 LED：

```lua
local gpio = lisa.gpio
local dev = lisa.device("gpiob")

local LED_PIN = 9

-- 配置 PB9 为输出，初始低电平
gpio.configure(dev, LED_PIN,
    gpio.LISA_GPIO_OUTPUT | gpio.LISA_GPIO_OUTPUT_INIT_LOW)

-- 闪烁 20 次
local led_on = true
for i = 1, 20 do
    if led_on then
        gpio.write_pin(dev, LED_PIN, gpio.LISA_GPIO_HIGH)
    else
        gpio.write_pin(dev, LED_PIN, gpio.LISA_GPIO_LOW)
    end
    led_on = not led_on
    lisa.msleep(500)
end

-- 关闭 LED
gpio.write_pin(dev, LED_PIN, gpio.LISA_GPIO_LOW)
```

## Lua C API 绑定生成原理

本示例使用标准 Lua 5.4 C API 实现驱动绑定，不依赖 LuaJIT，也不使用 LuaJIT 的 `ffi` 模块。驱动绑定代码由 `gen_lua_bindings.py` 在 CMake 构建阶段自动生成，无需手动维护。

### 工作流程

```
lisa_*.h 头文件                  CMake configure
      |                               |
      v                               v
gen_lua_bindings.py  ------->  build/bindings/
  解析 static inline 函数             |
  提取参数类型和常量              lua_lisa_gpio.c
  生成 Lua C API binding          lua_lisa_pwm.c
                                  lua_lisa_all.h
                                      ...
                                      |
                                      v
                               编译链接到固件
```

### 解析规则

脚本解析 `drivers/lisa_*/lisa_*.h` 中的 `static inline` 函数声明，按参数类型自动分类处理：

| C 参数类型 | Lua 侧 | 处理方式 |
|-----------|---------|---------|
| `lisa_device_t *dev` | `lightuserdata` | 由 `lisa.device()` 获取 |
| `uint32_t`, `int`, `uint8_t` 等标量 | `integer` | `luaL_checkinteger` |
| `bool` | `boolean` | `lua_toboolean` |
| `const lisa_xxx_config_t *` | `table` | 逐字段读取 table |
| `uint32_t *`（输出参数） | 多返回值 | 声明局部变量，传地址，push 返回 |
| 结构体输出指针 | `table` | `memset` + 逐字段写入 table |
| 回调函数指针 | — | 跳过，不生成绑定 |
| `void *` / `const void *` 缓冲区 | — | 跳过，不生成绑定 |

### 常量导出

脚本同时提取头文件中的：
- `typedef enum { ... }` 枚举值
- `#define LISA_*` 整型宏定义

注册为每个模块 table 的字段，Lua 中可直接使用如 `lisa.gpio.LISA_GPIO_OUTPUT`。

### 自定义与扩展

- **添加新驱动**：在 `gen_lua_bindings.py` 的 `TARGET_DRIVERS` 列表和 `CMakeLists.txt` 的 `LISA_DRIVER_MODULES` 中添加模块名
- **重新生成**：构建系统会在生成脚本或已支持的驱动头文件变化时自动重新生成；如遇旧构建目录缓存异常，可执行 `./build.sh -C` 清理构建
- **手动运行**：`python3 gen_lua_bindings.py --drivers-dir ../../drivers --output-dir /tmp/bindings`

## 配置说明

`prj.conf` 中的关键配置项：

| 配置项 | 说明 |
|--------|------|
| `CONFIG_LISA_SHELL=y` | 启用 shell 命令行 |
| `CONFIG_TINY_USB=y` | 启用 TinyUSB |
| `CONFIG_DISK_DRIVER_SDMMC=y` | 启用 SDMMC 磁盘驱动 |
| `CONFIG_FILE_SYSTEM=y` | 启用文件系统 |
| `CONFIG_FATFS_FILESYSTEM=y` | 启用 FAT 文件系统 |
| `CONFIG_LSFS=y` | 启用 LSFS 抽象层 |
| `CONFIG_LVFS=y` | 启用 LVFS 虚拟文件系统 |

## 目录结构

```
samples/modules/lua/
├── CMakeLists.txt          # 构建配置，含自动生成逻辑
├── gen_lua_bindings.py     # Lua C API 绑定生成脚本
├── prj.conf                # 项目 Kconfig 配置
├── scripts/
│   └── blink.lua           # LED 闪烁示例脚本
├── src/
│   ├── main.c              # 主程序（shell 命令、USB MSC、文件系统）
│   ├── tusb_config.h       # TinyUSB 配置
│   └── usb_descriptors.c   # USB 设备描述符
└── components/
    ├── lua/                 # Lua 5.4 源码
    └── port/               # 平台移植层
```

## 注意事项

1. 首次使用需将 SD 卡插入设备，固件启动时会自动挂载（挂载失败会格式化）
2. USB MSC 和文件系统共享 SD 卡，USB 拷贝文件后建议安全弹出再执行脚本
3. Lua 脚本最大支持 32 KB，超出此大小需修改 `LUA_SCRIPT_MAXLEN`
4. 带回调参数的驱动函数（如 `lisa_gpio_configure_irq`）不会生成 Lua 绑定，需在 C 侧完成配置；不带回调的 `enable_irq` / `disable_irq` 仍可在 Lua 中调用
5. 交互式 REPL 中输入表达式会自动打印结果值（内部尝试 `return <expr>` 求值）
