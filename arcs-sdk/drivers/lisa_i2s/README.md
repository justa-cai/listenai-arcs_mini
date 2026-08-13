# I2S 驱动

基于 lisa_device 框架的 I2S 设备驱动，为 ARCS 平台提供统一的音频数据传输接口。

## 功能特性

- **设备支持**: I2S0、I2S1 两个 I2S 音频接口
- **工作模式**: 支持主模式和从模式
- **协议支持**: 标准 I2S（Philips）、左对齐、右对齐、PCM 短帧（MODE 0/1）
- **数据位宽**: 16/20/24/32 位数据宽度
- **采样率**: 8kHz、16kHz、24kHz、32kHz、48kHz、96kHz
- **声道配置**: 单声道（MONO）、立体声（STEREO）
- **传输方向**: 支持单向（TX/RX）或双向（TX+RX）同时传输
- **DMA 传输**: 基于 DMA 的高效数据传输，支持循环缓冲区
- **事件回调**: 支持 TX 完成、RX 完成、下溢、溢出等事件通知

## 配置选项

在 `prj.conf` 中启用驱动：

```kconfig
CONFIG_LISA_I2S_DEVICE=y
CONFIG_LISA_I2S0=y                   # 启用 I2S0 设备
CONFIG_LISA_I2S1=y                   # 启用 I2S1 设备

# 可选配置
CONFIG_LISA_I2S_BLOCK_COUNT=4        # DMA 循环缓冲区数量（默认 4）
CONFIG_LISA_I2S_ECHO=n               # 启用 I2S 回声功能（默认禁用）
CONFIG_LISA_I2S_PIPO=y               # 启用 I2S PIPO 模式（默认启用）

# DMA 通道配置
CONFIG_LISA_I2S0_RX_DMA_CHANNEL=0    # I2S0 RX DMA 通道（默认 0）
CONFIG_LISA_I2S0_TX_DMA_CHANNEL=1    # I2S0 TX DMA 通道（默认 1）
CONFIG_LISA_I2S1_RX_DMA_CHANNEL=2    # I2S1 RX DMA 通道（默认 2）
CONFIG_LISA_I2S1_TX_DMA_CHANNEL=3    # I2S1 TX DMA 通道（默认 3）
```

根据需要选择启用 I2S0 或 I2S1 设备。

## API 接口

### 配置接口

```c
/* 配置 I2S 参数 */
int lisa_i2s_configure(lisa_device_t *dev, const lisa_i2s_config_t *config);

/* 获取当前配置 */
int lisa_i2s_get_config(lisa_device_t *dev, lisa_i2s_config_t *config);
```

**重要**: 
- 必须先调用 `lisa_i2s_configure()` 配置设备后，才能使用下面的功能 API
- 配置中的 `block_size` 参数决定每次传输块的大小（字节），必须是 32 的倍数

### 数据传输接口

```c
/* 发送数据（非阻塞，将数据写入临时缓存等待发送）
 * data: 发送数据缓冲区（uint32_t 类型）
 * cnt: 发送数据点数（每个点为 uint32_t，即 4 字节）
 * timeout_ms: 超时时间（毫秒）
 * 返回: LISA_DEVICE_OK 成功，其他为错误码
 */
int lisa_i2s_write(lisa_device_t *dev, uint32_t *data, uint32_t cnt, uint32_t timeout_ms);

/* 接收数据（阻塞，直到接收到数据）
 * data: 输出参数，指向接收缓冲区的指针（驱动内部缓冲区，不需要释放）
 * len: 输出参数，实际接收的字节数
 * timeout_ms: 超时时间（毫秒，0 表示不等待）
 * 返回: LISA_DEVICE_OK 成功，其他为错误码
 */
int lisa_i2s_read(lisa_device_t *dev, uint8_t **data, uint32_t *len, uint32_t timeout_ms);
```

**注意**:
- `lisa_i2s_write()` 的 `cnt` 参数是 uint32_t 的数量，发送的总字节数 = cnt × 4
- 对于 16 位数据宽度，每个 uint32_t 包含 2 个声道的数据
- 对于其他数据宽度，每个 uint32_t 包含 1 个声道的数据
- 建议发送的总字节数等于配置的 `block_size`

### 控制接口

```c
/* 启动/停止/暂停/恢复 I2S 传输
 * direction: 传输方向（LISA_I2S_DIRECTION_TX/RX/BOTH）
 * cmd: 命令类型（LISA_I2S_CMD_START/STOP/PAUSE/RESUME）
 */
int lisa_i2s_trigger(lisa_device_t *dev, lisa_i2s_direction_t direction, lisa_i2s_cmd_t cmd);
```

### 回调接口

```c
/* 注册事件回调函数（在中断上下文中执行）
 * callback: 回调函数指针
 * user_data: 用户数据指针
 */
int lisa_i2s_set_callback(lisa_device_t *dev, lisa_i2s_event_callback_t callback, void *user_data);
```

## 硬件配置

### 引脚复用配置

I2S 驱动在初始化时会自动调用板型目录中定义的 `lisa_i2sX_pinmux()` 函数（X 为 0/1），用于配置 I2S 设备的引脚复用。

**配置位置**:
- **定义**: `boards/<板型名>/pinmux.c` 中实现 `lisa_i2s0_pinmux()`、`lisa_i2s1_pinmux()` 函数
- **声明**: `boards/<板型名>/pinmux.h` 中声明 `void lisa_i2sX_pinmux()`
- **调用时机**: 对应 I2S 设备初始化时自动调用

**示例** (参考 `boards/arcs_evb/pinmux.c`):
```c
// I2S0 引脚复用配置
void lisa_i2s0_pinmux()
{
    // 配置 PA12 为 I2S0_BCK（位时钟，功能码 9）
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 12, 9);
    // 配置 PA13 为 I2S0_LRCK（帧时钟/字选择，功能码 9）
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 13, 9);
    // 配置 PA14 为 I2S0_DIN（数据输入，功能码 9）
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 14, 9);
    // 配置 PA15 为 I2S0_DOUT（数据输出，功能码 9）
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 15, 9);
}

// I2S1 引脚复用配置
void lisa_i2s1_pinmux()
{
    // 根据实际硬件连接配置 I2S1 引脚
}
```

**注意**:
- 该函数由板型相关代码实现，不同板型的引脚配置可能不同
- 只需配置实际使用的引脚（如仅 TX，则只需配置 BCK、LRCK、DOUT）
- 主模式下，BCK 和 LRCK 为输出；从模式下为输入

## 使用示例

### 主模式发送（Master TX）

```c
#include "lisa_i2s.h"

// 获取 I2S0 设备
lisa_device_t *i2s0 = lisa_device_get("i2s0");
if (!i2s0) {
    return -1;
}

// 配置为主模式发送
lisa_i2s_config_t config = LISA_I2S_DEFAULT_CONFIG_TX();
config.mode = LISA_I2S_MODE_MASTER;
// 设置每次传输块的大小（必须是 32 的倍数）
// 对于 16kHz 立体声 16 位：16ms 音频 = 16000 * 0.016 * 2 * 2 = 1024 字节
config.block_size = 1024;
int ret = lisa_i2s_configure(i2s0, &config);
if (ret != 0) {
    return -1;
}

// 准备发送数据
uint8_t tx_buffer[1024];
for (int i = 0; i < 1024; i++) {
    data[i] = i;  // 填充测试数据
}

// 预先加载数据到 DMA 队列（建议 CONFIG_LISA_I2S_BLOCK_COUNT - 1 次）
for (int i = 0; i < CONFIG_LISA_I2S_BLOCK_COUNT - 1; i++) {
    lisa_i2s_write(i2s0, (uint32_t *)tx_buffer, config.block_size / 4, 0);
}

// 启动 I2S 发送
lisa_i2s_trigger(i2s0, LISA_I2S_DIRECTION_TX, LISA_I2S_CMD_START);

// 持续发送数据
while (1) {
    ret = lisa_i2s_write(i2s0, (uint32_t *)tx_buffer, config.block_size / 4, 100);
    if (ret < 0) {
        // 发送失败处理
    }
}
```

### 从模式接收（Slave RX）

```c
#include "lisa_i2s.h"

// 事件回调函数
void i2s_event_callback(lisa_i2s_event_t event, void *user_data)
{
    if (event & LISA_I2S_EVENT_RX_DONE) {
        // RX 完成
    }
    if (event & LISA_I2S_EVENT_RX_OVERRUN) {
        // RX 溢出错误
    }
}

// 获取 I2S0 设备
lisa_device_t *i2s0 = lisa_device_get("i2s0");

// 配置为从模式接收
lisa_i2s_config_t config = LISA_I2S_DEFAULT_CONFIG_RX();
config.mode = LISA_I2S_MODE_SLAVE;
config.block_size = 1024;  // 设置每次传输块的大小（必须是 32 的倍数）
lisa_i2s_configure(i2s0, &config);

// 设置回调函数
lisa_i2s_set_callback(i2s0, i2s_event_callback, NULL);

// 启动 I2S 接收
lisa_i2s_trigger(i2s0, LISA_I2S_DIRECTION_RX, LISA_I2S_CMD_START);

// 接收数据
while (1) {
    uint8_t *rx_buffer = NULL;
    uint32_t len = 0;
    int ret = lisa_i2s_read(i2s0, &rx_buffer, &len, 100);
    if (ret == 0) {
        // 处理接收到的数据
        // rx_buffer 指向驱动内部缓冲区，不需要释放
    }
}
```

### 双向传输（Master RX+TX）

```c
// 配置为主模式双向传输
lisa_i2s_config_t config = LISA_I2S_DEFAULT_CONFIG_TX();
config.mode = LISA_I2S_MODE_MASTER;
config.direction = LISA_I2S_DIRECTION_BOTH;
config.block_size = 1024;  // 设置每次传输块的大小
lisa_i2s_configure(i2s0, &config);

// 预加载 TX 数据
for (int i = 0; i < CONFIG_LISA_I2S_BLOCK_COUNT - 1; i++) {
    lisa_i2s_write(i2s0, (uint32_t *)tx_buffer, config.block_size / 4, 0);
}

// 启动双向传输
lisa_i2s_trigger(i2s0, LISA_I2S_DIRECTION_BOTH, LISA_I2S_CMD_START);

// 创建 RX 和 TX 任务分别处理接收和发送
xTaskCreate(i2s_rx_task, "i2s rx", 4096, i2s0, 9, NULL);
xTaskCreate(i2s_tx_task, "i2s tx", 4096, i2s0, 8, NULL);
```

### 自定义配置

```c
// 手动配置各项参数
lisa_i2s_config_t config = {
    .mode = LISA_I2S_MODE_MASTER,                   // 主模式
    .protocol = LISA_I2S_PROTOCOL_PHILIPS,          // 标准 I2S 协议
    .data_width = LISA_I2S_DATA_WIDTH_16BIT,        // 16 位数据宽度
    .bit_order = LISA_I2S_BIT_ORDER_MSB,            // MSB 先行
    .sample_rate = LISA_I2S_SAMPLE_RATE_48K,        // 48kHz 采样率
    .slot_mask = LISA_I2S_SLOT_STEREO,              // 立体声
    .direction = LISA_I2S_DIRECTION_TX,             // 仅发送
    .echo = {
        .enable = false,                             // 不启用回声
    },
    .use_tdm = false,                                // 不使用 TDM
    .block_size = 3072,                             // 16ms音频数据
};

lisa_i2s_configure(i2s0, &config);
```

## 配置宏

驱动提供便捷配置宏用于快速初始化：

- `LISA_I2S_DEFAULT_CONFIG_TX()` - 主模式，标准 I2S，16位，16kHz，立体声，TX 方向，block_size=1024
- `LISA_I2S_DEFAULT_CONFIG_RX()` - 主模式，标准 I2S，16位，16kHz，立体声，RX 方向，block_size=1024

**默认配置参数**:
- 模式: `LISA_I2S_MODE_MASTER`（主模式）
- 协议: `LISA_I2S_PROTOCOL_PHILIPS`（标准 I2S）
- 数据位宽: `LISA_I2S_DATA_WIDTH_16BIT`（16位）
- 位序: `LISA_I2S_BIT_ORDER_MSB`（MSB 先行）
- 采样率: `LISA_I2S_SAMPLE_RATE_16K`（16kHz）
- 声道: `LISA_I2S_SLOT_STEREO`（立体声）
- 传输块大小: `block_size = 1024`（对应 16ms 音频数据）
- 回声: 禁用
- TDM: 禁用

**注意**: 使用配置宏后，需要根据实际需求修改 `mode`（主/从模式）和 `block_size`（传输块大小）

## 事件类型

- `LISA_I2S_EVENT_TX_DONE` - 发送完成（一个 DMA 块发送完成）
- `LISA_I2S_EVENT_RX_DONE` - 接收完成（一个 DMA 块接收完成）
- `LISA_I2S_EVENT_TX_UNDERRUN` - 发送下溢（TX 队列为空，无数据可发送）
- `LISA_I2S_EVENT_RX_OVERRUN` - 接收溢出（RX 队列满，接收数据丢失）
- `LISA_I2S_EVENT_ERROR` - 其他错误

## 数据格式说明

### 16 位立体声数据格式

对于 16 位立体声配置，数据以 `uint32_t` 数组形式传输：
- 每个 `uint32_t` 包含左右两个声道的数据
- 低 16 位：左声道数据
- 高 16 位：右声道数据

```c
uint32_t sample = (right_channel << 16) | (left_channel & 0xFFFF);
```

### 传输大小计算

**block_size 计算公式**:
```
block_size = 采样率 × 时间(秒) × 声道数 × 每样本字节数
```

**示例**:
- 16kHz 立体声 16位，传输 16ms 音频：
  - block_size = 16000 × 0.016 × 2 × 2 = 1024 字节
- 48kHz 立体声 16位，传输 10ms 音频：
  - block_size = 48000 × 0.010 × 2 × 2 = 1920 字节

**lisa_i2s_write() 参数计算**:
- 对于 16 位数据宽度：`cnt = block_size / 4`（每个 uint32_t 包含 2 个声道）
- 对于其他数据宽度：`cnt = block_size / 4`（每个 uint32_t 包含 1 个声道）

**注意**: block_size 必须是 32 的倍数

## 注意事项

1. **必须先配置**: 设备初始化后必须先调用 `lisa_i2s_configure()` 配置设备，才能使用其他 API
2. **block_size 限制**: 配置中的 `block_size` 必须是 32 的倍数，建议根据采样率和时间计算合适的值
3. **预加载数据**: 启动 TX 前建议预先调用 `lisa_i2s_write()` 加载最少1次，最大 `CONFIG_LISA_I2S_BLOCK_COUNT - 1` 次数据，避免下溢
4. **接收缓冲区**: `lisa_i2s_read()` 返回的缓冲区指针指向驱动内部循环缓冲区，不需要释放，应及时拷贝数据以防被覆盖
5. **主从同步**: 从模式下必须连接到主设备，由主设备提供时钟信号（BCK 和 LRCK）
6. **事件回调**: 在中断上下文执行，应保持简短快速，不要执行阻塞操作
7. **线程安全**: 驱动支持全双工通信，可在不同线程中同时进行收发操作
9. **DMA 循环**: 驱动使用循环 DMA 缓冲区，应用层需持续调用 `write`/`read` 以避免下溢/溢出
9. **数据格式**: 对于 16 位数据宽度，每个 uint32_t 包含左右两个声道；其他数据宽度每个 uint32_t 包含一个声道
10. **数据协议**: 目前暂时不支持PCM长帧模式 

## 示例代码

参考 `samples/drivers/devices/lisa_i2s/` 目录下的完整示例：
- `master_tx/` - 主模式发送示例
- `master_rx/` - 主模式接收示例
- `master_rx_tx/` - 主模式双向传输示例
- `slave_tx/` - 从模式发送示例
- `slave_rx/` - 从模式接收示例
- `slave_rx_tx/` - 从模式双向传输示例

## 文件说明

- `lisa_i2s.h` - 驱动头文件，包含所有 API 和类型定义
- `lisa_i2s_arcs.c` - ARCS 平台适配实现
- `CMakeLists.txt` - 构建配置
- `Kconfig` - 配置选项
