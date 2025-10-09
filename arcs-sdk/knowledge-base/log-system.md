# 日志系统 (components/lisa_porting/log)

## 1. 概述

ToyCloud-CP SDK 提供了一个分层的日志系统。底层的 `syslog` 模块（位于 `arcs-base/startup/arcs/`）提供了基础的 UART 日志输出和 `printk` 功能。在此之上，`components/lisa_porting/log` 目录下的 `lisa_log` 组件提供了一个更高级、功能更丰富的日志接口。`lisa_log` 可以配置使用 `EasyLogger` 作为其核心日志处理前端，支持多种日志级别、日志标签、异步记录以及自定义日志输出，并且会接管和整合来自底层 `syslog` 的输出。

## 2. 关键文件

*   **`lisa_log.h`**: 定义了日志系统的公共接口、宏（如 `LOGE`, `LOGW`, `LOGI` 等）以及日志级别。
*   **`lisa_log.c`**: 包含了日志系统核心函数的实现，例如初始化、设置级别和日志输出处理。
*   **`Kconfig`**: 定义了日志系统的编译时配置选项，允许用户根据需求定制日志功能。
*   **`CMakeLists.txt`**: `lisa_log` 模块的构建脚本.
*   **`arcs-base/startup/arcs/syslog.h`**: 定义了底层 `syslog` 模块的公共接口。
*   **`arcs-base/startup/arcs/syslog.c`**: 包含了底层 `syslog` 模块的实现，主要负责 UART 初始化和原始/格式化输出。

## 3. 主要功能

### 3.1. 日志级别

系统定义了以下日志级别，严重程度从高到低：
*   `LOG_LEVEL_NONE`
*   `LOG_LEVEL_ERROR`
*   `LOG_LEVEL_WARN`
*   `LOG_LEVEL_INFO` (默认级别，可通过 `Kconfig` 修改)
*   `LOG_LEVEL_DEBUG`
*   `LOG_LEVEL_VERBOSE`

可以通过 `lisa_log_set_level()` 函数在运行时更改日志级别。

### 3.2. 日志标签 (`LOG_TAG`)

每个日志消息都可以关联一个标签，用于标识日志来源的模块或组件。默认标签为 "NO_TAG"。

### 3.3. 初始化与配置

*   **`syslog_init()`**: （通常由系统早期调用）初始化底层 `syslog` 使用的 UART 通道和波特率。
*   **`lisa_log_init()`**: 初始化 `lisa_log` 组件。如果选择 `EasyLogger` 作为前端（默认配置），此函数会：
    *   初始化 `EasyLogger`，设置其日志格式和级别。
    *   调用 `syslog_hook_set(elog_raw_output_v)`，将底层 `syslog`（包括其 `printk`）的输出重定向到 `EasyLogger`。这意味着所有通过 `printk` 或底层 `syslog` 接口产生的日志都将由 `EasyLogger` 处理。
    *   启动 `EasyLogger`。

### 3.4. 日志前端

*   **`EasyLogger`**: (通过 `CONFIG_LOG_FRONTEND_EASYLOGGER` 启用，为默认前端)
    *   提供更丰富的日志格式化选项。
    *   支持异步日志记录，可以减少对应用程序性能的影响。
    *   提供 `elog_hexdump` 等高级功能。
*   **`printk`**: 如果未选择 `EasyLogger`，系统会回退到一个基于 `printk` 的简单日志实现。

### 3.5. 日志输出

*   **默认后端**: 通过 `Kconfig` 中的 `LOG_BACKEND_UART` 配置，默认为 UART 输出。
*   **自定义输出处理**: `lisa_log_output_handle_set(void (*handle)(const char *, int))` 函数允许用户注册一个自定义的函数来处理日志输出。这使得日志可以被重定向到文件、网络或其他目标。
*   `elog_port_output_log()`: 当使用 `EasyLogger` 时，此函数负责将格式化后的日志传递给自定义输出处理函数或 `syslog_write`。

### 3.6. 十六进制转储

*   `LOGH(name, data, len)` (宏，使用 `EasyLogger` 时)
*   `logDump(uint8_t *data, int len)`
*   `logHexDump(char *name, uint8_t width, uint8_t *data, int len)`

这些函数和宏方便以十六进制格式打印二进制数据。

### 3.7. 断言

*   `LISA_ASSERT(exp, fmt, ...)`: 提供断言功能，表达式 `exp` 为假时，会打印错误信息并调用标准 `assert`。

### 3.8. 底层 Syslog 模块 (`arcs-base/startup/arcs/`)

SDK 包含一个基础的 `syslog` 模块，其主要职责是通过 UART 提供日志输出能力。

*   **核心功能**:
    *   `syslog_init()`: 配置和初始化用于日志输出的 UART 硬件（如 UART0 或 UART1）。
    *   `syslog_write()`: 将原始数据直接写入已配置的 UART。
    *   `printk()`: 提供一个标准 `printk` 接口，用于格式化输出。默认情况下，其输出通过 `syslog_write()` 定向到 UART。
*   **钩子机制 (`syslog_output_hook`)**:
    *   `syslog` 模块提供 `syslog_hook_set()` 函数，允许其他模块（如 `lisa_log` 中的 `EasyLogger`）注册一个钩子函数。
    *   一旦钩子被设置，所有通过 `syslog_raw_output_v`（进而影响 `printk`）的输出都会被重定向到这个钩子函数，而不是直接发送到 UART。
*   **与 `lisa_log` 的集成**: 
    *   当 `lisa_log` 使用 `EasyLogger` 前端时，`lisa_log_init()` 会调用 `syslog_hook_set(elog_raw_output_v)`。
    *   这使得所有源自 `printk` 或 `syslog` 内部的日志消息都被 `EasyLogger` 捕获。
    *   `EasyLogger` 随后根据其自身的配置（格式、级别、同步/异步模式）处理这些消息，并通过其移植层函数 `elog_port_output_log()` 输出。这意味着即使用户代码直接调用底层 `printk`，其输出也会遵循 `EasyLogger` 的规则和格式。

## 4. Kconfig 配置选项

`components/lisa_porting/log/Kconfig` 文件提供了以下主要的编译时配置：

*   **`LOG`**: (bool) 是否启用日志系统 (依赖 `LISA_PORTING`)。
*   **Log Frontend**:
    *   `LOG_FRONTEND_EASYLOGGER`: (bool) 选择 `EasyLogger` 作为日志前端 (依赖 `SDK_MODULE_EASYLOGGER`)，默认启用。
*   **Log Backend**:
    *   `LOG_BACKEND_UART`: (bool) 选择 UART 作为日志输出后端，默认启用。
*   **`LOG_MODE_ASYNC`**: (bool) 是否启用异步日志模式，默认启用。通过 `select EASYLOGGER_LOG_MODE_ASYNC` 机制自动配置 EasyLogger 的异步模式。
*   **Log Level**: 选择默认的编译时日志级别，默认为 `LOG_LEVEL_INFO`。

## 5. 使用示例 (伪代码)

```c
#define LOG_TAG "MY_MODULE" // 定义当前模块的日志标签
#include "lisa_log.h"

void my_module_init() {
    // lisa_log_init() 通常在系统启动早期被调用
    LOGI("Initializing My Module");
    // ...
}

void my_module_process_data(uint8_t* data, int len) {
    if (data == NULL) {
        LOGE("Received NULL data pointer!");
        return;
    }
    LOGD("Processing data, length: %d", len);
    LISA_LOGH(LOG_TAG, data, len, "Received Data"); // 假设这是 EasyLogger 的 hexdump 宏或类似功能
    // ...
}
```

## 6. 总结

该 SDK 提供了一个分层的日志解决方案。基础的 `syslog` 模块负责底层的 UART 输出和 `printk` 功能。`lisa_log` 组件（尤其在与 `EasyLogger` 前端结合时）在其之上构建了一个功能更全面的日志系统，不仅提供了日志级别、标签、自定义输出等高级特性，还通过钩子机制整合了来自底层 `syslog` 和 `printk` 的输出。

这种设计使得所有日志（无论是通过高级 `LOGX` 宏还是底层 `printk`）都能被统一管理和格式化。通过 Kconfig 的配置和运行时 API 的调用，开发者可以灵活地控制日志的输出行为、级别和格式，以满足不同的调试和监控需求。
