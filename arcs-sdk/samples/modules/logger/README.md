# Logger 示例

## 功能说明

演示如何使用 LISA Log 组件进行日志输出，包括不同级别的日志、格式化输出、十六进制转储和动态级别调整等功能。

## 硬件连接

看对应板型描述串口输出引脚。

**UART 配置**：
- **设备**: UART0（可在 `prj.conf` 中修改为 UART1 或 UART2）
- **波特率**: 921600（可在 `prj.conf` 中修改）
- **参数**: 8 数据位，1 停止位，无校验

使用串口工具连接到开发板的 UART0，配置与上述参数一致。

## 示例内容

本示例包含 6 个演示功能：

1. **基本日志输出**: 演示使用不同级别的日志宏（INFO、WARN、ERROR、DEBUG、VERBOSE）
2. **格式化日志输出**: 演示输出整数、字符串、十六进制、指针等不同类型的数据
3. **带标签参数的日志**: 演示使用 `LISA_LOG*` 系列宏动态指定日志标签
4. **十六进制数据转储**: 演示使用 `LOGH` 和 `LISA_LOGH` 宏输出二进制数据
5. **动态调整日志级别**: 演示在运行时改变日志输出级别
6. **异步日志刷新**: 演示使用 `log_flush()` 刷新日志缓冲区

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 预期输出

日志输出格式为：`级别/标签 [时间戳 任务ID 函数名] 消息内容`

```
I/logger_sample   [1034:42:44.159 1 main] === LISA Log Component Example ===
I/logger_sample   [1034:42:44.159 1 main] This example demonstrates various logging features
I/logger_sample   [1034:42:44.159 1 main]
I/logger_sample   [1034:42:44.259 1 main] ---------- Basic Logging Demo ----------
I/logger_sample   [1034:42:44.259 1 main] This is an INFO level log
W/logger_sample   [1034:42:44.259 1 main] This is a WARNING level log
E/logger_sample   [1034:42:44.259 1 main] This is an ERROR level log
I/logger_sample   [1034:42:44.259 1 main] Basic logging demo completed
I/logger_sample   [1034:42:44.259 1 main]
I/logger_sample   [1034:42:44.759 1 main] ---------- Formatted Logging Demo ----------
I/logger_sample   [1034:42:44.759 1 main] Current temperature: 25°C
I/logger_sample   [1034:42:44.759 1 main] Voltage: 3.30 V
I/logger_sample   [1034:42:44.759 1 main] Device name: UART0
I/logger_sample   [1034:42:44.759 1 main] Memory address: 0x20001000
I/logger_sample   [1034:42:44.759 1 main] Allocated memory at: 0x5a100
I/logger_sample   [1034:42:44.759 1 main] Formatted logging demo completed
I/logger_sample   [1034:42:44.759 1 main]
I/logger_sample   [1034:42:45.259 1 main] ---------- Tagged Logging Demo ----------
I/network         [1034:42:45.259 1 main] Connection established
I/network         [1034:42:45.259 1 main] IP: 192.168.1.100
I/storage         [1034:42:45.259 1 main] Filesystem mounted
I/storage         [1034:42:45.259 1 main] Free space: 1024 KB
I/sensor          [1034:42:45.259 1 main] Temperature sensor initialized
I/logger_sample   [1034:42:45.259 1 main] Tagged logging demo completed
I/logger_sample   [1034:42:45.259 1 main]
I/logger_sample   [1034:42:45.759 1 main] ---------- Hex Dump Demo ----------
I/logger_sample   [1034:42:45.759 1 main] Dumping small data buffer:
D/HEX data1: 0000-000F: 01 02 03 04 05 06 07 08                             ........
I/logger_sample   [1034:42:45.759 1 main] Dumping large data buffer:
D/HEX data2: 0000-000F: AA BB CC DD EE FF 11 22  33 44 55 66 77 88 99 00    ......."3DUfw...
I/logger_sample   [1034:42:45.759 1 main] Hex dump demo completed
I/logger_sample   [1034:42:45.759 1 main]
I/logger_sample   [1034:42:46.259 1 main] ---------- Dynamic Log Level Demo ----------
I/logger_sample   [1034:42:46.259 1 main] Current log level, all logs visible:
I/logger_sample   [1034:42:46.259 1 main] INFO: Application running
I/logger_sample   [1034:42:46.259 1 main] Switching to WARN level...
W/logger_sample   [1034:42:46.259 1 main] WARN: This warning IS visible
E/logger_sample   [1034:42:46.259 1 main] ERROR: This error IS visible
I/logger_sample   [1034:42:46.259 1 main] Switched to VERBOSE level
I/logger_sample   [1034:42:46.259 1 main] INFO: All logs visible now
I/logger_sample   [1034:42:46.259 1 main] Restored to INFO level
I/logger_sample   [1034:42:46.259 1 main] Dynamic log level demo completed
I/logger_sample   [1034:42:46.259 1 main]
I/logger_sample   [1034:42:46.759 1 main] ---------- Log Flush Demo ----------
I/logger_sample   [1034:42:46.759 1 main] Log message 1
I/logger_sample   [1034:42:46.759 1 main] Log message 2
I/logger_sample   [1034:42:46.759 1 main] Log message 3
I/logger_sample   [1034:42:46.759 1 main] Log message 4
I/logger_sample   [1034:42:46.759 1 main] Log message 5
I/logger_sample   [1034:42:46.759 1 main] Flushing log buffer...
I/logger_sample   [1034:42:46.765 1 main] Log buffer flushed, all logs have been output
I/logger_sample   [1034:42:46.765 1 main] Log flush demo completed
I/logger_sample   [1034:42:46.765 1 main]
I/logger_sample   [1034:42:46.765 1 main] === All examples completed ===
I/logger_sample   [1034:42:46.765 1 main] System will keep running...
```

**日志格式说明**：
- **级别**: I(Info), W(Warning), E(Error), D(Debug), V(Verbose)
- **标签**: 日志来源标识，如 `logger_sample`, `network`, `storage`
- **时间戳**: 格式为 `天:时:分:秒.毫秒`
- **任务ID**: FreeRTOS 任务标识
- **函数名**: 调用日志的函数名称

## 核心 API

| API | 说明 |
|-----|------|
| `LOGI(fmt, ...)` | 输出 INFO 级别日志（使用 LOG_TAG） |
| `LOGW(fmt, ...)` | 输出 WARN 级别日志（使用 LOG_TAG） |
| `LOGE(fmt, ...)` | 输出 ERROR 级别日志（使用 LOG_TAG） |
| `LOGD(fmt, ...)` | 输出 DEBUG 级别日志（使用 LOG_TAG） |
| `LOGV(fmt, ...)` | 输出 VERBOSE 级别日志（使用 LOG_TAG） |
| `LISA_LOGI(tag, fmt, ...)` | 输出 INFO 级别日志（指定标签） |
| `LISA_LOGD(tag, fmt, ...)` | 输出 DEBUG 级别日志（指定标签） |
| `LOGH(name, data, len)` | 输出十六进制数据转储（使用 LOG_TAG） |
| `LISA_LOGH(tag, data, len, name)` | 输出十六进制数据转储（指定标签） |
| `lisa_log_set_level(lvl)` | 动态设置日志级别 |
| `log_flush()` | 刷新异步日志缓冲区 |

## 关键代码

### 1. LOG_TAG 定义

**重要**：必须在包含 `lisa_log.h` 之前定义 `LOG_TAG`：

```c
#define LOG_TAG "logger_sample"
#include <lisa_log.h>
```

### 2. 基本日志输出

```c
/* 使用基于 TAG 的日志宏 */
LOGI("This is an INFO level log");
LOGW("This is a WARNING level log");
LOGE("This is an ERROR level log");
LOGD("This is a DEBUG level log");
LOGV("This is a VERBOSE level log");
```

### 3. 格式化日志输出

```c
int temperature = 25;
LOGI("Current temperature: %d°C", temperature);

const char *device_name = "UART0";
LOGI("Device name: %s", device_name);

void *ptr = (void *)0x20005A100;
LOGI("Allocated memory at: %p", ptr);
```

### 4. 带标签参数的日志

```c
/* 使用 LISA_LOG* 系列宏，可以动态指定标签 */
LISA_LOGI("network", "Connection established");
LISA_LOGI("storage", "Filesystem mounted");
LISA_LOGD("sensor", "Reading temperature: %d°C", 25);
```

### 5. 十六进制数据转储

```c
uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

/* 使用基于 TAG 的转储 */
LOGH("data1", data, sizeof(data));

/* 使用带标签的转储 */
LISA_LOGH("hex_dump", data, sizeof(data), "data1");
```

### 6. 动态调整日志级别

```c
/* 切换到 WARN 级别 - 只显示警告和错误 */
lisa_log_set_level(LISA_LOG_LEVEL_WARN);
LOGI("This log will NOT be visible");
LOGW("This warning IS visible");

/* 切换到 VERBOSE 级别 - 显示所有日志 */
lisa_log_set_level(LISA_LOG_LEVEL_VERBOSE);
LOGI("All logs visible now");
LOGD("Debug information visible");
LOGV("Very detailed information visible");
```

### 7. 异步日志刷新

```c
/* 输出日志后刷新缓冲区，确保所有日志已输出 */
LOGI("Important operation completed");
log_flush();
```

## 配置说明

### 日志级别配置

在 `prj.conf` 中配置编译期日志级别：

```ini
# 选择一个日志级别（只能选择一个）
CONFIG_LOG_LEVEL_INFO=y      # 信息级别（默认）
# CONFIG_LOG_LEVEL_DEBUG=y   # 调试级别
# CONFIG_LOG_LEVEL_VERBOSE=y # 详细级别
```

**说明**：
- 选择某个级别后，该级别及以上的日志都会被编译进代码
- 例如选择 `INFO`，则 ERROR、WARN、INFO 会被编译，DEBUG 和 VERBOSE 会被优化掉
- 可以在运行时使用 `lisa_log_set_level()` 动态调整级别（在编译期级别范围内）

### 异步模式配置

在 `prj.conf` 中配置异步模式参数：

```ini
# 异步模式（推荐）
CONFIG_LOG_MODE_ASYNC=y
CONFIG_LOG_ASYNC_BUF_SIZE=10240           # 缓冲区大小
CONFIG_LOG_ASYNC_TASK_STACK_SIZE=512      # 任务栈大小
CONFIG_LOG_ASYNC_TASK_PRIORITY=1          # 任务优先级
```

### UART 后端配置

在 `prj.conf` 中配置 UART 输出参数：

```ini
# UART 设备选择
CONFIG_SYSLOG_UART_DEVICE_UART0=y
# CONFIG_SYSLOG_UART_DEVICE_UART1=y
# CONFIG_SYSLOG_UART_DEVICE_UART2=y

# 波特率
CONFIG_SYSLOG_UART_BAUDRATE=921600
# CONFIG_SYSLOG_UART_BAUDRATE=115200
```

## 注意事项

1. **LOG_TAG 定义顺序**: 使用 `LOGI` 等宏时，必须在 `#include <lisa_log.h>` 之前定义 `LOG_TAG`，否则会使用默认标签 "NO_TAG"

2. **日志级别过滤**: 编译期配置的日志级别决定了哪些日志会被编译进代码，低于该级别的日志会被优化掉。运行期只能在编译期级别范围内调整

3. **异步模式日志丢失**: 在异步模式下，如果日志输出速度过快，可能导致缓冲区满而丢失日志。可以通过增加 `CONFIG_LOG_ASYNC_BUF_SIZE` 或调用 `log_flush()` 来避免

4. **串口波特率匹配**: 串口工具的波特率必须与 `CONFIG_SYSLOG_UART_BAUDRATE` 配置一致，否则会出现乱码

5. **日志输出语言**: 建议日志消息使用英文，保持日志输出国际化和一致性

6. **资源占用**: 日志系统会占用一定的 RAM（缓冲区）和 ROM（日志字符串），生产环境可以降低日志级别或减小缓冲区以节省资源

