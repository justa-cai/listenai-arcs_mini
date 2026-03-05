# LISA UART 轮询发送示例（轮询模式）

## 功能说明

演示如何使用 `lisa_uart_poll_out()` API 以轮询方式发送单字节数据。

底层使用轮询方式阻塞发送每个字节，适用于简单的调试输出场景。

## 硬件连接

- **PB2**: UART1 TX（发送）
- **PB3**: UART1 RX（接收）

连接到 PC 串口工具，配置为 **115200, 8N1, 无流控**

## 使用场景

适用于以下场景：
- 简单的调试输出
- 低速率字节发送
- 不需要中断/DMA的场景
- 对实时性要求不高的应用

## 示例步骤

1. 获取 UART 设备
2. 配置引脚
3. 配置 UART
4. 循环轮询发送每个字节

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 预期输出

**终端输出：**
```
=== LISA UART poll_out example ===
UART ready, start polling send...

Sending message 0: Sent
Sending message 1: Sent
Sending message 2: Sent
...
```

**PC 串口工具接收：**
```
Hello UART!
Hello UART!
Hello UART!
...
```

## 核心 API

| API | 说明 |
|-----|------|
| `lisa_device_get()` | 获取 UART 设备 |
| `lisa_uart_configure()` | 配置 UART 参数 |
| `lisa_uart_poll_out()` | 轮询发送单字节（阻塞） |

## 轮询发送说明

`lisa_uart_poll_out()` 是一个**同步阻塞**的单字节发送接口：
- **发送单个字节**：每次调用只发送一个字节
- **阻塞等待**：调用后阻塞等待直到发送完成
- **轮询方式**：底层不使用中断或DMA，直接轮询硬件状态
- **返回值**：发送成功返回0，失败返回负数错误码
- **适用场景**：简单的字节级发送，如调试输出

## 关键代码

```c
/* 1. 获取并检查设备 */
lisa_device_t *uart_dev = lisa_device_get("uart1");

/* 2. 配置 UART (115200, 8N1) */
lisa_uart_config_t config = LISA_UART_CONFIG_DEFAULT();
lisa_uart_configure(uart_dev, &config);

/* 3. 轮询发送每个字节 */
char msg[] = "Hello UART!\r\n";
for (int i = 0; i < strlen(msg); i++) {
    lisa_uart_poll_out(uart_dev, (unsigned char)msg[i]);
}
```

## 注意事项

1. **阻塞等待**：`lisa_uart_poll_out()` 会阻塞当前任务，直到字节发送完成
2. **单字节发送**：每次只能发送一个字节，不适合大批量数据传输
3. **CPU占用高**：轮询方式会持续占用CPU，不适合实时性要求高的场景
4. **简单场景**：适合调试输出等简单场景，生产环境建议使用中断或DMA模式
