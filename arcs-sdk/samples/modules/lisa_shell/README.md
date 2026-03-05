# Shell 示例

## 功能说明

演示 LISA Shell 组件的完整使用方法,通过5个典型示例展示如何创建和注册各种类型的 Shell 命令。

本示例是学习 LISA Shell 命令开发的完整教程,涵盖从简单命令到复杂命令组的各种使用场景,每个示例都包含详细的中文注释说明。

### 示例内容

- **示例1: 简单命令** - 不带参数的基础命令 (version)
- **示例2: 带参数命令** - 处理用户输入参数 (echo)
- **示例3: 可选参数** - 必选参数+可选参数的组合使用 (memread)
- **示例4: 命令组** - 组织多个相关子命令 (info)
- **示例5: 命令选项** - 处理命令行选项参数 (list)

## 硬件连接

- **PA2**: UART0 RX (接收)
- **PA3**: UART0 TX (发送)

连接到 PC 串口工具,配置为 **115200, 8N1, 无流控**

## 使用场景

适用于需要为嵌入式应用添加 Shell 命令行接口的场景:
- 学习如何创建自定义 Shell 命令
- 实现系统调试和配置接口
- 开发交互式命令行工具
- 提供设备远程控制功能

## 示例步骤

1. 初始化 LISA Shell 组件
2. 实现5种不同类型的命令示例
3. 使用 `SHELL_EXPORT_CMD` 宏注册命令
4. 通过串口工具连接并执行命令
5. 观察各种命令的实现和效果

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 预期输出

**终端日志:**
```
[INFO] [shell] === LISA Shell Usage Sample ===
[INFO] [shell] Shell initialized successfully
[INFO] [shell]
[INFO] [shell] This sample demonstrates:
[INFO] [shell]   1. Simple command (version)
[INFO] [shell]   2. Command with parameters (echo)
[INFO] [shell]   3. Optional parameters (memread)
[INFO] [shell]   4. Command group (info)
[INFO] [shell]   5. Command options (list)
[INFO] [shell]
[INFO] [shell] Type 'help' in shell to see all commands
```

**串口工具交互:**

```
letter shell 3.1
arcs@shell:/>help
version              : show system version
echo                 : echo the input message
memread              : read memory content
info                 : system information commands
list                 : list files (-l: verbose, -a: all)
...

arcs@shell:/>version
ARCS SDK Version: 1.0.0
Build Date: Jan 15 2025 10:30:45

arcs@shell:/>echo hello world
hello world

arcs@shell:/>memread 0x20000000 32

Memory at 0x20000000 (32 bytes):
         00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F
-------------------------------------------------------
20000000: 12 34 56 78 9A BC DE F0 11 22 33 44 55 66 77 88
20000010: AA BB CC DD EE FF 00 11 22 33 44 55 66 77 88 99

arcs@shell:/>info

Usage: info <subcommand>

Available subcommands:
  version      - Show version
  tasks        - Show task list

arcs@shell:/>info version
Version: 1.0.0

arcs@shell:/>info tasks

Task List:
Name                State  Pri  Stack  Num
---------------------------------------------
IDLE                R      0    128    1
shell_task          B      5    512    2
main_task           B      10   256    3
---------------------------------------------

arcs@shell:/>list

File List:
main.c  shell.c

arcs@shell:/>list -l

File List:
Name                      Size       Date
------------------------------------------
main.c                    1234       2025-01-15
shell.c                   5678       2025-01-14
```

## 核心 API

| API | 说明 |
|-----|------|
| `lisa_shell_init()` | 初始化 LISA Shell 组件 |
| `SHELL_EXPORT_CMD()` | 注册命令到 Shell |
| `printf()` | 在 Shell 中输出信息 |
| `LISA_LOGI()` / `LISA_LOGE()` | 输出日志信息 |

## 示例详解

### 示例1: 简单命令 (version)

演示最基本的命令实现,不需要处理任何参数。

```c
static int cmd_version(int argc, char **argv)
{
    printf("ARCS SDK Version: 1.0.0\n");
    printf("Build Date: %s %s\n", __DATE__, __TIME__);
    return 0;
}

/* 注册命令 */
SHELL_EXPORT_CMD(
    SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
    version,              /* Shell 中的命令名 */
    cmd_version,          /* 命令处理函数 */
    show system version   /* 帮助信息 */
);
```

**要点:**
- 命令函数返回 0 表示成功,-1 表示失败
- 使用 `printf()` 输出信息到 Shell
- `SHELL_EXPORT_CMD` 必须在全局作用域使用

### 示例2: 带参数命令 (echo)

演示如何接收和处理用户输入的参数。

```c
static int cmd_echo(int argc, char **argv)
{
    /* argc: 参数个数 */
    /* argv: 参数数组,argv[0]是第一个参数 */

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
```

**要点:**
- `argc` 不包括命令名本身,只计算参数个数
- `argv[0]` 是第一个参数(不是命令名)
- 需要检查参数个数,避免数组越界

### 示例3: 可选参数 (memread)

演示如何处理必选参数和可选参数的组合。

```c
static int cmd_memread(int argc, char **argv)
{
    uint32_t addr;
    uint32_t len = 64;  /* 可选参数的默认值 */

    if (argc < 1) {
        /* 必选参数缺失 */
        printf("Usage: memread <address> [length]\n");
        return -1;
    }

    /* 解析必选参数 */
    addr = strtoul(argv[0], NULL, 0);

    /* 解析可选参数 */
    if (argc >= 2) {
        len = strtoul(argv[1], NULL, 0);
    }

    /* 使用参数... */
}
```

**要点:**
- 可选参数需要设置合理的默认值
- 使用 `strtoul()` 可以自动识别十进制和十六进制(0x前缀)
- 需要对参数进行有效性检查

### 示例4: 命令组 (info)

演示如何组织多个相关子命令为一个命令组。

```c
/* 定义子命令表 */
static const struct shell_cmd_entry info_cmds[] = {
    {"version", subcmd_info_version, "Show version"},
    {"tasks",   subcmd_info_tasks,   "Show task list"},
};

/* 命令组处理函数 */
static int cmd_info_handler(int argc, char **argv)
{
    /* argc包括命令名, argv[0]是命令名 info */
    /* argv[1] 是子命令名 */

    if (argc == 1) {
        /* 没有子命令,显示帮助 */
        info_show_help();
        return 0;
    }

    /* 查找子命令 */
    const char *subcmd = argv[1];
    for (int i = 0; i < count; i++) {
        if (strcmp(info_cmds[i].name, subcmd) == 0) {
            /* 执行子命令,传递剩余参数 */
            return info_cmds[i].handler(argc - 2, argv + 2);
        }
    }

    /* 未找到子命令 */
    printf("Error: unknown subcommand '%s'\n", subcmd);
    return -1;
}

/* 注册命令组(注意使用 SHELL_CMD_DISABLE_RETURN 标志) */
SHELL_EXPORT_CMD(
    SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN,
    info,
    cmd_info_handler,
    system information commands
);
```

**要点:**
- 命令组的 `argc` 包括命令名,`argv[0]` 是命令名
- 子命令名在 `argv[1]`
- 传递给子命令的参数需要调整: `argc-2`, `argv+2`
- 命令组通常添加 `SHELL_CMD_DISABLE_RETURN` 标志

### 示例5: 命令选项 (list)

演示如何解析和处理类似 `-l`, `-a` 这样的命令行选项。

```c
static int cmd_list(int argc, char **argv)
{
    int verbose = 0;   /* -l 选项 */
    int show_all = 0;  /* -a 选项 */

    /* 解析选项 */
    for (int i = 0; i < argc; i++) {
        if (argv[i][0] == '-') {
            /* 这是一个选项 */
            for (int j = 1; argv[i][j] != '\0'; j++) {
                if (argv[i][j] == 'l') {
                    verbose = 1;
                } else if (argv[i][j] == 'a') {
                    show_all = 1;
                }
            }
        }
    }

    /* 根据选项执行不同的逻辑 */
    if (verbose) {
        /* 详细输出 */
    } else {
        /* 简单输出 */
    }
}
```

**要点:**
- 选项以 `-` 开头
- 支持组合选项如 `-la`
- 根据选项标志执行不同的逻辑

## 配置说明

在 prj.conf 中需要启用以下配置:

```c
# 启用 LISA Shell
CONFIG_LISA_SHELL=y
```

## 注意事项

1. **命令注册位置**: `SHELL_EXPORT_CMD` 宏必须在全局作用域使用,不能在函数内部
2. **参数索引**: 命令函数的 `argc` 不包括命令名,`argv[0]` 是第一个参数
3. **命令组参数**: 命令组处理函数的 `argc` 包括命令名,`argv[0]` 是命令名本身
4. **返回值**: 命令函数返回 0 表示成功,-1 表示失败
5. **日志接口**: 使用 `LISA_LOGI()` / `LISA_LOGE()` 输出日志,消息使用英文
6. **注释规范**: 代码注释使用中文,便于理解功能
7. **内存安全**: 访问内存地址前需要检查有效性,避免系统崩溃
