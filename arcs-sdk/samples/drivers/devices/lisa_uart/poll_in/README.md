# LISA UART 轮询接收示例（轮询模式）

## 功能说明

演示如何使用 `lisa_uart_poll_in()` API 以轮询方式接收单字节数据。

底层使用轮询方式非阻塞接收每个字节，适用于简单的字符输入场景。

## 硬件连接

- **PB2**: UART1 TX（发送）
- **PB3**: UART1 RX（接收）

连接到 PC 串口工具，配置为 **115200, 8N1, 无流控**

## 使用场景

适用于以下场景：
- 简单的字符输入
- 命令行交互
- 低速率字节接收
- 不需要中断/事件的场景

## 示例步骤

1. 获取 UART 设备
2. 配置引脚
3. 配置 UART
4. 循环轮询检查是否有数据
5. 调用 `lisa_uart_poll_in()` 尝试接收
6. 如果成功，处理接收到的字节
7. 短暂延时，避免CPU占用过高

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 预期输出

**终端输出：**
```
=== LISA UART poll_in example ===
UART ready, waiting for data...
Please send data via serial tool.

Received: 0x48 ('H')
Received: 0x65 ('e')
Received: 0x6C ('l')
Received: 0x6C ('l')
Received: 0x6F ('o')
...
```

**PC 串口工具发送：**
```
Hello
```

程序会实时显示接收到的每个字节，可打印字符会同时显示十六进制值和字符。

## 核心 API

| API | 说明 |
|-----|------|
| `lisa_device_get()` | 获取 UART 设备 |
| `lisa_uart_configure()` | 配置 UART 参数 |
| `lisa_uart_poll_in()` | 轮询接收单字节（非阻塞） |

## 轮询接收说明

`lisa_uart_poll_in()` 是一个**非阻塞**的单字节接收接口：
- **尝试接收一个字节**：每次调用尝试读取一个字节
- **立即返回**：不阻塞，无数据时立即返回错误码
- **轮询方式**：底层不使用中断，直接读取硬件FIFO状态
- **返回值**：
  - `0`：成功接收到数据
  - `LISA_DEVICE_ERR_TIMEOUT`：无数据可读
  - 其他负数：错误码
- **适用场景**：轮询检查是否有数据到达

## 关键代码

```c
/* 1. 获取并检查设备 */
lisa_device_t *uart_dev = lisa_device_get("uart1");

/* 2. 配置 UART (115200, 8N1) */
lisa_uart_config_t config = LISA_UART_CONFIG_DEFAULT();
lisa_uart_configure(uart_dev, &config);

/* 3. 循环轮询接收 */
while (1) {
    unsigned char ch;
    int ret = lisa_uart_poll_in(uart_dev, &ch);

    if (ret == 0) {
        /* 成功接收到数据 */
        if (ch >= 32 && ch <= 126) {
            printf("Received: 0x%02X ('%c')\n", ch, ch);
        } else {
            printf("Received: 0x%02X\n", ch);
        }
    }

    /* 短暂延时，避免CPU占用过高 */
    vTaskDelay(pdMS_TO_TICKS(10));
}
```

## 注意事项

1. **非阻塞调用**：`lisa_uart_poll_in()` 立即返回，无数据时不等待
2. **单字节接收**：每次只能接收一个字节，需要循环调用
3. **CPU占用**：需要循环轮询，建议添加适当延时以降低CPU占用
4. **简单场景**：适合简单字符输入，生产环境建议使用中断或DMA模式
