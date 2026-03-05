# ACOMP Logger 组件

## 概述

ACOMP Logger 组件是一个用于在 Remote (AP) 和 Master 核心之间传输系统日志的通信组件。它通过创建 R2M (Remote to Master) 数据流，将 Remote 端的日志实时传输到 Master 端进行输出和显示。

## 架构

```
Remote 端 (AP)                          Master 端
┌────────────────────┐                 ┌────────────────────┐
│  lisa_log 系统     │                 │  日志接收线程      │
│        ↓           │                 │        ↓           │
│  logger backend    │                 │  从 R2M 流读取     │
│        ↓           │   R2M Stream    │        ↓           │
│  acomp logger 设备 │ ──────────────→ │  缓冲区处理        │
│        ↓           │    (IPC)        │        ↓           │
│  stream tx         │                 │  lisa_log 输出     │
└────────────────────┘                 └────────────────────┘
```

## 功能特性

### Remote 端

- 通过 `lisa_log_backend_add` 自动注册为日志后端
- 拦截并捕获所有系统日志
- 通过 R2M 流将日志数据实时发送到 Master 端
- 无需手动配置，开箱即用

### Master 端

- **自动化管理**：通过简单的 API 完成所有初始化和启动
- **灵活配置**：通过 Kconfig 自定义缓冲区、触发策略等参数
- **两种触发模式**：
  - **手动触发模式**（默认）：按固定间隔轮询检查日志
  - **自动触发模式**：基于信号量自动触发，收到指定数量缓冲区后处理
- **线程安全**：专用接收线程自动处理日志接收和输出
- **高性能**：支持大容量缓冲池（最多 128 个缓冲区）和可配置的缓冲区大小

## 使用方法

### Remote 端

Remote 端的 logger 设备会自动注册并初始化，只需要确保：

1. 在 Kconfig 中启用 logger 组件
2. 系统会自动加载 `acomp.logger` 设备并注册日志后端

无需编写任何代码，Remote 端会自动开始捕获日志。

### Master 端

#### 快速开始

最简单的使用方式，只需两步：

```c
#include "acomp_logger.h"

int main(void)
{
    int ret;

    // 1. 初始化 logger 组件
    ret = acomp_logger_init();
    if (ret != ACOMP_ERR_OK) {
        printf("Logger init failed: %d\n", ret);
        return ret;
    }

    // 2. 启动 logger - 自动创建接收线程并输出日志
    ret = acomp_logger_start();
    if (ret != ACOMP_ERR_OK) {
        printf("Logger start failed: %d\n", ret);
        return ret;
    }

    printf("Logger started! Remote logs will appear here.\n");

    // Remote 端的日志现在会自动显示在 Master 端

    // 应用程序继续运行...

    return 0;
}
```

#### `acomp_logger_start()` 自动完成的工作

调用 `acomp_logger_start()` 后，组件会自动：

1. **配置 R2M 流通道**：根据 Kconfig 设置配置缓冲区大小和数量
2. **启动 Remote 端日志捕获**：通知 Remote 端开始发送日志
3. **创建接收线程**：根据配置的触发策略自动接收和输出日志
   - 手动触发模式：按配置的轮询间隔定期检查
   - 自动触发模式：等待信号量触发，收到指定数量缓冲区后处理

#### 停止日志传输

```c
// 停止 logger - 自动停止线程和流通道
int ret = acomp_logger_stop();
if (ret != ACOMP_ERR_OK) {
    printf("Logger stop failed: %d\n", ret);
}
```

`acomp_logger_stop()` 会自动：

1. 停止日志接收线程
2. 停止 Remote 端的日志捕获
3. 禁用并清理 R2M 流通道

## API 参考

### 基础 API

ACOMP Logger 组件提供了简洁的 API：

#### `acomp_logger_init()`

初始化 logger 组件。

**返回值：**
- `ACOMP_ERR_OK` - 成功
- `ACOMP_ERR_NO_MEM` - 内存不足
- `ACOMP_ERR_INVALID_STATE` - 无效状态
- `ACOMP_ERR_NOT_FOUND` - 设备未找到

#### `acomp_logger_start()`

启动日志传输功能。

该函数会自动完成：
1. 根据 Kconfig 配置 R2M 流通道
2. 启动 Remote 端的日志捕获
3. 创建日志接收线程（根据触发策略工作）

**返回值：**
- `ACOMP_ERR_OK` - 成功
- `ACOMP_ERR_NO_MEM` - 内存不足
- `ACOMP_ERR_INVALID_STATE` - 无效状态

#### `acomp_logger_stop()`

停止日志传输功能。

该函数会自动完成：
1. 停止日志接收线程
2. 停止 Remote 端的日志捕获
3. 禁用并清理 R2M 流通道

**返回值：**
- `ACOMP_ERR_OK` - 成功
- `ACOMP_ERR_INVALID_STATE` - 无效状态

---

### 高级 API - 自定义输出

#### `acomp_logger_set_output_callback()`

```c
typedef int (*acomp_logger_output_cb_t)(const uint8_t *log, uint32_t len);
int acomp_logger_set_output_callback(acomp_logger_output_cb_t cb);
```

设置自定义日志输出回调函数，允许应用层完全控制 AP 日志的输出方式。

**参数：**
- `cb` - 输出回调函数指针
  - 传入函数指针：使用自定义输出方式
  - 传入 `NULL`：恢复默认输出（LISA_LOG_RAW）

**返回值：**
- `ACOMP_ERR_OK` - 成功
- `ACOMP_ERR_INVALID_STATE` - 组件未初始化

**回调函数原型：**
```c
int output_callback(const uint8_t *log, uint32_t len)
{
    // log: 日志数据缓冲区（已格式化的字符串）
    // len: 日志数据长度（字节数）
    // 返回: 0 成功，非 0 失败
}
```

**使用示例：**

```c
/* 示例 1: 输出到 printf */
int my_output(const uint8_t *log, uint32_t len) {
    printf("[AP] %.*s", len, log);
    return 0;
}

/* 示例 2: 带颜色高亮 */
int colored_output(const uint8_t *log, uint32_t len) {
    printf("\033[46m[AP]\033[0m %.*s", len, log);
    return 0;
}

/* 示例 3: 写入文件 */
int file_output(const uint8_t *log, uint32_t len) {
    fwrite(log, 1, len, log_file);
    return 0;
}

/* 使用自定义输出 */
acomp_logger_init();
acomp_logger_set_output_callback(my_output);  // 设置自定义输出
acomp_logger_start();

/* 恢复默认输出 */
acomp_logger_set_output_callback(NULL);  // 恢复默认
```

**注意事项：**
- 必须在 `acomp_logger_init()` 之后调用
- 可以在运行时动态切换回调函数
- 回调函数在日志接收线程中执行，应避免阻塞操作
- 回调函数应尽快返回，避免影响日志接收性能

## 配置参数

所有配置参数通过 Kconfig 系统管理，可通过 `make menuconfig` 进行配置。

### Master 端配置

在 `Components config -> ACOMP Logger` 菜单中配置：

#### 缓冲区配置

- **`ACOMP_LOGGER_STREAM_BUFFER_SIZE`**
  - 说明：每个流缓冲区的大小（字节）
  - 默认值：256
  - 建议：根据日志量调整，较大的缓冲区可以减少传输次数

- **`ACOMP_LOGGER_STREAM_BUFFER_COUNT`**
  - 说明：流缓冲区的数量
  - 默认值：128
  - 要求：**必须是 2 的幂次方**（例如：2, 4, 8, 16, 32, 64, 128）
  - 建议：增加缓冲区数量可提高性能，但会消耗更多内存

#### 触发策略

- **`ACOMP_LOGGER_KICK_POLICY`**
  - 选项：
    - **Manual kick**（默认）：手动轮询模式
    - **Auto kick**：自动触发模式

**手动触发模式配置：**

- **`ACOMP_LOGGER_MANUAL_POLL_INTERVAL_MS`**
  - 说明：缓冲区检查轮询间隔（毫秒）
  - 默认值：10
  - 适用：手动触发模式
  - 建议：减小间隔可提高实时性，但会增加 CPU 占用

**自动触发模式配置：**

- **`ACOMP_LOGGER_AUTO_KICK_BUFFER_THRESHOLD`**
  - 说明：自动触发的缓冲区阈值
  - 默认值：4
  - 适用：自动触发模式
  - 说明：接收到此数量的缓冲区后自动处理

#### 线程配置

- **`ACOMP_LOGGER_THREAD_PRIORITY`**
  - 说明：接收线程优先级
  - 默认值：5
  - 范围：0（最低）到 31（最高）
  - 建议：根据系统负载调整，较高优先级可确保日志及时输出

- **`ACOMP_LOGGER_THREAD_STACK_SIZE`**
  - 说明：接收线程栈大小（字节）
  - 默认值：2048
  - 建议：如遇栈溢出，适当增加此值

#### 日志显示配置

- **`ACOMP_LOGGER_USE_COLOR`**
  - 说明：启用颜色高亮显示 AP 日志
  - 类型：布尔值
  - 默认值：是（启用）
  - 功能：使用 ANSI 颜色码为 AP 日志添加背景色，便于区分 AP 日志和 Master 日志
  - 注意：需要终端支持 ANSI 转义码

- **`ACOMP_LOGGER_COLOR_SCHEME`**（仅当启用颜色时可用）
  - 说明：AP 日志的颜色方案
  - 类型：单选
  - 默认值：`Cyan background`（青色背景）
  - 可选值：
    - **Cyan background**（推荐） - 青色背景 + 黑色文字
    - **Blue background** - 蓝色背景 + 白色文字
    - **Green background** - 绿色背景 + 黑色文字
    - **Yellow background** - 黄色背景 + 黑色文字
    - **Purple background** - 紫色背景 + 白色文字
    - **Light gray background** - 浅灰色背景 + 黑色文字

**颜色效果示例：**

启用颜色后，AP 日志将显示为：
```
[AP] I (12345) tag: This is an AP log message
 ^-- 带颜色背景的 [AP] 前缀
```

禁用颜色后，AP 日志将显示为：
```
[AP] I (12345) tag: This is an AP log message
 ^-- 纯文本 [AP] 前缀
```

**配置方法：**

方式 1 - 通过 menuconfig：
```bash
make menuconfig
# 导航到: Components config -> ACOMP Logger
# 选择: Enable color highlighting for AP logs
# 选择: AP log color scheme
```

方式 2 - 通过配置文件（prj.conf）：
```conf
CONFIG_ACOMP_LOGGER_USE_COLOR=y
CONFIG_ACOMP_LOGGER_COLOR_CYAN_BG=y
```

方式 3 - 禁用颜色：
```conf
CONFIG_ACOMP_LOGGER_USE_COLOR=n
```

### Remote 端配置

Remote 端无需配置，设备会自动注册并初始化。

## 注意事项

### 一般使用

1. **简单 API**：只需调用 `init()` 和 `start()`，无需手动管理流通道和缓冲区
2. **自动化管理**：所有资源管理由组件内部自动完成
3. **线程安全**：专用接收线程自动处理所有日志接收和输出
4. **优雅停止**：`acomp_logger_stop()` 会等待线程安全退出后再清理资源
5. **日志格式**：接收到的日志已是格式化字符串，直接通过 `lisa_log` 输出

### 配置建议

1. **缓冲区数量**：
   - 必须是 2 的幂次方（2, 4, 8, 16, 32, 64, 128）
   - 默认 128 个缓冲区适合大多数场景
   - 日志量大时可适当增加

2. **缓冲区大小**：
   - 默认 256 字节适合一般日志
   - 单条日志超过缓冲区大小会被截断
   - 根据实际日志长度调整

3. **触发模式选择**：
   - **手动触发**（推荐）：简单可靠，适合大多数场景
   - **自动触发**：减少轮询开销，适合日志密集的场景

4. **性能调优**：
   - 手动模式：减小轮询间隔可提高实时性，但增加 CPU 占用
   - 自动模式：调整缓冲区阈值平衡实时性和批处理效率
   - 线程优先级：根据系统负载调整，确保日志及时输出

5. **自定义输出**：
   - **默认行为**：不设置回调时，使用 `LISA_LOG_RAW` 输出，带颜色底纹
   - **自定义控制**：通过 `acomp_logger_set_output_callback()` 完全控制输出方式
   - **应用场景**：
     - 输出到不同目标（文件、网络、数据库）
     - 日志过滤和分类
     - 格式转换（JSON、XML 等）
     - 日志统计和分析
   - **性能注意**：回调函数在接收线程中执行，避免长时间阻塞

6. **颜色显示**：
   - 默认启用青色背景，便于区分 AP 日志
   - 可通过 Kconfig 选择不同颜色或禁用
   - 需要终端支持 ANSI 转义码
   - 自定义输出时可自行决定是否使用颜色

### 故障排查

1. **日志丢失**：增加缓冲区数量或大小
2. **延迟高**：减小轮询间隔或降低自动触发阈值
3. **CPU 占用高**：增加轮询间隔或使用自动触发模式
4. **栈溢出**：增加线程栈大小
5. **颜色不显示**：
   - 检查终端是否支持 ANSI 颜色码
   - 确认 `CONFIG_ACOMP_LOGGER_USE_COLOR=y`
   - 部分串口工具可能不支持颜色显示
6. **自定义输出无效**：
   - 确认在 `acomp_logger_init()` 之后调用
   - 检查回调函数是否正确注册
   - 验证回调函数返回值

## 文件结构

```
arcs-sdk/components/acomp/logger/
├── acomp_logger.c           # Master 端核心实现
├── acomp_logger.h           # Master 端公共 API
├── acomp_logger_example.c   # 使用示例代码
├── Kconfig                  # 配置选项定义
├── CMakeLists.txt           # Master 端构建配置
└── README.md                # 本文档

apps/aopu-yuba_ap/acomp/logger/
├── logger.c                 # Remote 端实现
├── logger.h                 # Remote 端头文件
├── Kconfig                  # Remote 端配置
└── CMakeLists.txt           # Remote 端构建配置
```

## 使用示例

完整的使用示例请参考 [acomp_logger_example.c](acomp_logger_example.c) 文件。

### 基础示例

```c
#include "acomp_logger.h"

#define TAG "my_app"
#include "lisa_log.h"

int main(void)
{
    int ret;

    /* 1. 初始化 logger 组件 */
    ret = acomp_logger_init();
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "Logger init failed: %d", ret);
        return ret;
    }

    /* 2. 启动 logger 组件 */
    ret = acomp_logger_start();
    if (ret != ACOMP_ERR_OK) {
        LISA_LOGE(TAG, "Logger start failed: %d", ret);
        return ret;
    }

    LISA_LOGI(TAG, "Logger started successfully!");

    /* 应用程序主循环 */
    while (1) {
        /* Remote 端的日志会自动显示在这里 */
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    /* 3. 停止 logger 组件（可选） */
    acomp_logger_stop();

    return 0;
}
```

### 自定义输出示例

#### 示例 1：输出到 printf

```c
#include "acomp_logger.h"

/* 自定义输出函数 */
int my_log_output(const uint8_t *log, uint32_t len)
{
    printf("[AP] %.*s", len, log);
    return 0;
}

int main(void)
{
    /* 初始化 */
    acomp_logger_init();

    /* 设置自定义输出 */
    acomp_logger_set_output_callback(my_log_output);

    /* 启动 */
    acomp_logger_start();

    /* AP 日志现在通过 printf 输出 */

    return 0;
}
```

#### 示例 2：带颜色高亮

```c
/* 带 ANSI 颜色码的输出 */
int colored_log_output(const uint8_t *log, uint32_t len)
{
    /* 青色背景 + 黑色文字 */
    printf("\033[46m\033[30m[AP]\033[0m %.*s", len, log);
    return 0;
}

int main(void)
{
    acomp_logger_init();
    acomp_logger_set_output_callback(colored_log_output);
    acomp_logger_start();

    return 0;
}
```

#### 示例 3：日志过滤

```c
/* 只输出错误日志 */
int error_only_output(const uint8_t *log, uint32_t len)
{
    const char *log_str = (const char *)log;

    /* 检查是否包含错误标记 */
    if (strstr(log_str, " E (") || strstr(log_str, "ERROR")) {
        printf("[AP ERROR] %.*s", len, log);
    }

    return 0;
}
```

#### 示例 4：写入文件

```c
#include <stdio.h>

static FILE *log_file = NULL;

/* 初始化日志文件 */
void log_file_init(void)
{
    log_file = fopen("/sdcard/ap_logs.txt", "a");
}

/* 输出到文件 */
int file_log_output(const uint8_t *log, uint32_t len)
{
    if (log_file) {
        fwrite(log, 1, len, log_file);
        fflush(log_file);  /* 立即刷新 */
    }
    return 0;
}

int main(void)
{
    log_file_init();

    acomp_logger_init();
    acomp_logger_set_output_callback(file_log_output);
    acomp_logger_start();

    return 0;
}
```

#### 示例 5：运行时切换输出方式

```c
int main(void)
{
    acomp_logger_init();
    acomp_logger_start();

    /* 阶段 1：使用默认输出 */
    LISA_LOGI(TAG, "Using default output");
    vTaskDelay(pdMS_TO_TICKS(5000));

    /* 阶段 2：切换到自定义输出 */
    acomp_logger_set_output_callback(my_log_output);
    LISA_LOGI(TAG, "Switched to custom output");
    vTaskDelay(pdMS_TO_TICKS(5000));

    /* 阶段 3：恢复默认输出 */
    acomp_logger_set_output_callback(NULL);
    LISA_LOGI(TAG, "Restored default output");
    vTaskDelay(pdMS_TO_TICKS(5000));

    acomp_logger_stop();
    return 0;
}
```

## 常见问题

### Q: 日志没有显示？

检查以下几点：
1. 确认 Remote 端已启用 logger 组件
2. 确认 Master 端已调用 `acomp_logger_start()`
3. 检查 Remote 端是否有日志输出
4. 检查配置参数是否正确

### Q: 如何提高日志传输性能？

1. 增加缓冲区数量（`ACOMP_LOGGER_STREAM_BUFFER_COUNT`）
2. 增大缓冲区大小（`ACOMP_LOGGER_STREAM_BUFFER_SIZE`）
3. 使用自动触发模式并调整阈值
4. 提高接收线程优先级

### Q: 日志出现乱码或截断？

1. 确认缓冲区大小足够容纳单条日志
2. 检查字符编码是否一致
3. 确认没有缓冲区溢出

## 更新日志

### v2.0
- 简化 API，移除手动管理函数
- 移除事件回调机制
- 新增 Kconfig 配置系统
- 支持自动触发和手动触发两种模式
- 增加缓冲区配置灵活性
- 优化线程管理和资源清理

### v1.0
- 初始版本
