# LISA UART 同步接收示例（DMA 模式 + 循环缓冲区）

## 功能说明

演示在 DMA 模式下使用 UART 同步接收接口，配合循环缓冲区和空闲中断。

底层使用 DMA 方式异步接收，通过循环缓冲区和空闲中断支持定长和不定长数据接收，CPU 占用更低。

## 硬件连接

- **PB2**: UART1 TX（发送）
- **PB3**: UART1 RX（接收）

连接到 PC 串口工具，配置为 **115200, 8N1, 无流控**

## 使用场景

适用于以下场景：
- 需要接收定长或不定长数据包
- 高速率、大数据量的串口通信
- 需要使用循环缓冲区提高接收效率
- 底层采用 DMA 模式传输，CPU 占用更低

## 示例步骤

1. 获取 UART 设备
2. 配置引脚和参数（DMA 模式 + 循环缓冲区）
3. 使能接收（启动循环缓冲区自动接收）
4. 调用 `lisa_uart_read_sync()` 同步接收
5. 满足返回条件后自动返回（收满、空闲、溢出）
6. 如遇缓冲区溢出，调用 `rx_disable` + `rx_enable` 恢复接收

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 预期输出

**终端输出：**
```
=== LISA UART Recv Sync (DMA Mode + Circular Buffer) ===
UART configured:
  - DMA mode with idle interrupt
  - 2 circular buffers x 16 bytes
  - Auto-detect variable length packets
Please send data via serial port (115200, 8N1)

Received 8 bytes:
48 65 6C 6C 6F 21 0D 0A  ("Hello!..")
```

**PC 串口工具发送：**
```
Hello!
```

## 核心 API

| API | 说明 |
|-----|------|
| `lisa_device_get()` | 获取 UART 设备 |
| `lisa_uart_configure()` | 配置 UART 参数 |
| `lisa_uart_rx_enable()` | 使能 UART 接收 |
| `lisa_uart_read_sync()` | 同步接收数据（阻塞等待） |

## 同步接收说明

`lisa_uart_read_sync()` 在 DMA 模式 + 循环缓冲区下的工作原理：
- **底层实现**：DMA + Ping-Pong 缓冲区自动接收，应用层阻塞读取
- **循环缓冲区**：使用 Ping-Pong 双缓冲区机制，一个接收时另一个可供读取
- **空闲中断**：检测到 UART 空闲时触发，用于判断不定长数据包结束
- **返回条件**：
  1. 收满指定长度
  2. 检测到空闲中断（不定长数据包结束）
  3. 缓冲区溢出（驱动停止接收）
- **返回值**：
  - 成功时返回实际接收字节数（> 0）
  - 溢出时返回 `LISA_DEVICE_ERR_OVERFLOW`（需 rx_disable + rx_enable 恢复）
  - 出错时返回其他负数错误码

## 关键代码

```c
uint8_t rx_buffer[16];

/* 1. 获取并检查设备 */
lisa_device_t *uart_dev = lisa_device_get("uart1");

/* 2. 配置 UART (115200, 8N1, DMA 模式, 循环缓冲区) */
lisa_uart_config_t config = LISA_UART_CONFIG_DMA();
config.rx_buf_config.buffer_count = 2;   /* Ping-Pong 缓冲区 */
config.rx_buf_config.buffer_size = 16;   /* 每个缓冲区 16 字节 */
/* DMA 通道配置（可选）：
 * config.dma_tx_channel = 0;  // 指定 TX DMA 通道
 * config.dma_rx_channel = 1;  // 指定 RX DMA 通道
 * 不指定时使用 0xFF 自动分配
 */
lisa_uart_configure(uart_dev, &config);

/* 3. 使能接收（启动循环缓冲区自动接收） */
lisa_uart_rx_enable(uart_dev);

/* 4. 同步接收数据（阻塞等待） */
int ret = lisa_uart_read_sync(uart_dev, rx_buffer, sizeof(rx_buffer));

if (ret > 0) {
    /* 成功接收到数据 */
    printf("Received %d bytes: ", ret);
    for (int i = 0; i < ret; i++) {
        printf("%02X ", (unsigned char)rx_buffer[i]);
    }
    printf("\n");
} else if (ret == LISA_DEVICE_ERR_OVERFLOW) {
    /* 缓冲区溢出，需要恢复接收 */
    lisa_uart_rx_disable(uart_dev);
    lisa_uart_rx_enable(uart_dev);
}
```

## 注意事项

1. **及时读取**：应用层应及时调用 `lisa_uart_read_sync()` 读取数据，避免缓冲区溢出
2. **溢出处理**：返回 `LISA_DEVICE_ERR_OVERFLOW` 时需调用 `rx_disable` + `rx_enable` 恢复接收
3. **空闲中断**：本示例启用空闲中断，可自动检测不定长数据包结束
4. **缓冲区配置**：
   - `buffer_count` 建议设为 2（Ping-Pong 模式）
   - `buffer_size` 根据应用层读取量设置，本示例为 16 字节
5. **DMA 配置**：使用 `LISA_UART_CONFIG_DMA()` 宏自动配置 DMA 模式参数
6. **DMA 通道分配**：
   - 默认使用自动分配（0xFF），适合单设备场景
   - 多设备同时使用 DMA 时，建议显式指定不同通道（系统共 4 个通道：0-3）
   - 示例：UART0 使用通道 0/1，UART1 使用通道 2/3