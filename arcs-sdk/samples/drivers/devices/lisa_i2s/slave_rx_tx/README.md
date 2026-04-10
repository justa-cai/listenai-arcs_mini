# LISA I2S 从模式双向传输示例

## 功能说明

演示如何使用 I2S 从模式同时进行发送和接收（全双工）。从设备接收主设备产生的时钟信号（BCK 和 LRCK），并通过 DOUT 引脚发送数据，同时通过 DIN 引脚接收数据。

配置为标准 I2S 协议，16kHz 采样率、16位数据宽度、立体声模式。

## 硬件连接

- **PA12**: I2S0_BCK（位时钟输入，由主设备提供）
- **PA13**: I2S0_LRCK（帧时钟/字选择输入，由主设备提供）
- **PA14**: I2S0_DIN（数据输入）
- **PA15**: I2S0_DOUT（数据输出）

连接到外部 I2S 主设备。从设备必须连接到主设备才能正常工作。

## 使用场景

适用于以下场景：
- 从设备全双工音频通信
- 音频回环测试（从设备端）
- 同时向主设备发送和接收音频数据
- 主设备控制时钟的双向传输场景

## 示例步骤

1. 初始化发送缓冲区
2. 获取 I2S0 设备
3. 配置 I2S（从模式，标准 I2S，16位，16kHz，立体声，双向）
4. 设置事件回调函数
5. 预先加载要发送的数据（CONFIG_LISA_I2S_BLOCK_COUNT - 1 次）
6. 启动 I2S 双向传输
7. 创建接收和发送任务
8. 主循环（持续运行）

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

- 外部i2s master启动收发之前，从设备输出错误信息
- 外部i2s master启动收发之后，从设备输出正常信息

```
********Arcs SDK 0.1.2 @ v0.0.1-917-g14ea2d8ccfe3********
Running on hart-id: 1
I/elog            [1034:42:44.159 1 elog_async] EasyLogger V2.2.99 is initialize success.
I/main            [1034:42:44.160 1 main] ========================================
I/main            [1034:42:44.160 1 main]   LISA I2S SLAVE TX RX Example
I/main            [1034:42:44.160 1 main] ========================================
I/lisa_i2s_arcs   [1034:42:44.161 1 main] one transfer size: 1024
I/lisa_i2s_arcs   [1034:42:44.161 1 main] I2S1 Control: control=0x5a026, argv=0x0
I/lisa_i2s_arcs   [1034:42:44.161 1 main] I2S configured: mode=1, protocol=0, data_width=0, bit_order: 0, sample_rate=16000, slot_mask=3, direction=3, echo=0, echo_slot_mask=0, use_tdm=0, tdm_slots=0, block_size:1024
I/lisa_i2s_arcs   [1034:42:44.161 1 main] I2S start RX: data=0x2800ce00, len=512, slot_mask=3, start_now=0
I/lisa_i2s_arcs   [1034:42:44.162 1 main] I2S start TX: data=0x2800bd80, len=512, slot_mask=3, start_now=0
I/lisa_i2s_arcs   [1034:42:44.162 1 main] I2S RX TX started
E/main            [1034:42:44.562 1 i2s tx] Failed to send data: -7
E/main            [1034:42:44.661 1 i2s rx] Failed to receive data: -7
E/main            [1034:42:44.662 1 i2s tx] Failed to send data: -7
E/main            [1034:42:44.761 1 i2s rx] Failed to receive data: -7
E/main            [1034:42:44.762 1 i2s tx] Failed to send data: -7
I/main            [1034:42:45.009 1 isr] TX completed
I/main            [1034:42:45.010 1 isr] RX completed
I/main            [1034:42:45.010 1 i2s rx] Received 1024 bytes data
I/main            [1034:42:45.010 1 i2s rx] [0] 0 [1] 1 [2] 2 [3] 3
[4] 4 [5] 5 [6] 6 [7] 7
I/main            [1034:42:45.025 1 isr] TX completed
I/main            [1034:42:45.026 1 isr] RX completed
I/main            [1034:42:45.026 1 i2s rx] Received 1024 bytes data
I/main            [1034:42:45.026 1 i2s rx] [0] 0 [1] 1 [2] 2 [3] 3
[4] 4 [5] 5 [6] 6 [7] 7
...
```

## 核心 API

| API | 说明 |
|-----|------|
| `lisa_device_get()` | 获取 I2S 设备 |
| `lisa_i2s_configure()` | 配置 I2S 参数 |
| `lisa_i2s_set_callback()` | 设置事件回调 |
| `lisa_i2s_write()` | 发送数据（非阻塞） |
| `lisa_i2s_read()` | 接收数据（阻塞） |
| `lisa_i2s_trigger()` | 启动/停止 I2S |

## 关键代码

```c
/* 1. 获取 I2S 设备 */
lisa_device_t *i2s_dev = lisa_device_get("i2s0");

/* 2. 配置 I2S: 从模式，标准 I2S，16位，16kHz，立体声，双向 */
lisa_i2s_config_t config = LISA_I2S_DEFAULT_CONFIG_TX();
config.mode = LISA_I2S_MODE_SLAVE;  // 从模式
config.direction = LISA_I2S_DIRECTION_BOTH;  // 双向传输
config.block_size = 1024;
int ret = lisa_i2s_configure(i2s_dev, &config);

/* 3. 设置回调函数 */
ret = lisa_i2s_set_callback(i2s_dev, i2s_event_callback, NULL);

/* 4. 预先加载要发送的数据 */
for(int i = 0; i < CONFIG_LISA_I2S_BLOCK_COUNT - 1; i++) {
    ret = lisa_i2s_write(i2s_dev, (uint32_t *)tx_buffer, 
                         BLOCK_SIZE / sizeof(uint32_t), 0);
}

/* 5. 启动 I2S 双向传输 */
ret = lisa_i2s_trigger(i2s_dev, LISA_I2S_DIRECTION_BOTH, LISA_I2S_CMD_START);

/* 6. 创建接收和发送任务 */
xTaskCreate(i2s_rx_task, "i2s rx", 4096, i2s_dev, 9, NULL);
xTaskCreate(i2s_tx_task, "i2s tx", 4096, i2s_dev, 8, NULL);

/* RX 任务中循环接收数据 */
while (1) {
    uint8_t *rx_buffer = NULL;
    uint32_t len = 0;
    ret = lisa_i2s_read(i2s_dev, &rx_buffer, &len, 100);
    if (ret == 0) {
        LISA_LOGI(TAG, "Received %d bytes data", len);
    }
}

/* TX 任务中循环发送数据 */
while (1) {
    ret = lisa_i2s_write(i2s_dev, (uint32_t *)tx_buffer, 
                         BLOCK_SIZE / sizeof(uint32_t), 100);
}
```

## 配置说明

### I2S 配置参数

- **模式**: `LISA_I2S_MODE_SLAVE`（从模式，接收时钟）
- **协议**: `LISA_I2S_PROTOCOL_PHILIPS`（标准 I2S）
- **数据位宽**: `LISA_I2S_DATA_WIDTH_16BIT`（16位）
- **采样率**: `LISA_I2S_SAMPLE_RATE_16K`（16kHz）
- **声道**: `LISA_I2S_SLOT_STEREO`（立体声）
- **方向**: `LISA_I2S_DIRECTION_BOTH`（双向传输）
- **block_size**: 1024 字节（对应 16ms 音频数据）

### 数据格式

对于 16 位立体声，每个 `uint32_t` 包含左右两个声道：
- 低 16 位：左声道数据
- 高 16 位：右声道数据

## 注意事项

1. **从设备依赖**：从模式下必须连接到主设备，由主设备提供 BCK 和 LRCK 时钟信号
2. **双向传输**：需要同时配置 TX 和 RX，使用 `LISA_I2S_DIRECTION_BOTH`
3. **任务分离**：建议创建独立的 RX 和 TX 任务分别处理接收和发送 
4. **预加载数据**：启动前需预先加载 `CONFIG_LISA_I2S_BLOCK_COUNT - 1` 次数据
5. **时钟输入**：从模式下，BCK 和 LRCK 为输入，由主设备产生
6. **线程安全**：驱动支持全双工通信，可在不同线程中同时进行收发操作
