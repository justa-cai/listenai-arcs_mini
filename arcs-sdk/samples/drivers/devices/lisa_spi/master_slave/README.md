# LISA SPI MASTER_SLAVE 发送示例

## 功能说明

演示如何使用 `lisa_spi_transfer()` API 传输数据。

## 硬件连接

- **PA15**: SPI0 CLK（时钟）
- **PA14**: SPI0 MOSI（主出从入）
- **PA13**：SPI0 MISO (主入从出)
- **PA12**: SPI0 CS （片选

- **PA25**: SPI1 CLK（时钟）
- **PA24**: SPI1 MOSI（主出从入）
- **PA23**：SPI1 MISO (主入从出)
- **PA22**: SPI1 CS （片选

连接到 PC 串口工具，配置为 **921600, 8N1, 无流控**

## API 说明

`lisa_spi_transfer` 是一个**非阻塞**的传输接口：
- 
- 立即返回（不阻塞）
- 可以注册回调函数来获取传输完成的状态

## 示例步骤

```text
1. 获取 SPI 设备
2. 创建传输完成的信号量
3. 配置引脚
4. 配置 SPI
5. 传输数据
   - 调用 lisa_spi_transfer()
   - 如果成功，会收到传输完成的信号量
```

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 预期输出

**终端输出：**
```
=== LISA SPI master-slave thread demo ===
SPI1 transfer completed
SPI0 transfer completed
SPI1 Received: 1 2 3 4 5 6 7 8
SPI0 Received: 8 7 6 5 4 3 2 1
...
```

## 使用说明

1. 编译并运行程序
2. 通过 PC 串口可以看到发送完成的打印信息

## 核心 API

| API | 说明 |
|-----|------|
| `lisa_spi_transfer()` | spi传输数据（非阻塞） |

## 返回值说明

- `0`: 成功接收到数据
- 其他负数: 错误码

## 使用场景

- SPI master slave 发送并接收数据
