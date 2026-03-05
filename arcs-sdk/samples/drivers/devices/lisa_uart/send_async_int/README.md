# LISA UART 异步发送示例（中断模式）

## 功能说明

演示在中断模式下使用 UART 异步发送接口，通过事件回调接收发送完成通知。

底层使用中断方式异步发送，调用后立即返回，通过回调函数通知发送完成。

## 硬件连接

- **PB2**: UART1 TX（发送）
- **PB3**: UART1 RX（接收）

连接到 PC 串口工具，配置为 **115200, 8N1, 无流控**

## 使用场景

适用于需要异步非阻塞发送数据的场景，底层采用中断模式传输，应用层通过回调确认发送完成。

## 示例步骤

1. 创建信号量（用于同步中断回调）
2. 获取 UART 设备
3. 配置引脚和参数（中断模式）
4. 设置事件回调函数（接收 TX_DONE 事件）
5. 调用 `lisa_uart_write_async()` 异步发送
6. 等待回调释放信号量确认发送完成

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 预期输出

**终端输出：**
```
=== LISA UART async send (interrupt mode) example ===
UART ready, start sending...

Sent: Hello UART! Counter: 0
Sent: Hello UART! Counter: 1
Sent: Hello UART! Counter: 2
...
```

**PC 串口工具接收：**
```
Hello UART! Counter: 0
Hello UART! Counter: 1
Hello UART! Counter: 2
...
```

## 核心 API

| API | 说明 |
|-----|------|
| `lisa_device_get()` | 获取 UART 设备 |
| `lisa_uart_configure()` | 配置 UART 参数 |
| `lisa_uart_set_callback()` | 设置事件回调函数 |
| `lisa_uart_write_async()` | 异步发送数据（非阻塞） |

## 异步发送说明

`lisa_uart_write_async()` 在中断模式下的工作原理：
- **底层实现**：通过中断方式异步发送数据，CPU 无需轮询
- **返回时机**：调用后立即返回，不等待发送完成
- **回调触发**：中断发送完成后触发 `LISA_UART_EVENT_TX_DONE` 事件
- **返回值**：
  - 成功时返回实际发送字节数（> 0）
  - 失败时返回负数错误码

## 关键代码

```c
char msg[64];

/* 1. 获取并检查设备 */
lisa_device_t *uart_dev = lisa_device_get("uart1");

/* 2. 配置 UART (115200, 8N1, 中断模式) */
lisa_uart_config_t config = LISA_UART_CONFIG_DEFAULT();
lisa_uart_configure(uart_dev, &config);

/* 3. 创建信号量（用于等待发送完成） */
tx_sem = xSemaphoreCreateBinary();

/* 4. 设置事件回调 */
lisa_uart_set_callback(uart_dev, uart_event_callback, NULL);

/* 中断回调函数 */
static void uart_event_callback(lisa_uart_event_t event, void *user_data)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (event & LISA_UART_EVENT_TX_DONE) {
        /* 从中断中释放信号量 */
        xSemaphoreGiveFromISR(tx_sem, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

/* 5. 异步发送数据 */
int ret = lisa_uart_write_async(uart_dev, (uint8_t *)msg, strlen(msg));
if (ret > 0) {
    /* 等待回调释放的信号量，超时时间 100ms */
    if (xSemaphoreTake(tx_sem, pdMS_TO_TICKS(100)) == pdTRUE) {
        /* 发送完成 */
    } else {
        /* 发送超时 */
    }
}
```

## 注意事项

1. **缓冲区生命周期**：发送完成前不能释放或修改发送缓冲区内容
2. **回调上下文**：回调函数在中断上下文中执行，应尽快返回，不要执行耗时操作
3. **超时处理**：建议设置合理的信号量超时时间，避免永久等待
4. **并发控制**：如需连续发送，应等待上一次发送完成后再发起下一次发送
5. **中断模式**：使用 `LISA_UART_CONFIG_DEFAULT()` 宏配置为中断模式（默认）
