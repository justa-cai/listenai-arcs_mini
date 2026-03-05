# LISA UART 同步发送示例（中断模式）

## 功能说明

演示在中断模式下使用 UART 同步发送接口。

底层使用中断方式异步发送，通过信号量实现同步等待，避免轮询，降低 CPU 占用。

## 硬件连接

- **PB2**: UART1 TX（发送）
- **PB3**: UART1 RX（接收）

连接到 PC 串口工具，配置为 **115200, 8N1, 无流控**

## 使用场景

适用于需要阻塞等待发送完成的场景，底层采用中断模式传输，应用层代码简洁。

## 示例步骤

1. 获取 UART 设备
2. 配置引脚和参数
3. 调用 `lisa_uart_write_sync()` 同步发送
4. 底层通过中断发送，任务等待信号量
5. 发送完成后自动返回

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 预期输出

**终端输出：**
```
=== UART Sync Send (Interrupt Mode) ===
UART configured in INT mode, start sending...
Sent: Hello UART (sync, INT)! Counter: #0
Sent: Hello UART (sync, INT)! Counter: #1
Sent: Hello UART (sync, INT)! Counter: #2
...
```

**PC 串口工具接收：**
```
Hello UART (sync, INT)! Counter: #0
Hello UART (sync, INT)! Counter: #1
Hello UART (sync, INT)! Counter: #2
...
```

每秒发送一条消息，计数器递增。

## 核心 API

| API | 说明 |
|-----|------|
| `lisa_device_get()` | 获取 UART 设备 |
| `lisa_uart_configure()` | 配置 UART 参数 |
| `lisa_uart_write_sync()` | 同步发送数据（阻塞等待） |

## 同步发送说明

`lisa_uart_write_sync()` 在中断模式下的工作原理：
- **底层实现**：通过中断方式异步发送数据，内部使用信号量等待完成
- **阻塞等待**：调用后阻塞当前任务，直到发送完成或超时
- **返回时机**：中断触发发送完成事件后返回
- **返回值**：
  - 成功时返回实际发送字节数（> 0）
  - 超时时返回 `LISA_DEVICE_ERR_TIMEOUT`
  - 失败时返回其他负数错误码

## 关键代码

```c
/* 1. 获取并检查设备 */
lisa_device_t *uart_dev = lisa_device_get("uart1");

/* 2. 配置 UART (115200, 8N1, INTERRUPT) */
lisa_uart_config_t config = LISA_UART_CONFIG_DEFAULT();
lisa_uart_configure(uart_dev, &config);

/* 3. 同步发送（中断异步发送+信号量等待），超时 100ms */
char msg[64];
snprintf(msg, sizeof(msg), "Hello UART (sync, INT)! Counter: #%lu\r\n", counter);
int ret = lisa_uart_write_sync(uart_dev, (uint8_t *)msg, strlen(msg), 100);
if (ret > 0) {
    /* 发送成功 */
} else if (ret == LISA_DEVICE_ERR_TIMEOUT) {
    /* 发送超时 */
}
```

## 注意事项

1. **阻塞等待**：`lisa_uart_write_sync()` 会阻塞当前任务，直到发送完成或超时
2. **超时设置**：建议根据数据长度和波特率设置合理的超时时间
3. **中断模式**：使用 `LISA_UART_CONFIG_DEFAULT()` 宏配置为中断模式（默认）
4. **错误处理**：需要检查返回值处理超时和失败情况

