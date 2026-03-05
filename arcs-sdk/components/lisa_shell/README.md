# Shell 组件

## 简介

LISA Shell 是一个基于 [Letter Shell](https://github.com/NevermindZZT/letter-shell) 的命令行交互组件，为 ARCS SDK 提供串口命令行界面支持。通过 UART 接口，用户可以在终端中执行自定义命令、查看系统信息、调试应用程序等。

## 主要特性

- **UART 串口通信**：基于 lisa_uart 驱动实现串口收发
- **FreeRTOS 任务集成**：独立的 shell 任务处理用户输入
- **灵活配置**：通过 Kconfig 配置任务优先级、缓冲区大小等参数
- **日志集成**：自动接管系统日志输出到串口
- **命令扩展**：支持使用 `SHELL_EXPORT_CMD` 宏注册自定义命令
- **多种回车模式**：支持 LF、CR、CRLF 等多种命令行回车触发方式

## 组件架构

```
┌─────────────────────────────────────────┐
│           用户应用程序                    │
│  ┌──────────────────────────────────┐   │
│  │  SHELL_EXPORT_CMD 宏注册命令      │   │
│  └──────────────────────────────────┘   │
└─────────────┬───────────────────────────┘
              │
┌─────────────▼───────────────────────────┐
│          LISA Shell 组件                 │
│  ┌──────────────┐  ┌──────────────┐    │
│  │ shell_task   │  │ shell_write  │    │
│  │  (接收处理)   │  │  (输出函数)   │    │
│  └──────┬───────┘  └──────┬───────┘    │
│         │                 │             │
│  ┌──────▼─────────────────▼───────┐    │
│  │    Letter Shell 核心库         │    │
│  │  (命令解析、命令管理、TAB补全)  │    │
│  └──────────────────────────────┬─┘    │
└─────────────────────────────────┼──────┘
                                  │
┌─────────────────────────────────▼──────┐
│          lisa_uart 驱动                 │
│  (UART 接收/发送、缓冲区管理)            │
└────────────────────────────────────────┘
```

## 配置选项

在 `menuconfig` 或 `prj.conf` 中可配置以下选项：

### 基本配置

| 配置项 | 说明 | 默认值 |
|--------|------|--------|
| `CONFIG_LISA_SHELL` | 启用 LISA Shell 组件 | n |

### 任务配置

| 配置项 | 说明 | 默认值 | 范围 |
|--------|------|--------|------|
| `CONFIG_LISA_SHELL_TASK_STACK_SIZE` | Shell 任务栈大小 (字节) | 1024 | - |
| `CONFIG_LISA_SHELL_TASK_PRIORITY` | Shell 任务优先级 | 5 | 0-31 |

### 缓冲区配置

| 配置项 | 说明 | 默认值 |
|--------|------|--------|
| `CONFIG_LISA_SHELL_BUFFER_SIZE` | Shell 命令缓冲区大小 (字节) | 512 |
| `CONFIG_LISA_SHELL_RX_BUF_SIZE` | UART 接收缓冲区大小 (字节) | 32 |
| `CONFIG_LISA_SHELL_SCAN_BUFFER` | 格式化输入缓冲区大小 (字节) | 128 |

### 行为配置

| 配置项 | 说明 | 默认值 |
|--------|------|--------|
| `CONFIG_LISA_SHELL_SUPPORT_END_LINE` | 支持尾行模式 | y |
| `CONFIG_LISA_SHELL_ENTER_LF` | 使用 LF (`\n`) 作为回车触发 | y |
| `CONFIG_LISA_SHELL_ENTER_CR` | 使用 CR (`\r`) 作为回车触发 | y |
| `CONFIG_LISA_SHELL_ENTER_CRLF` | 使用 CRLF (`\r\n`) 作为回车触发 | n |

> **注意**：`SHELL_ENTER_CRLF` 不能与 `SHELL_ENTER_LF` 或 `SHELL_ENTER_CR` 同时启用。

## 快速开始

### 1. 启用组件

在项目的 `prj.conf` 文件中添加：

```c
CONFIG_LISA_SHELL=y
```

### 2. 配置 UART 设备

LISA Shell 使用系统日志 UART 设备，需要配置对应的 UART：

```c
# 选择 UART0 作为 Shell 串口
CONFIG_SYSLOG_UART_DEVICE_UART0=y
```

### 3. 初始化 Shell

在应用程序的 `main` 函数中调用初始化函数：

```c
#include "lisa_shell.h"

int main(int argc, char **argv)
{
    // 初始化 shell
    lisa_shell_init();

    // 你的应用代码
    while (1) {
        vTaskDelay(1000);
    }

    return 0;
}
```

### 4. 注册自定义命令

使用 `SHELL_EXPORT_CMD` 宏注册命令：

```c
#include "shell.h"

// 简单命令示例
static int cmd_hello(int argc, char **argv)
{
    printf("Hello World\n");
    return 0;
}

// 带参数的命令示例
static int cmd_echo(int argc, char **argv)
{
    if (argc < 1) {
        printf("Usage: echo <message>\n");
        return -1;
    }

    for (int i = 0; i < argc; i++) {
        printf("%s ", argv[i]);
    }
    printf("\n");

    return 0;
}

// 注册命令
SHELL_EXPORT_CMD(
    SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
    hello,
    cmd_hello,
    "Print hello world"
);

SHELL_EXPORT_CMD(
    SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
    echo,
    cmd_echo,
    "Echo arguments"
);
```

### 5. 命令组示例

创建命令组来组织相关的命令：

```c
// 命令表
struct cmd_entry {
    char *name;
    int (*exec)(int argc, char **argv);
    char *help;
};

static int subcmd_test(int argc, char **argv)
{
    printf("Running test...\n");
    return 0;
}

static int subcmd_info(int argc, char **argv)
{
    printf("System information\n");
    return 0;
}

static const struct cmd_entry my_cmds[] = {
    {"test", subcmd_test, "Run test"},
    {"info", subcmd_info, "Show info"},
};

// 命令组处理函数
static int cmd_group_handler(int argc, char **argv)
{
    if (argc == 1) {
        // 显示帮助
        printf("Available subcommands:\n");
        for (int i = 0; i < sizeof(my_cmds) / sizeof(my_cmds[0]); i++) {
            printf("  %-10s : %s\n", my_cmds[i].name, my_cmds[i].help);
        }
        return 0;
    }

    // 查找并执行子命令
    for (int i = 0; i < sizeof(my_cmds) / sizeof(my_cmds[0]); i++) {
        if (strcmp(my_cmds[i].name, argv[1]) == 0) {
            return my_cmds[i].exec(argc - 2, argv + 2);
        }
    }

    printf("Unknown subcommand: %s\n", argv[1]);
    return -1;
}

// 注册命令组
SHELL_EXPORT_CMD(
    SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN,
    myapp,
    cmd_group_handler,
    "My application command group"
);
```

## API 参考

### lisa_shell_init

```c
int lisa_shell_init(void);
```

**功能**：初始化 LISA Shell 组件

**返回值**：
- `0`：初始化成功
- `-1`：初始化失败（UART 设备未就绪、配置失败或内存分配失败）

**说明**：
- 自动配置 UART 设备（波特率、缓冲区等）
- 创建 shell 任务进行命令接收和处理
- 将系统日志输出重定向到 shell 串口


## 注意事项

1. **UART 设备共享**：Shell 使用系统日志的 UART 设备，确保日志和 Shell 配置一致
2. **任务优先级**：根据系统需求调整 Shell 任务优先级，避免影响实时性要求高的任务
3. **缓冲区大小**：根据命令复杂度和长度调整 `SHELL_BUFFER_SIZE`
4. **内存分配**：Shell 使用动态内存分配，确保系统有足够的堆内存
5. **命令注册**：`SHELL_EXPORT_CMD` 宏必须在全局作用域使用，不能在函数内部
6. **格式化输入**：`SHELL_SCAN_BUFFER` 功能会阻塞 shell 任务，仅适用于 RTOS 环境

## 常见问题

**Q: Shell 初始化失败怎么办？**

A: 检查以下几点：
- UART 设备是否正确配置并初始化
- 系统是否有足够的堆内存
- `CONFIG_SYSLOG_UART_DEVICE_UARTx` 配置是否正确

**Q: 无法输入命令？**

A: 检查：
- 串口终端配置（波特率、数据位等）是否正确
- UART 引脚连接是否正常
- 回车触发模式（LF/CR/CRLF）配置是否与终端匹配


**Q: 命令注册后在 help 中看不到？**

A: 确保：
- 命令正确使用了 `SHELL_EXPORT_CMD` 宏
- 宏调用在全局作用域（不在函数内部）
- 对应的源文件已链接到最终二进制文件

## 依赖项

- `SDK_MODULE_LETTER_SHELL`：Letter Shell 核心库
- `LISA_PORTING`：LISA 平台移植层
- `lisa_uart`：UART 驱动
- `lisa_log`：日志系统
- `lisa_mem`：内存管理
- `FreeRTOS`：实时操作系统

