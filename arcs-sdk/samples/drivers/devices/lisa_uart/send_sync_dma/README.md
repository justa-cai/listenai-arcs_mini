# LISA UART 同步发送示例（DMA 模式）

## 功能说明

演示在 DMA 模式下使用 UART 同步发送接口。

底层使用 DMA 硬件传输，通过信号量实现同步等待，CPU 占用更低，适合高速率场景。

## 硬件连接

- **PB2**: UART1 TX（发送）
- **PB3**: UART1 RX（接收）

连接到 PC 串口工具，配置为 **115200, 8N1, 无流控**

## 使用场景

适用于需要阻塞等待发送完成的场景，底层采用 DMA 硬件传输，CPU 占用更低，应用层代码简洁。

## 示例步骤

1. 获取 UART 设备
2. 配置引脚（PB2=TX, PB3=RX）
3. 配置 UART 参数（115200, 8N1, DMA 模式）
4. 循环调用 `lisa_uart_write_sync()` 同步发送
5. 函数阻塞等待 DMA 传输完成后返回

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 预期输出

**终端输出：**
```
=== LISA UART send sync (DMA mode) example ===
UART configured in DMA mode, start sending...

Sent: Hello UART (sync, DMA)! Counter: 0
Sent: Hello UART (sync, DMA)! Counter: 1
Sent: Hello UART (sync, DMA)! Counter: 2
...
```

**PC 串口工具接收：**
```
Hello UART (sync, DMA)! Counter: 0
Hello UART (sync, DMA)! Counter: 1
Hello UART (sync, DMA)! Counter: 2
...
```

## 核心 API

| API | 说明 |
|-----|------|
| `lisa_device_get()` | 获取 UART 设备 |
| `lisa_uart_configure()` | 配置 UART 参数 |
| `lisa_uart_write_sync()` | 同步发送数据（阻塞等待） |

## 同步发送说明

`lisa_uart_write_sync()` 在 DMA 模式下的工作原理：
- **底层实现**：DMA 硬件自动搬移数据到 UART 发送 FIFO，CPU 无需参与
- **阻塞等待**：调用后通过信号量等待，直到 DMA 传输完成
- **返回时机**：发送完成或超时后返回
- **返回值**：
  - 成功时返回实际发送字节数（> 0）
  - 超时时返回 `LISA_DEVICE_ERR_TIMEOUT`
  - 失败时返回其他负数错误码

## 关键代码

```c
/* 1. 获取并检查设备 */
lisa_device_t *uart_dev = lisa_device_get("uart1");

/* 2. 配置 UART (115200, 8N1, DMA 模式) */
lisa_uart_config_t config = LISA_UART_CONFIG_DMA();
/* DMA 通道配置（可选）：
 * config.dma_tx_channel = 0;  // 指定 TX DMA 通道
 * config.dma_rx_channel = 1;  // 指定 RX DMA 通道
 * 不指定时使用 0xFF 自动分配
 */
lisa_uart_configure(uart_dev, &config);

/* 3. 同步发送（DMA异步发送+信号量等待），超时 100ms */
char msg[64];
snprintf(msg, sizeof(msg), "Hello UART (sync, DMA)! Counter: %lu\r\n", counter);
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
3. **DMA 配置**：使用 `LISA_UART_CONFIG_DMA()` 宏自动配置 DMA 模式参数
4. **DMA 通道分配**：
   - 默认使用自动分配（0xFF），适合单设备场景
   - 多设备同时使用 DMA 时，建议显式指定不同通道（系统共 4 个通道：0-3）
   - 示例：UART0 使用通道 0/1，UART1 使用通道 2/3
5. **错误处理**：需要检查返回值处理超时和失败情况
