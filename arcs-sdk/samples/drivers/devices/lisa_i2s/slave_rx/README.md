# I2S 从模式接收示例

## 功能说明

演示如何使用 I2S 从模式接收音频数据。从设备接收主设备产生的时钟信号（BCK 和 LRCK），并通过 DIN 引脚接收数据。

配置为标准 I2S 协议，16kHz 采样率、16位数据宽度、立体声模式。

## 硬件连接

- **PA12**: I2S1_BCK（位时钟输入，由主设备提供）
- **PA13**: I2S1_LRCK（帧时钟/字选择输入，由主设备提供）
- **PA14**: I2S1_DIN（数据输入）

使用I2S1驱动，连接到外部 I2S 主设备。从设备必须连接到主设备才能正常工作。

## 使用场景

适用于以下场景：
- 从主设备接收音频数据
- 作为从设备接收音频流
- 主设备控制时钟的接收场景

## 示例步骤

1. 初始化发送缓冲区（用于数据比对）
2. 获取 I2S1 设备
3. 配置 I2S（从模式，标准 I2S，16位，16kHz，立体声，RX）
4. 设置事件回调函数
5. 启动 I2S 接收
6. 创建接收任务，循环接收数据并与预期数据比对

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 烧录

```{eval-rst}
.. include:: /sample_flash.rst
```

## 预期输出

**终端输出：**

- 外部i2s master没有启动
```
********Arcs SDK 0.1.2 @ v0.0.1-961-gc8139d111aea********
Running on hart-id: 1
I/elog            [1034:42:44.159 1 elog_async] EasyLogger V2.2.99 is initialize success.
I/main            [1034:42:44.160 1 main] ========================================
I/main            [1034:42:44.160 1 main]   LISA I2S SLAVE RX Example
I/main            [1034:42:44.160 1 main] ========================================
I/lisa_i2s_arcs   [1034:42:44.160 1 main] one transfer size: 1024
I/lisa_i2s_arcs   [1034:42:44.161 1 main] I2S1 Control: control=0x42026, argv=0x0
I/lisa_i2s_arcs   [1034:42:44.161 1 main] I2S configured: mode=1, protocol=0, data_width=0, bit_order: 0, sample_rate=16000, slot_mask=3, direction=2, echo=0, echo_slot_mask=0, use_tdm=0, tdm_slots=0, block_size:1024
I/lisa_i2s_arcs   [1034:42:44.161 1 main] I2S start RX: data=0x2800bd80, len=512, slot_mask=3, start_now=0
I/lisa_i2s_arcs   [1034:42:44.161 1 main] I2S RX  started
E/main            [1034:42:44.261 1 main] Failed to receive data: -7
E/main            [1034:42:44.361 1 main] Failed to receive data: -7
E/main            [1034:42:44.461 1 main] Failed to receive data: -7
...
```
- 外部i2s master启动了发送之后
```
I/main            [1034:42:44.661 1 main] Received 1024 bytes data
I/main            [1034:42:45.316 1 main] [0] 0 [1] 1 [2] 2 [3] 3
[4] 4 [5] 5 [6] 6 [7] 7
I/main            [1034:42:45.661 1 main] Received 1024 bytes data
I/main            [1034:42:45.661 1 main] [0] 0 [1] 1 [2] 2 [3] 3
[4] 4 [5] 5 [6] 6 [7] 7
I/main            [1034:42:46.661 1 main] Received 1024 bytes data
I/main            [1034:42:46.661 1 main] [0] 0 [1] 1 [2] 2 [3] 3
[4] 4 [5] 5 [6] 6 [7] 7
...
```

## 核心 API

| API | 说明 |
|-----|------|
| `lisa_device_get()` | 获取 I2S 设备 |
| `lisa_i2s_configure()` | 配置 I2S 参数 |
| `lisa_i2s_set_callback()` | 设置事件回调 |
| `lisa_i2s_read()` | 接收数据（阻塞） |
| `lisa_i2s_trigger()` | 启动/停止 I2S |

## 关键代码

```c
/* 1. 获取 I2S 设备 */
lisa_device_t *i2s_dev = lisa_device_get("i2s1");

/* 2. 配置 I2S: 从模式，标准 I2S，16位，16kHz，立体声，RX */
lisa_i2s_config_t config = LISA_I2S_DEFAULT_CONFIG_RX();
config.mode = LISA_I2S_MODE_SLAVE;  // 从模式
config.block_size = 1024;
int ret = lisa_i2s_configure(i2s_dev, &config);

/* 3. 设置回调函数 */
ret = lisa_i2s_set_callback(i2s_dev, i2s_event_callback, NULL);

/* 4. 启动 I2S 接收 */
ret = lisa_i2s_trigger(i2s_dev, LISA_I2S_DIRECTION_RX, LISA_I2S_CMD_START);

/* 接收任务中循环接收数据 */
while (1) {
    uint8_t *rx_buffer = NULL;
    uint32_t len = 0;
    ret = lisa_i2s_read(i2s_dev, &rx_buffer, &len, 100);
    if (ret == 0) {
        LISA_LOGI(TAG, "Received %d bytes data", len);

        LISA_LOGI(TAG, "[0] %x [1] %x [2] %x [3] %x\n[4] %x [5] %x [6] %x [7] %x\n",
                rx_buffer[0], rx_buffer[1], rx_buffer[2], rx_buffer[3], 
                rx_buffer[4], rx_buffer[5], rx_buffer[6], rx_buffer[7]);
    }
}
```

## 配置说明

### I2S 配置参数

- **模式**: `LISA_I2S_MODE_SLAVE`（从模式，接收时钟）
- **协议**: `LISA_I2S_PROTOCOL_PHILIPS`（标准 I2S）
- **数据位宽**: `LISA_I2S_DATA_WIDTH_16BIT`（16位）
- **采样率**: `LISA_I2S_SAMPLE_RATE_16K`（16kHz）
- **声道**: `LISA_I2S_SLOT_STEREO`（立体声）
- **block_size**: 1024 字节（对应 16ms 音频数据）

### 数据格式

对于 16 位立体声，每个 `uint32_t` 包含左右两个声道：
- 低 16 位：左声道数据
- 高 16 位：右声道数据

## 注意事项

1. **从设备依赖**：从模式下必须连接到主设备，由主设备提供 BCK 和 LRCK 时钟信号
2. **接收缓冲区**：`lisa_i2s_read()` 返回的缓冲区指针指向驱动内部循环缓冲区，应及时拷贝数据
3. **时钟输入**：从模式下，BCK 和 LRCK 为输入，由主设备产生
4. **阻塞接收**：`lisa_i2s_read()` 会阻塞直到接收到数据或超时
