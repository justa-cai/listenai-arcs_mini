# Logger 组件

## 概述

`lisa_log` 是 ARCS SDK 的日志管理组件，提供统一的日志输出接口。该组件基于 EasyLogger 库实现，当前通过 UART 后端输出日志到串口，支持异步输出、多级别过滤等特性。

### 主要特性

- **基于 EasyLogger**：使用成熟的 EasyLogger 库进行日志格式化和管理
- **UART 串口输出**：通过 syslog 将日志输出到 UART 串口
- **异步日志输出**：支持异步模式，避免阻塞应用程序，提高系统性能
- **日志级别控制**：支持多级别日志过滤（ERROR、WARN、INFO、DEBUG、VERBOSE）
- **彩色输出**：支持终端彩色日志输出，便于区分不同级别
- **线程安全**：支持多任务环境下的并发日志输出
- **灵活配置**：通过 `prj.conf` 文件提供丰富的配置选项

> **注意**：当前版本仅支持 UART 后端，未来版本可能会支持其他后端类型（如文件、网络等）。

## 快速开始

### 1. 启用日志组件

在项目的 `prj.conf` 文件中添加以下配置：

```ini
CONFIG_LOG=y
```

### 2. 基本使用

```c
// 重要：LOG_TAG 必须在包含头文件之前定义
#define LOG_TAG "MyApp"
#include "lisa_log.h"

void app_main(void) {
    // 日志系统已在启动时自动初始化，直接使用即可
    LOGI("Application started, version: %s", "1.0.0");
    LOGD("Debug mode enabled");
    LOGW("This is a warning message");
    LOGE("Error code: %d", -1);
}
```

## 配置选项

在项目的 `prj.conf` 文件中配置日志组件的各项参数。

### 1. 启用日志组件

| 配置项 | 说明 | 默认值 |
|--------|------|--------|
| `CONFIG_LOG` | 使能日志组件 | n |

在 `prj.conf` 中添加：

```ini
CONFIG_LOG=y
```

启用后，系统会自动使用 EasyLogger 作为日志前端，UART 作为日志后端。

### 2. 日志级别

日志级别从严重到详细依次为：

| 级别 | 配置项 | 说明 | 使用场景 |
|------|--------|------|---------|
| ASSERT | `CONFIG_LOG_LEVEL_ASSERT` | 断言级别 | 致命错误，程序无法继续运行 |
| ERROR | `CONFIG_LOG_LEVEL_ERROR` | 错误级别 | 严重错误，功能受影响 |
| WARN | `CONFIG_LOG_LEVEL_WARN` | 警告级别 | 潜在问题，不影响正常运行 |
| INFO | `CONFIG_LOG_LEVEL_INFO` | 信息级别（默认） | 一般信息，程序运行状态 |
| DEBUG | `CONFIG_LOG_LEVEL_DEBUG` | 调试级别 | 调试信息，开发阶段使用 |
| VERBOSE | `CONFIG_LOG_LEVEL_VERBOSE` | 详细级别 | 最详细的日志，包含所有信息 |

**配置方式**：

在 `prj.conf` 中选择一个日志级别（只能选择一个）：

```ini
# 选择信息级别（默认，推荐生产环境）
CONFIG_LOG_LEVEL_INFO=y

# 或者选择其他级别（根据需要）
# CONFIG_LOG_LEVEL_ASSERT=y
# CONFIG_LOG_LEVEL_ERROR=y
# CONFIG_LOG_LEVEL_WARN=y
# CONFIG_LOG_LEVEL_DEBUG=y     # 开发调试
# CONFIG_LOG_LEVEL_VERBOSE=y   # 最详细
```

**说明**：
- 选择某个级别后，该级别及以上的日志都会被输出
- 例如选择 `CONFIG_LOG_LEVEL_INFO=y`，则 ASSERT、ERROR、WARN、INFO 级别的日志会输出，DEBUG 和 VERBOSE 不会输出
- 建议生产环境使用 `INFO` 或 `WARN`，开发调试时使用 `DEBUG` 或 `VERBOSE`

### 3. 异步模式配置

异步模式可以避免日志输出阻塞应用程序，提高系统响应性能。

| 配置项 | 说明 | 默认值 | 调整建议 |
|--------|------|--------|---------|
| `CONFIG_LOG_MODE_ASYNC` | 使能异步模式 | y | 建议保持开启 |
| `CONFIG_LOG_ASYNC_BUF_SIZE` | 异步缓冲区大小（字节） | 10240 | 日志量大时可增加 |
| `CONFIG_LOG_ASYNC_TASK_STACK_SIZE` | 异步任务栈大小（字节） | 512 | 栈溢出时增加 |
| `CONFIG_LOG_ASYNC_TASK_PRIORITY` | 异步任务优先级 | 1 | 根据系统调度需求调整 |

**配置方式**：

在 `prj.conf` 中添加（使用默认值）：

```ini
CONFIG_LOG_MODE_ASYNC=y
```

或者自定义异步模式参数：

```ini
CONFIG_LOG_MODE_ASYNC=y
CONFIG_LOG_ASYNC_BUF_SIZE=20480           # 增加缓冲区到 20KB
CONFIG_LOG_ASYNC_TASK_STACK_SIZE=1024     # 增加栈大小到 1KB
CONFIG_LOG_ASYNC_TASK_PRIORITY=3          # 提高任务优先级
```

如需使用同步模式（不推荐）：

```ini
# CONFIG_LOG_MODE_ASYNC=n    # 注释掉或设为 n
```

**工作原理**：
- 应用程序调用 `LOGI()` 等宏时，日志数据会先写入缓冲区，立即返回
- 后台任务从缓冲区读取日志数据，通过 UART 串口输出
- 缓冲区满时，新的日志会被丢弃（建议增加缓冲区大小）

**异步模式 vs 同步模式**：

| 模式 | 优点 | 缺点 | 适用场景 |
|------|------|------|---------|
| 异步（推荐） | 不阻塞应用，性能高 | 可能丢失日志（缓冲区满） | 一般应用场景 |
| 同步 | 不丢失日志 | 阻塞应用，性能低 | 调试关键问题 |

### 4. 高级配置

| 配置项 | 说明 | 默认值 | 调整建议 |
|--------|------|--------|---------|
| `CONFIG_LOG_LINE_BUF_SIZE` | 单行日志缓冲区大小（字节） | 1024 | 单条日志过长时增加 |
| `CONFIG_LOG_USE_POSIX_TIME` | 使用 POSIX 时间戳 | n | 需要精确时间时启用 |
| `CONFIG_LOG_BACKEND_SYS_NAME` | UART 后端名称 | "sys.log" | 一般不需要修改 |

**配置方式**：

在 `prj.conf` 中添加（可选）：

```ini
# 增加单行日志缓冲区大小
CONFIG_LOG_LINE_BUF_SIZE=2048

# 启用 POSIX 时间戳（需要系统支持）
CONFIG_LOG_USE_POSIX_TIME=y

# 修改 UART 后端名称（一般不需要）
CONFIG_LOG_BACKEND_SYS_NAME="sys.log"
```

### 5. UART 后端配置（syslog）

日志组件通过系统 syslog 接口将日志输出到 UART 串口。syslog 的 UART 配置需要在 `prj.conf` 中设置。

#### 5.1 选择 UART 设备

在 `prj.conf` 中选择用于日志输出的 UART 设备（只能选择一个）：

```ini
# 选择 UART0（默认）
CONFIG_SYSLOG_UART_DEVICE_UART0=y

# 或选择 UART1
# CONFIG_SYSLOG_UART_DEVICE_UART1=y

# 或选择 UART2
# CONFIG_SYSLOG_UART_DEVICE_UART2=y
```

#### 5.2 配置波特率

在 `prj.conf` 中设置 UART 波特率：

```ini
# 设置波特率（默认 921600）
CONFIG_SYSLOG_UART_BAUDRATE=921600

# 常用波特率选项：
# CONFIG_SYSLOG_UART_BAUDRATE=115200
# CONFIG_SYSLOG_UART_BAUDRATE=460800
# CONFIG_SYSLOG_UART_BAUDRATE=921600
```

**说明**：
- 波特率越高，日志输出越快，但对硬件和连接质量要求越高
- 建议使用 921600 或 115200
- 确保串口工具的波特率设置与此配置一致

### 6. printf 和 printk 输出配置

系统提供了 `printf` 和 `printk` 两种标准输出函数，可以配置它们的输出行为。

#### 6.1 默认输出行为

**不启用重定向时**（默认）：

| 函数 | 默认输出 | 说明 |
|------|---------|------|
| `printf` | UART（syslog） | 通过 syslog 直接立即输出到 UART |
| `printk` | UART（syslog） | 通过 syslog 直接立即输出到 UART |

```c
// 默认情况下，printf 和 printk 都直接同步输出到 UART
printf("Hello from printf\n");   // 同步到 UART
printk("Hello from printk\n");   // 同步到 UART
```

#### 6.2 重定向到日志系统

可以将 `printf` 和 `printk` 重定向到日志系统（lisa_log），由日志系统决定后续行为。

**配置方式**：

在 `prj.conf` 中添加：

```ini
# 将 printk 重定向到日志系统
CONFIG_SYSLOG_PRINTK_REDIRECT=y

# 将 printf 重定向到日志系统
CONFIG_SYSLOG_PRINTF_REDIRECT=y
```

**启用重定向后的行为**：

| 函数 | 重定向后输出 | 说明 |
|------|------------|------|
| `printf` | 日志系统 | 经过 lisa_log 格式化，带时间戳、标签等 |
| `printk` | 日志系统 | 经过 lisa_log 格式化，带时间戳、标签等 |

```c
// 启用重定向后，printf 和 printk 会经过日志系统处理
printf("Hello from printf\n");   // 格式化输出：[I/TAG] Hello from printf
printk("Hello from printk\n");   // 格式化输出：[I/TAG] Hello from printk
```

**使用建议**：

| 场景 | 推荐配置 | 原因 |
|------|---------|------|
| 调试第三方库 | 不重定向（默认） | 第三方库的 printf 直接输出，不受日志级别影响 |
| 统一日志管理 | 启用重定向 | 所有输出都经过日志系统，便于过滤和管理 |
| 高性能应用 | 不重定向（默认） | 避免日志系统开销 |

#### 6.3 使用日志宏 vs printf/printk

**推荐使用日志宏**（`LOGI`、`LOGD` 等）而不是 `printf`/`printk`：

```c
// ✅ 推荐：使用日志宏
#define LOG_TAG "MyApp"
#include "lisa_log.h"

LOGI("Application started");        // 带日志级别、标签、时间戳
LOGD("Debug value: %d", value);     // 可以通过级别过滤

// ❌ 不推荐：使用 printf/printk
printf("Application started\n");     // 无日志级别，无法过滤
printk("Debug value: %d\n", value);  // 无法通过日志级别控制
```

**日志宏的优势**：
- 支持日志级别过滤
- 自动添加时间戳、文件名、行号等信息
- 支持编译期优化（低级别日志可被优化掉）
- 统一的输出格式

### 7. 完整配置示例

以下是典型的 `prj.conf` 配置示例，包含日志组件和 syslog 配置。

**开发调试配置**（详细日志，高波特率，大缓冲区）：

```ini
# ========== 日志组件配置 ==========
# 启用日志组件
CONFIG_LOG=y

# 日志级别：调试模式
CONFIG_LOG_LEVEL_DEBUG=y

# 异步模式
CONFIG_LOG_MODE_ASYNC=y
CONFIG_LOG_ASYNC_BUF_SIZE=20480
CONFIG_LOG_ASYNC_TASK_STACK_SIZE=1024
CONFIG_LOG_ASYNC_TASK_PRIORITY=2

# 行缓冲区
CONFIG_LOG_LINE_BUF_SIZE=2048

# ========== syslog UART 配置 ==========
# UART 设备选择
CONFIG_SYSLOG_UART_DEVICE_UART0=y

# 波特率（高速输出）
CONFIG_SYSLOG_UART_BAUDRATE=921600

# printf/printk 重定向（统一日志管理）
CONFIG_SYSLOG_PRINTF_REDIRECT=y
CONFIG_SYSLOG_PRINTK_REDIRECT=y
```

**生产环境配置**（精简日志，标准波特率，小内存占用）：

```ini
# ========== 日志组件配置 ==========
# 启用日志组件
CONFIG_LOG=y

# 日志级别：仅显示警告和错误
CONFIG_LOG_LEVEL_WARN=y

# 异步模式
CONFIG_LOG_MODE_ASYNC=y
CONFIG_LOG_ASYNC_BUF_SIZE=4096
CONFIG_LOG_ASYNC_TASK_STACK_SIZE=512
CONFIG_LOG_ASYNC_TASK_PRIORITY=1

# 行缓冲区
CONFIG_LOG_LINE_BUF_SIZE=512

# ========== syslog UART 配置 ==========
# UART 设备选择
CONFIG_SYSLOG_UART_DEVICE_UART0=y

# 波特率（标准波特率，兼容性好）
CONFIG_SYSLOG_UART_BAUDRATE=115200

# printf/printk 重定向（统一日志管理）
CONFIG_SYSLOG_PRINTF_REDIRECT=y
CONFIG_SYSLOG_PRINTK_REDIRECT=y
```

## API 参考

### 初始化

#### lisa_log_init

```c
int lisa_log_init(void);
```

**功能**：初始化日志系统

**返回值**：
- `0`：成功
- 其他：失败

**说明**：
- **系统会在启动流程中自动调用此函数**，用户代码无需手动调用
- 会自动初始化 EasyLogger 前端和 UART 后端
- 初始化完成后，日志会自动输出到 UART 串口
- 重复调用是安全的

### 日志输出宏

#### 基于 TAG 的日志宏

```c
#define LOGE(fmt, ...)  // 错误级别
#define LOGW(fmt, ...)  // 警告级别
#define LOGI(fmt, ...)  // 信息级别
#define LOGD(fmt, ...)  // 调试级别
#define LOGV(fmt, ...)  // 详细级别
```

**重要说明**：
- 使用这些宏之前，**必须**先定义 `LOG_TAG` 宏
- `LOG_TAG` 定义必须在 `#include "lisa_log.h"` **之前**
- 每个源文件只需定义一次 `LOG_TAG`

**使用方法**：

```c
// 重要：LOG_TAG 必须在包含头文件之前定义
#define LOG_TAG "MyModule"
#include "lisa_log.h"

void my_function(void) {
    LOGI("Application started");
    LOGD("Debug info: value = %d", 42);
    LOGE("Error occurred: %s", "connection failed");
}
```

#### 带标签参数的日志宏

```c
#define LISA_LOGE(tag, format, ...)
#define LISA_LOGW(tag, format, ...)
#define LISA_LOGI(tag, format, ...)
#define LISA_LOGD(tag, format, ...)
#define LISA_LOGV(tag, format, ...)
```

**使用方法**：

```c
void my_function(void) {
    LISA_LOGI("Network", "Connection established");
    LISA_LOGD("Network", "IP: %s", "192.168.1.1");
}
```

#### 十六进制转储

```c
#define LOGH(name, data, len)           // 基于 TAG 的转储
#define LISA_LOGH(tag, data, len, name) // 带标签的转储
```

**使用方法**：

```c
#define LOG_TAG "DataDump"
#include "lisa_log.h"

void dump_data(void) {
    uint8_t buffer[16] = {0x01, 0x02, 0x03, ...};
    LOGH("buffer", buffer, 16);
    LISA_LOGH("Network", buffer, 16, "rx_data");
}
```

### 日志级别控制

#### lisa_log_set_level

```c
void lisa_log_set_level(uint8_t lvl);
```

**功能**：动态设置日志输出级别

**参数**：
- `lvl`：日志级别（LISA_LOG_LEVEL_ERROR, LISA_LOG_LEVEL_INFO 等）

**使用方法**：

```c
// 只输出错误级别及以上的日志
lisa_log_set_level(LISA_LOG_LEVEL_ERROR);

// 输出所有级别的日志
lisa_log_set_level(LISA_LOG_LEVEL_VERBOSE);
```

### 日志刷新

#### log_flush

```c
void log_flush(void);
```

**功能**：刷新异步日志缓冲区

**说明**：
- 在异步模式下，确保所有日志都已输出
- 在系统进入休眠或关键操作前调用

### UART 后端控制

UART 后端在系统初始化时自动启用，以下 API 可用于控制日志输出：

#### log_flush

```c
void log_flush(void);
```

**功能**：刷新异步日志缓冲区，确保所有日志已通过 UART 输出

**使用场景**：
- 系统进入休眠前
- 关键操作前，确保日志已完整输出
- 调试时需要立即查看日志

**示例**：

```c
LOGI("Entering sleep mode...");
log_flush();  // 确保日志已输出
enter_sleep();
```

## 使用示例

### 基本使用

```c
// 重要：LOG_TAG 必须在包含头文件之前定义
#define LOG_TAG "MyApp"
#include "lisa_log.h"

void app_main(void) {
    // 注意：日志系统已在系统启动时自动初始化，通常不需要手动调用 lisa_log_init()

    // 输出不同级别的日志
    LOGI("Application started, version: %s", "1.0.0");
    LOGD("Debug mode enabled");
    LOGW("This is a warning message");
    LOGE("Error code: %d", -1);

    // 十六进制转储
    uint8_t data[] = {0xAA, 0xBB, 0xCC, 0xDD};
    LOGH("data", data, sizeof(data));
}
```

### 动态控制日志级别

```c
#define LOG_TAG "LogCtrl"
#include "lisa_log.h"

void set_debug_level(const char *level) {
    if (strcmp(level, "error") == 0) {
        lisa_log_set_level(LISA_LOG_LEVEL_ERROR);
    } else if (strcmp(level, "info") == 0) {
        lisa_log_set_level(LISA_LOG_LEVEL_INFO);
    } else if (strcmp(level, "debug") == 0) {
        lisa_log_set_level(LISA_LOG_LEVEL_DEBUG);
    } else if (strcmp(level, "verbose") == 0) {
        lisa_log_set_level(LISA_LOG_LEVEL_VERBOSE);
    }
    LOGI("Log level changed to: %s", level);
}
```

### 使用带标签的日志

```c
#include "lisa_log.h"

void network_task(void) {
    LISA_LOGI("Network", "Task started");

    if (connect_to_server()) {
        LISA_LOGI("Network", "Connected to server");
    } else {
        LISA_LOGE("Network", "Connection failed");
    }
}

void storage_task(void) {
    LISA_LOGI("Storage", "Task started");

    if (mount_filesystem()) {
        LISA_LOGI("Storage", "Filesystem mounted");
    } else {
        LISA_LOGE("Storage", "Mount failed");
    }
}
```

### 进入低功耗前刷新日志

```c
#define LOG_TAG "Power"
#include "lisa_log.h"

void enter_low_power_mode(void) {
    LOGI("Preparing to enter low power mode");

    // 刷新所有待输出的日志
    log_flush();

    // 进入低功耗模式
    hal_pm_enter_sleep();
}

void exit_low_power_mode(void) {
    LOGI("System resumed from low power mode");
}
```

## 注意事项

### 1. LOG_TAG 定义顺序

**重要**：使用基于 TAG 的日志宏（`LOGE`/`LOGW`/`LOGI`/`LOGD`/`LOGV`）时，必须在包含头文件之前定义 `LOG_TAG`：

```c
// ✅ 正确的顺序
#define LOG_TAG "MyModule"
#include "lisa_log.h"

// ❌ 错误的顺序
#include "lisa_log.h"
#define LOG_TAG "MyModule"  // 错误：定义太晚
```

**说明**：
- `LOG_TAG` 必须在 `#include "lisa_log.h"` 之前定义
- 如果未定义 `LOG_TAG`，默认会使用 `"NO_TAG"`
- 使用 `LISA_LOG*` 系列宏（如 `LISA_LOGI`）不需要定义 `LOG_TAG`

### 2. 异步模式日志丢失

在异步模式下，如果日志输出速度过快，可能导致缓冲区满而丢失日志。

**解决方法**：
- 增加 `CONFIG_LOG_ASYNC_BUF_SIZE`（缓冲区大小）
- 提高异步任务优先级 `CONFIG_LOG_ASYNC_TASK_PRIORITY`
- 降低日志输出频率
- 在关键位置调用 `log_flush()` 确保日志输出

#### 常见问题

如果日志无输出或出现乱码，请检查：

- 串口工具波特率是否与 `CONFIG_SYSLOG_UART_BAUDRATE` 一致
- 是否连接到了正确的 UART 设备（`prj.conf` 中配置的 UART0/UART1/UART2）
- 硬件连接是否正确（TX、RX、GND）
- 日志级别是否过滤了所有输出

### 4. 日志级别过滤

日志级别在编译期和运行期都可以过滤：

**编译期过滤**（减少代码体积）：
- 通过 `pri.conf` 设置 `CONFIG_LOG_LEVEL_*`
- 低于设置级别的日志宏会被优化掉

**运行期过滤**（动态调整）：
- 调用 `lisa_log_set_level()` 动态设置级别
- 只影响运行期输出，不减少代码体积

### 5. 线程安全

- 日志系统是线程安全的，可以在多任务环境下并发调用
- 内部使用互斥锁保护共享数据
- 在中断上下文中调用时，建议使用同步模式避免延迟

### 6. 编译期日志优化

通过定义 `LISA_LOG_LEVEL` 宏可以在编译期过滤特定模块的日志：

```c
// 在模块头文件中定义
#define LISA_LOG_LEVEL LISA_LOG_LEVEL_INFO
#include "lisa_log.h"

// 该模块中低于 INFO 级别的日志会被编译器优化掉
// LOGD() 和 LOGV() 不会产生代码
```

## 常见问题 FAQ

**Q1：如何修改日志输出的串口？**

A：日志使用的串口由系统 syslog 决定，需要在系统启动代码中配置 UART 初始化。日志组件本身不负责串口选择。

**Q2：异步模式和同步模式如何选择？**

A：
- **异步模式（推荐）**：适合一般应用，不阻塞程序，性能好
- **同步模式**：适合调试关键问题，确保日志不丢失

**Q3：如何在代码中动态开关日志？**

A：使用 `lisa_log_set_level()` 设置日志级别：

```c
// 关闭日志（只输出 ASSERT）
lisa_log_set_level(LISA_LOG_LEVEL_ASSERT);

// 开启所有日志
lisa_log_set_level(LISA_LOG_LEVEL_VERBOSE);
```

**Q4：日志会影响实时性吗？**

A：异步模式下，日志调用会立即返回，对实时性影响很小。如果对实时性要求极高，可以在关键代码段降低日志级别或关闭日志。

