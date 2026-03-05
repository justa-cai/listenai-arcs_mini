# I2C 驱动

基于 lisa_device 框架的 I2C 设备驱动，为 ARCS 平台提供统一的 I2C 通信接口。

## 功能特性

- **设备支持**: I2C0、I2C1 两个独立的 I2C 控制器，可独立配置和使用
- **灵活配置**: 可选择性启用 I2C0 或 I2C1，节省资源
- **主机模式**: 支持标准模式（100kHz）、快速模式（400kHz）、快速增强模式（1MHz）
- **传输接口**: 支持单次读写和组合传输（写后读）
- **设备探测**: 支持 0 字节传输进行设备探测
- **线程安全**: 内部使用互斥锁保护并发访问

## 配置选项

在 `prj.conf` 中启用驱动：

```kconfig
CONFIG_LISA_I2C=y          # 使能 I2C 驱动框架
CONFIG_LISA_I2C0=y         # 使能 I2C0 控制器（可选）
CONFIG_LISA_I2C1=y         # 使能 I2C1 控制器（可选）
```

**注意**: 根据应用需求选择启用对应的 I2C 控制器，未启用的控制器不会编译进代码。

## API 接口

### 配置接口

```c
int lisa_i2c_configure(lisa_device_t *dev, const lisa_i2c_config_t *config);
int lisa_i2c_get_config(lisa_device_t *dev, lisa_i2c_config_t *config);
```

### 传输接口

```c
int lisa_i2c_transfer(lisa_device_t *dev, lisa_i2c_msg_t *msgs, uint32_t num_msgs);
int lisa_i2c_write(lisa_device_t *dev, uint16_t addr, const uint8_t *buf, uint32_t len);
int lisa_i2c_read(lisa_device_t *dev, uint16_t addr, uint8_t *buf, uint32_t len);
```

## 速度模式

驱动支持以下速度模式：

- `LISA_I2C_SPEED_STANDARD` (100000 Hz): 标准模式
- `LISA_I2C_SPEED_FAST` (400000 Hz): 快速模式
- `LISA_I2C_SPEED_FAST_PLUS` (1000000 Hz): 快速增强模式

## 传输标志

- `LISA_I2C_FLAG_NONE`: 无特殊标志（默认为写操作）
- `LISA_I2C_FLAG_READ`: 读操作标志（不设置则为写操作）
- `LISA_I2C_FLAG_NO_START`: 不发送 START 条件（当前实现中忽略）
- `LISA_I2C_FLAG_NO_STOP`: 不发送 STOP 条件（用于组合传输）
- `LISA_I2C_FLAG_10BIT_ADDR`: 使用 10 位地址模式

## 使用示例

### 基本配置和读写

```c
#include "lisa_i2c.h"
#include "lisa_device.h"

// 1. 获取 I2C 设备
lisa_device_t *i2c_dev = lisa_device_get("i2c0");
if (!lisa_device_ready(i2c_dev)) {
    printf("Error: i2c0 device not ready\n");
    return -1;
}

// 2. 配置 I2C 总线（可选，默认为标准模式）
lisa_i2c_config_t config = {
    .speed = LISA_I2C_SPEED_FAST,  // 400kHz
    .master_mode = true,
    .slave_addr = 0
};
lisa_i2c_configure(i2c_dev, &config);

// 3. 写入数据
uint8_t write_data[] = {0x01, 0x02, 0x03};
int ret = lisa_i2c_write(i2c_dev, 0x50, write_data, sizeof(write_data));
if (ret != 0) {
    printf("Write failed: %d\n", ret);
}

// 4. 读取数据
uint8_t read_data[4];
ret = lisa_i2c_read(i2c_dev, 0x50, read_data, sizeof(read_data));
if (ret != 0) {
    printf("Read failed: %d\n", ret);
}
```

### 组合传输（写后读）

```c
// 使用 transfer 接口实现写后读操作
lisa_i2c_msg_t msgs[2];

// 第一个消息：写入寄存器地址
uint8_t reg_addr = 0x01;
msgs[0].addr = 0x50;
msgs[0].flags = LISA_I2C_FLAG_NO_STOP;  // 写操作，不发送 STOP，继续传输
msgs[0].len = 1;
msgs[0].buf = &reg_addr;

// 第二个消息：读取数据
uint8_t read_buf[4];
msgs[1].addr = 0x50;
msgs[1].flags = LISA_I2C_FLAG_READ;     // 读操作标志
msgs[1].len = sizeof(read_buf);
msgs[1].buf = read_buf;

int ret = lisa_i2c_transfer(i2c_dev, msgs, 2);
if (ret != 0) {
    printf("Transfer failed: %d\n", ret);
}
```

### 设备探测

```c
// 使用 0 字节传输探测设备是否存在
lisa_i2c_msg_t msg = {
    .addr = 0x50,
    .flags = LISA_I2C_FLAG_NONE,
    .len = 0,      // 0 字节传输
    .buf = NULL
};

int ret = lisa_i2c_transfer(i2c_dev, &msg, 1);
if (ret == LISA_DEVICE_OK) {
    printf("Device found at address 0x50\n");
} else if (ret == LISA_DEVICE_ERR_NACK) {
    printf("No device at address 0x50\n");
}
```

## 硬件配置

### 引脚复用配置

I2C 驱动在初始化时会自动调用板型目录中定义的 `lisa_i2c0_pinmux()` 和 `lisa_i2c1_pinmux()` 函数，用于配置 I2C 引脚的复用功能。

**配置位置**:
- **定义**: `boards/<板型名>/pinmux.c` 中实现 `lisa_i2c0_pinmux()` 和 `lisa_i2c1_pinmux()` 函数
- **声明**: `boards/<板型名>/pinmux.h` 中声明函数
- **调用时机**: I2C0/I2C1 设备初始化时自动调用

**示例** (参考 `boards/arcs_evb/pinmux.c`):

```c
void lisa_i2c0_pinmux()
{
    /* I2C0: PA22=SDA, PA23=SCL, 功能码 8 */
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 22, 8);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, 23, 8);
}

void lisa_i2c1_pinmux()
{
    /* I2C1: PB0=SCL, PB1=SDA, 功能码 9 */
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 0, 9);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 1, 9);
}
```

**注意**:
- 该函数由板型相关代码实现，不同板型的引脚配置可能不同
- 只需配置实际使用的 I2C 控制器引脚
- I2C 功能码通常为 8 或 9，具体请参考芯片手册
- 如需自定义引脚配置，可在 sample 中重写 pinmux 函数（参考 `samples/drivers/devices/lisa_uart/poll_in/src/main.c`）

## 注意事项

1. **引脚配置**: 使用 I2C 前需要先配置对应的 GPIO 引脚为 I2C 功能，驱动初始化时会自动调用 pinmux 函数

2. **设备选择**: 使用前需在 `prj.conf` 中启用对应的 I2C 控制器

3. **设备初始化**: 系统启动时会自动初始化已启用的 I2C 设备

4. **线程安全**: 驱动内部已实现线程保护，可在多线程环境中使用

5. **超时处理**: 所有传输操作都有 1 秒的超时限制

6. **错误处理**: 
   - `LISA_DEVICE_ERR_NACK`: 从机无应答（设备不存在或地址错误）
   - `LISA_DEVICE_ERR_TIMEOUT`: 传输超时
   - `LISA_DEVICE_ERR_IO`: 硬件 IO 错误

7. **transfer 函数使用**: 
   - 支持读写操作，通过 `LISA_I2C_FLAG_READ` 标志区分
   - 支持组合传输（使用 `LISA_I2C_FLAG_NO_STOP` 实现写后读）
   - 支持 0 字节传输进行设备探测
   - 简单读写建议使用 `lisa_i2c_read()` 和 `lisa_i2c_write()` 函数

8. **引脚上拉**: I2C 总线需要外接上拉电阻（通常 4.7kΩ），或确认开发板已提供默认上拉

9. **设备探测**: 使用 0 字节传输（`len=0`）进行设备探测时，驱动会严格检查 ACK 响应，确保探测结果准确

## 文件说明

- `lisa_i2c.h` - 驱动头文件，包含所有 API 和类型定义
- `lisa_i2c_arcs.c` - ARCS 平台适配实现
- `CMakeLists.txt` - 构建配置
- `Kconfig` - 配置选项
- `README.md` - 驱动使用说明

## 示例程序

完整的示例程序位于：

- `samples/drivers/devices/lisa_i2c/basic_write_read/` - I2C 基础读写示例
  - 演示 `lisa_i2c_write()` API 用法
  - 演示 `lisa_i2c_read()` API 用法
  - 演示 `lisa_i2c_transfer()` 组合传输用法
  
- `samples/drivers/devices/lisa_i2c/slave_scan/` - I2C 从机扫描示例
  - 扫描 I2C 总线上的所有从机地址
  - 检测已连接的 I2C 设备
