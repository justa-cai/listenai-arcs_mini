# I2S 从模式发送示例

## 功能说明

演示如何使用 I2S 从模式发送音频数据。从设备接收主设备产生的时钟信号（BCK 和 LRCK），并通过 DOUT 引脚发送数据。

配置为标准 I2S 协议，16kHz 采样率、16位数据宽度、立体声模式。

## 硬件连接

- **PA12**: I2S0_BCK（位时钟输入，由主设备提供）
- **PA13**: I2S0_LRCK（帧时钟/字选择输入，由主设备提供）
- **PA15**: I2S0_DOUT（数据输出）

连接到外部 I2S 主设备。从设备必须连接到主设备才能正常工作。

## 使用场景

适用于以下场景：
- 向主设备发送音频数据
- 作为从设备发送音频流
- 主设备控制时钟的发送场景

## 示例步骤

1. 初始化发送缓冲区
2. 获取 I2S0 设备
3. 配置 I2S（从模式，标准 I2S，16位，16kHz，立体声，TX）
4. 设置事件回调函数
5. 预先加载要发送的数据（CONFIG_LISA_I2S_BLOCK_COUNT - 1 次）
6. 启动 I2S 发送
7. 主循环（持续运行）

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
********Arcs SDK 0.1.2 @ v0.0.1-960-g2f8d7197f7f6********
Running on hart-id: 1
I/elog            [1034:42:44.159 1 elog_async] EasyLogger V2.2.99 is initialize success.
I/main            [1034:42:44.160 1 main] ========================================
I/main            [1034:42:44.160 1 main]   LISA I2S SLAVE TX Example
I/main            [1034:42:44.160 1 main] ========================================
I/lisa_i2s_arcs   [1034:42:44.161 1 main] one transfer size: 1024
I/lisa_i2s_arcs   [1034:42:44.161 1 main] I2S0 Control: control=0x58026, argv=0x0
I/lisa_i2s_arcs   [1034:42:44.161 1 main] I2S configured: mode=1, protocol=0, data_width=0, bit_order: 0, sample_rate=16000, slot_mask=3, direction=1, echo=0, echo_slot_mask=0, use_tdm=0, tdm_slots=0, block_size:1024
I/lisa_i2s_arcs   [1034:42:44.161 1 main] I2S start TX: data=0x2800bd80, len=512, slot_mask=3, start_now=0
I/lisa_i2s_arcs   [1034:42:44.161 1 main] I2S  TX started
E/main            [1034:42:44.261 1 main] Failed to send data: -7
E/main            [1034:42:44.361 1 main] Failed to send data: -7
E/main            [1034:42:44.461 1 main] Failed to send data: -7
E/main            [1034:42:44.561 1 main] Failed to send data: -7
...
```

- 外部i2s master启动了发送之后

```
I/main            [1034:42:44.623 1 isr] TX completed
I/main            [1034:42:44.638 1 isr] TX completed
I/main            [1034:42:44.654 1 isr] TX completed
I/main            [1034:42:44.670 1 isr] TX completed
I/main            [1034:42:44.686 1 isr] TX completed
```

## 核心 API

| API | 说明 |
|-----|------|
| `lisa_device_get()` | 获取 I2S 设备 |
| `lisa_i2s_configure()` | 配置 I2S 参数 |
| `lisa_i2s_set_callback()` | 设置事件回调 |
| `lisa_i2s_write()` | 发送数据（非阻塞） |
| `lisa_i2s_trigger()` | 启动/停止 I2S |

## 关键代码

```c
/* 1. 获取 I2S 设备 */
lisa_device_t *i2s_dev = lisa_device_get("i2s0");

/* 2. 配置 I2S: 从模式，标准 I2S，16位，16kHz，立体声，TX */
lisa_i2s_config_t config = LISA_I2S_DEFAULT_CONFIG_TX();
config.mode = LISA_I2S_MODE_SLAVE;  // 从模式
config.block_size = 1024;
int ret = lisa_i2s_configure(i2s_dev, &config);

/* 3. 设置回调函数 */
ret = lisa_i2s_set_callback(i2s_dev, i2s_event_callback, NULL);

/* 4. 预先加载要发送的数据 */
for(int i = 0; i < CONFIG_LISA_I2S_BLOCK_COUNT - 1; i++) {
    ret = lisa_i2s_write(i2s_dev, (uint32_t *)tx_buffer, 
                         BLOCK_SIZE / sizeof(uint32_t), 0);
}

/* 5. 启动 I2S 发送 */
ret = lisa_i2s_trigger(i2s_dev, LISA_I2S_DIRECTION_TX, LISA_I2S_CMD_START);

/* 6. 循环发送数据 */
while (1) {
    ret = lisa_i2s_write(i2s_dev, (uint32_t *)tx_buffer, BLOCK_SIZE / sizeof(uint32_t), 100);
    if (ret < 0) {
        LISA_LOGE(TAG, "Failed to send data: %d", ret);
    }
}

/* 7. 停止 I2S 发送 */
ret = lisa_i2s_trigger(i2s_dev, LISA_I2S_DIRECTION_TX, LISA_I2S_CMD_STOP);
if (ret != 0) {
    LISA_LOGE(TAG, "Failed to stop I2S: %d", ret);
    return -1;
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
2. **预加载数据**：启动前需预先加载 `CONFIG_LISA_I2S_BLOCK_COUNT - 1` 次数据，避免下溢
3. **时钟输入**：从模式下，BCK 和 LRCK 为输入，由主设备产生
