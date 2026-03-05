# cAT AT 命令解析器基础示例

## 功能说明

演示如何使用 cAT 库创建一个简单的 AT 命令接口。通过 UART 接收命令、解析执行，并返回结果。

本示例包含的 AT 命令可用于查询系统状态、控制设备以及管理参数。

## 硬件连接

- **UART1_TX (PB2)**: 传输数据，连接到 USB-UART 转接器的 RX
- **UART1_RX (PB3)**: 接收数据，连接到 USB-UART 转接器的 TX

连接到 PC 串口工具，配置为 **115200, 8N1, 无流控**

## 示例内容

本示例演示以下功能：

1. 初始化 cAT 解析器并绑定 UART 通信接口
2. 通过 UART 接收 AT 命令并实时解析
3. 查询固件版本（`AT+VERSION?`）
4. 控制 LED 状态（`AT+LED=<state>`）
5. 读取和设置 GPIO 引脚（`AT+GPIO=<pin>,<value>`）
6. 执行设备重置（`AT+RESET`）
7. 列出所有可用命令（`AT+HELP`）

## 编译运行

```bash
./build.sh -C -DBOARD=arcs_evb
```

## 预期输出

**系统启动输出：**

```
[I][cat_example] cAT AT Command Parser Example
[I][cat_example] =============================
[I][cat_uart] cAT UART adapter initialized on uart1 @ 115200 baud
[I][cat_example] AT command parser ready on uart1
[I][cat_example] Try commands: AT+VERSION?, AT+LED=1, AT+HELP
[I][cat_example] Supported AT commands:
[I][cat_example]   +VERSION - Firmware version
[I][cat_example]   +LED - LED control (0=off, 1=on)
[I][cat_example]   +GPIO - GPIO control (pin, value)
[I][cat_example]   +RESET - Reset device
[I][cat_example]   +HELP - List all commands
[I][cat_uart] cAT UART task started
```

**发送 `AT+VERSION?` 后的输出：**

```
+VERSION: 1.0.0
OK
```

**发送 `AT+LED=1` 后的输出：**

```
[I][cat_example] LED state set to: 1
OK
```

**发送 `AT+GPIO=5,1` 后的输出：**

```
[I][cat_example] GPIO 5 set to: 1
OK
```

**发送 `AT+HELP` 后的输出：**

```
+HELP: +VERSION(RO),+LED(RW),+GPIO(RW),+RESET(RUN),+HELP(RUN)
OK
```

## 核心 API

| API | 说明 |
|-----|------|
| `cat_uart_adapter_init()` | 初始化 UART 适配器 |
| `cat_uart_adapter_start_service()` | 启动后台处理服务（自动初始化 cAT 解析器） |
| `cat_uart_adapter_stop_service()` | 停止后台处理服务 |
| `cat_uart_adapter_deinit()` | 清理 UART 适配器 |
| `cat_uart_adapter_send_urc()` | 发送非请求响应（URC） |

## 关键代码

**初始化 UART 适配器和启动服务：**

```c
/* 配置 UART 适配器 */
cat_uart_config_t uart_config = {
    .uart_device_name = "uart1",
    .baudrate = 115200,
};

/* 初始化适配器 */
cat_uart_adapter_t *adapter = cat_uart_adapter_init(&uart_config);

/* 启动后台处理任务（自动初始化 cAT 解析器） */
cat_uart_adapter_start_service(adapter, &cat, &cat_desc);
```

**定义 AT 命令和变量：**

```c
/* LED 控制命令的变量 */
static int32_t led_state = 0;
static struct cat_variable led_vars[] = {
    {
        .name = "state",
        .type = CAT_VAR_INT_DEC,
        .data = &led_state,
        .data_size = sizeof(led_state),
        .access = CAT_VAR_ACCESS_READ_WRITE,
    }
};

/* LED 命令定义 */
static struct cat_command commands[] = {
    {
        .name = "+LED",
        .description = "LED control (0=off, 1=on)",
        .write = cmd_led_write,
        .read = cmd_led_read,
        .var = led_vars,
        .var_num = sizeof(led_vars) / sizeof(led_vars[0]),
    },
    /* 其他命令... */
};
```

**命令处理器实现：**

```c
/* LED 写入处理器 - 处理 AT+LED=<state> */
static cat_return_state cmd_led_write(const struct cat_command *cmd,
                                       const uint8_t *data, const size_t data_size,
                                       const size_t args_num)
{
    if (args_num < 1) {
        return CAT_RETURN_STATE_ERROR;
    }

    LOGI("LED state set to: %d", (int)led_state);

    if (led_state < 0 || led_state > 1) {
        LOGE("Invalid LED state: %d", (int)led_state);
        return CAT_RETURN_STATE_ERROR;
    }

    return CAT_RETURN_STATE_OK;
}

/* LED 读取处理器 - 处理 AT+LED? */
static cat_return_state cmd_led_read(const struct cat_command *cmd,
                                      uint8_t *data, size_t *data_size,
                                      const size_t max_data_size)
{
    return CAT_RETURN_STATE_DATA_OK;
}
```

## 注意事项

1. **UART 连接**: 示例需要通过 UART 通信，确保硬件连接正确，波特率设置为 115200

2. **串口工具配置**: 使用任何串口调试工具（如 PuTTY、Minicom 或 VS Code Serial Monitor）连接，参数为 115200, 8N1, 无流控

3. **命令格式**: AT 命令必须以 `\r\n` 结尾，写入命令后需按 Enter 键发送

4. **变量访问**: 
   - `CAT_VAR_ACCESS_READ_WRITE`: 支持读写
   - `CAT_VAR_ACCESS_READ_ONLY`: 仅读
   - `CAT_VAR_ACCESS_WRITE_ONLY`: 仅写

5. **返回值检查**: 所有 API 调用都应检查返回值，处理可能的错误情况

6. **线程安全**: cAT 解析器支持多任务环境，互斥锁由适配器自动管理

## 命令参考

| 命令 | 说明 | 示例 |
|------|------|------|
| `AT+VERSION?` | 查询固件版本 | `AT+VERSION?` → `1.0.0\r\nOK` |
| `AT+LED=<state>` | 设置 LED 状态 | `AT+LED=1` → `OK` |
| `AT+LED?` | 查询 LED 状态 | `AT+LED?` → `+LED: 1\r\nOK` |
| `AT+GPIO=<pin>,<value>` | 设置 GPIO | `AT+GPIO=5,1` → `OK` |
| `AT+GPIO?` | 查询 GPIO（需实现） | `AT+GPIO?` → `+GPIO: 5,1\r\nOK` |
| `AT+RESET` | 重置设备 | `AT+RESET` → `OK` |
| `AT+HELP` | 列出所有命令 | `AT+HELP` → `+HELP: ...\r\nOK` |

