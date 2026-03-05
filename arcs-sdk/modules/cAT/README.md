# cAT - C AT Command Parser

cAT是一个轻量级的AT命令解析器，适用于嵌入式系统中MCU之间通过串口进行通信控制。

## 项目来源

- **原始仓库**: https://github.com/marcinbor85/cAT
- **许可证**: MIT
- **作者**: Marcin Borowicz

## 功能特性

- 完整支持标准AT命令格式:
  - **RUN**: `AT+CMD` - 执行命令
  - **READ**: `AT+CMD?` - 读取命令状态
  - **WRITE**: `AT+CMD=<arg1>,<arg2>,...` - 写入命令参数
  - **TEST**: `AT+CMD=?` - 测试命令参数格式
- 支持多种变量类型：整数、十六进制、字符串、字节数组
- 支持命令分组管理
- 支持Unsolicited Response Code (URC) 主动上报
- 代码精简（核心约500行代码）

## ARCS SDK 集成

### Kconfig配置

在 `prj.conf` 中启用cAT模块:

```conf
# 启用cAT核心模块
CONFIG_SDK_MODULE_CAT=y

# 设置Unsolicited命令缓冲区大小（可选，默认1）
CONFIG_SDK_MODULE_CAT_UNSOLICITED_BUFFER_SIZE=4

CONFIG_CAT_UART_DEVICE_NAME="uart1"
CONFIG_CAT_UART_BAUDRATE=115200
CONFIG_CAT_UART_RX_BUFFER_SIZE=256
CONFIG_CAT_UART_TX_BUFFER_SIZE=256
```

### 基本使用

#### 1. 定义AT命令变量

```c
#include "cat.h"

/* 定义命令关联的变量 */
static int32_t led_state = 0;

static struct cat_variable led_vars[] = {
    {
        .name = "state",
        .type = CAT_VAR_INT_DEC,
        .data = &led_state,
        .data_size = sizeof(led_state),
        .access = CAT_VAR_ACCESS_READ_WRITE,
    },
};
```

#### 2. 定义AT命令处理函数

```c
/* AT+LED=<state> 写入处理 */
static cat_return_state cmd_led_write(const struct cat_command *cmd,
                                       const uint8_t *data, const size_t data_size,
                                       const size_t args_num)
{
    if (args_num < 1) {
        return CAT_RETURN_STATE_ERROR;
    }
    
    /* led_state 已经被cAT自动解析并更新 */
    printf("LED state set to: %d\n", led_state);
    
    /* 控制LED硬件 */
    gpio_write(LED_PIN, led_state);
    
    return CAT_RETURN_STATE_OK;
}

/* AT+LED? 读取处理 */
static cat_return_state cmd_led_read(const struct cat_command *cmd,
                                      uint8_t *data, size_t *data_size,
                                      const size_t max_data_size)
{
    /* 变量会被cAT自动格式化输出 */
    return CAT_RETURN_STATE_DATA_OK;
}
```

#### 3. 注册AT命令

```c
static struct cat_command commands[] = {
    {
        .name = "+LED",
        .description = "LED control (0=off, 1=on)",
        .write = cmd_led_write,
        .read = cmd_led_read,
        .var = led_vars,
        .var_num = sizeof(led_vars) / sizeof(led_vars[0]),
    },
};

static struct cat_command_group cmd_group = {
    .name = "device",
    .cmd = commands,
    .cmd_num = sizeof(commands) / sizeof(commands[0]),
};

static struct cat_command_group *cmd_groups[] = {
    &cmd_group,
};
```

#### 4. 初始化和运行

```c
/* 工作缓冲区 */
static uint8_t cat_buf[256];

/* 解析器描述符 */
static struct cat_descriptor cat_desc = {
    .cmd_group = cmd_groups,
    .cmd_group_num = 1,
    .buf = cat_buf,
    .buf_size = sizeof(cat_buf),
};

/* 解析器对象 */
static struct cat_object cat;

int main(void)
{
    /* 使用UART适配器 */
    cat_uart_config_t uart_config = {
        .uart_device_name = "uart1",
        .baudrate = 115200,
    };
    
    cat_uart_adapter_t *adapter = cat_uart_adapter_init(&uart_config);
    const struct cat_io_interface *io = cat_uart_adapter_get_io(adapter);
    
    /* 初始化解析器 */
    cat_init(&cat, &cat_desc, io, NULL);
    
    /* 主循环 */
    while (1) {
        cat_uart_adapter_process(adapter, &cat);
    }
}
```

### 使用RTOS后台任务

```c
#include "cat_uart_adapter.h"

int main(void)
{
    /* ... 初始化代码 ... */
    
    /* 启动后台处理任务 */
    cat_uart_adapter_start_service(adapter, &cat);
    
    /* 主循环可以执行其他任务 */
    while (1) {
        /* 应用程序逻辑 */
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

### 发送URC（主动上报）

```c
/* 定义URC命令 */
static struct cat_command urc_cmd = {
    .name = "+EVENT",
    .read = event_read_handler,
    .var = event_vars,
    .var_num = 1,
};

/* 触发URC */
void trigger_event(int event_code)
{
    event_value = event_code;
    cat_trigger_unsolicited_read(&cat, &urc_cmd);
}
```

## AT命令测试示例

通过串口发送以下命令进行测试:

```
AT                    -> OK
AT+LED=1              -> OK
AT+LED?               -> +LED: 1\r\nOK
AT+LED=?              -> +LED: state(INT)\r\nOK
AT+VERSION?           -> 1.0.0\r\nOK
AT+RESET              -> OK
```

## API参考

### 核心函数

| 函数 | 描述 |
|------|------|
| `cat_init()` | 初始化AT命令解析器 |
| `cat_service()` | 处理AT命令（需周期性调用） |
| `cat_is_busy()` | 检查解析器是否正在处理命令 |
| `cat_is_hold()` | 检查解析器是否处于保持状态 |
| `cat_hold_exit()` | 退出保持状态 |

### URC函数

| 函数 | 描述 |
|------|------|
| `cat_trigger_unsolicited_read()` | 触发读取类型URC |
| `cat_trigger_unsolicited_test()` | 触发测试类型URC |
| `cat_is_unsolicited_buffer_full()` | 检查URC缓冲区是否已满 |

### UART适配器函数

| 函数 | 描述 |
|------|------|
| `cat_uart_adapter_init()` | 初始化UART适配器 |
| `cat_uart_adapter_deinit()` | 释放UART适配器资源 |
| `cat_uart_adapter_get_io()` | 获取IO接口 |
| `cat_uart_adapter_process()` | 处理UART数据和AT命令 |
| `cat_uart_adapter_start_task()` | 启动后台处理任务 |
| `cat_uart_adapter_stop_task()` | 停止后台处理任务 |

## 变量类型

| 类型 | 描述 | 示例 |
|------|------|------|
| `CAT_VAR_INT_DEC` | 有符号整数（十进制） | -123, 456 |
| `CAT_VAR_UINT_DEC` | 无符号整数（十进制） | 123, 456 |
| `CAT_VAR_NUM_HEX` | 无符号整数（十六进制） | 0x1A2B |
| `CAT_VAR_BUF_HEX` | 字节数组（十六进制） | "48454C4C4F" |
| `CAT_VAR_BUF_STRING` | 字符串 | "Hello" |

## 命令回调返回值

| 返回值 | 描述 |
|--------|------|
| `CAT_RETURN_STATE_OK` | 成功，输出 "OK" |
| `CAT_RETURN_STATE_ERROR` | 失败，输出 "ERROR" |
| `CAT_RETURN_STATE_DATA_OK` | 成功，输出数据后跟 "OK" |
| `CAT_RETURN_STATE_DATA_NEXT` | 输出数据，继续下一次回调 |
| `CAT_RETURN_STATE_HOLD` | 保持状态，延迟响应 |
| `CAT_RETURN_STATE_PRINT_CMD_LIST_OK` | 输出命令列表后跟 "OK" |

## 目录结构

```
arcs-sdk/modules/cat/
├── CMakeLists.txt      # ARCS SDK构建配置
├── Kconfig             # 配置选项
├── README.md           # 本文档
├── src/
│   ├── cat.c           # cAT核心实现
│   └── cat.h           # cAT头文件
└── port/
    ├── cat_uart_adapter.h  # UART适配器头文件
    └── cat_uart_adapter.c  # UART适配器实现
```

## 许可证

MIT License - 详见 [LICENSE](src/cat.h) 文件头部。