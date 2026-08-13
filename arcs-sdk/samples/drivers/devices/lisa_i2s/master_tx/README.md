# I2S 主模式发送示例

## 功能说明

演示如何使用 I2S 主模式发送音频数据。主设备产生时钟信号（BCK 和 LRCK），并通过 DOUT 引脚发送数据。

配置为标准 I2S 协议，16kHz 采样率、16位数据宽度、立体声模式。

## 硬件连接

- **PA12**: I2S0_BCK（位时钟输出）
- **PA13**: I2S0_LRCK（帧时钟/字选择输出）
- **PA15**: I2S0_DOUT（数据输出）

连接到外部 I2S 从设备（如 DAC、音频编解码器等）。

## 使用场景

适用于以下场景：
- 音频数据播放
- 主设备向从设备发送音频流
- 需要主设备控制时钟的场景

## 示例步骤

1. 获取 I2S0 设备
2. 配置 I2S（主模式，标准 I2S，16位，16kHz，立体声，TX）
3. 设置事件回调函数
4. 预先加载要发送的数据（CONFIG_LISA_I2S_BLOCK_COUNT - 1 次）
5. 启动 I2S 发送
6. 继续发送数据（循环 10 次）
7. 等待所有 TX 缓存发送完
8. 停止 I2S 发送

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
```
********Arcs SDK 0.1.2 @ v0.0.1-961-gc8139d111aea********
Running on hart-id: 1
I/elog            [1034:42:44.159 1 elog_async] EasyLogger V2.2.99 is initialize success.
I/main            [1034:42:44.160 1 main] ========================================
I/main            [1034:42:44.160 1 main]   LISA I2S Master TX Example
I/main            [1034:42:44.160 1 main] ========================================
I/lisa_i2s_arcs   [1034:42:44.161 1 main] one transfer size: 1024
I/lisa_i2s_arcs   [1034:42:44.161 1 main] I2S0 Control: control=0x58025, argv=0x3e80
I/lisa_i2s_arcs   [1034:42:44.161 1 main] I2S configured: mode=0, protocol=0, data_width=0, bit_order: 0, sample_rate=16000, slot_mask=3, direction=1, echo=0, echo_slot_mask=0, use_tdm=0, tdm_slots=0, block_size:1024
I/lisa_i2s_arcs   [1034:42:44.361 1 main] I2S start TX: data=0x2800bd80, len=512, slot_mask=3, start_now=0
I/lisa_i2s_arcs   [1034:42:44.361 1 main] I2S  TX started
I/main            [1034:42:44.377 1 isr] TX completed
I/main            [1034:42:44.393 1 isr] TX completed
I/main            [1034:42:44.409 1 isr] TX completed
I/main            [1034:42:44.425 1 isr] TX completed
I/main            [1034:42:44.440 1 isr] TX completed
I/main            [1034:42:44.456 1 isr] TX completed
I/main            [1034:42:44.472 1 isr] TX completed
I/main            [1034:42:44.489 1 isr] TX completed
I/main            [1034:42:44.504 1 isr] TX completed
I/lisa_i2s_arcs   [1034:42:44.505 1 main] I2S  TX stopped
I/main            [1034:42:44.505 1 main] 
I2S Master TX completed
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

/* 2. 配置 I2S: 主模式，标准 I2S，16位，16kHz，立体声，TX */
lisa_i2s_config_t config = LISA_I2S_DEFAULT_CONFIG_TX();
config.block_size = 1024;  // 16ms 音频数据
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

/* 6. 继续发送数据 */
for(int i = 0; i < 10; i++) {
    ret = lisa_i2s_write(i2s_dev, (uint32_t *)tx_buffer, 
                         BLOCK_SIZE / sizeof(uint32_t), 100);
}

/* 7. 停止 I2S 发送 */
ret = lisa_i2s_trigger(i2s_dev, LISA_I2S_DIRECTION_TX, LISA_I2S_CMD_STOP);
```

## 配置说明

### I2S 配置参数

- **模式**: `LISA_I2S_MODE_MASTER`（主模式，产生时钟）
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

1. **预加载数据**：启动前需预先加载 `CONFIG_LISA_I2S_BLOCK_COUNT - 1` 次数据，避免下溢
2. **主设备职责**：主模式下，BCK 和 LRCK 为输出，由主设备产生时钟
3. **block_size 限制**：必须是 32 的倍数
