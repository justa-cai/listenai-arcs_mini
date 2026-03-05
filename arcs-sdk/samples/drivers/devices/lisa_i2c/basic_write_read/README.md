# LISA I2C 基础读写示例

## 功能说明

本示例演示 LISA I2C 驱动的三个基础 API 用法：写数据、读数据和组合传输（写后读）。

底层使用中断方式传输，支持标准模式（100kHz）和快速模式（400kHz）。本示例主要展示 API 的使用方法，不依赖特定硬件。实际运行时如果总线上没有设备，会提示 NACK 错误，这是正常的。

## 硬件连接

如需连接实际 I2C 设备测试：

- **PA22**: I2C0 SDA（连接到 I2C 设备的 SDA，需上拉）
- **PA23**: I2C0 SCL（连接到 I2C 设备的 SCL，需上拉）
- **GND**: 与 I2C 设备共地

常见的 I2C 设备：EEPROM (AT24Cxx)、传感器 (BME280)、RTC (DS1307) 等。

## 示例步骤

1. 获取 `i2c0` 设备并配置为标准速率主机模式
2. 配置 I2C 引脚复用（PA22=SDA, PA23=SCL，通过 pinmux 重定向）
3. 演示 `lisa_i2c_write()` API：写数据到 I2C 设备
4. 演示 `lisa_i2c_read()` API：从 I2C 设备读数据
5. 演示 `lisa_i2c_transfer()` API：组合传输（写寄存器地址后读数据）

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 预期输出

**终端输出：**

```
=== LISA I2C basic write/read example ===
I2C0 SDA@PA22 / SCL@PA23 configured
i2c0 device ready
I2C configured (speed: 100000 Hz)

========================================
Demonstrating LISA I2C API usage
Target device address: 0x50
Note: Examples show API usage, actual results depend on hardware
========================================

--- Example 1: Write API ---
Calling: lisa_i2c_write(dev, 0x50, data, 5)
Data to write: [0x00 0x55 0xAA 0x12 0x34]
Write operation completed successfully

--- Example 2: Read API ---
Calling: lisa_i2c_read(dev, 0x50, buffer, 4)
Read operation completed successfully
Data read: [0x00 0x00 0x00 0x00]

--- Example 3: Transfer API (Write-Read Combined) ---
Calling: lisa_i2c_transfer(dev, msgs, 2)
  Message[0]: WRITE reg_addr=0x00 (NO_STOP)
  Message[1]: READ 4 bytes (READ flag)
Transfer operation completed successfully
Data read: [0x00 0x00 0x00 0x00]

========================================
All API examples completed
========================================
```

> **注意**：如果总线上没有设备，会显示 "Device not responding (NACK)"，这是正常的。

## 核心 API

| API | 说明 |
|-----|------|
| `lisa_device_get()` | 获取 I2C 设备 |
| `lisa_i2c_configure()` | 配置 I2C 总线参数（速度、模式） |
| `lisa_i2c_write()` | 写数据到 I2C 从设备 |
| `lisa_i2c_read()` | 从 I2C 从设备读数据 |
| `lisa_i2c_transfer()` | 组合传输（支持写后读等复杂操作） |

## 关键代码

### API 1: lisa_i2c_write()

写数据到 I2C 从设备：

```c
uint8_t write_data[] = {0x00, 0x55, 0xAA, 0x12, 0x34};
int ret = lisa_i2c_write(dev, slave_addr, write_data, sizeof(write_data));
```

### API 2: lisa_i2c_read()

从 I2C 从设备读取数据：

```c
uint8_t read_data[4];
int ret = lisa_i2c_read(dev, slave_addr, read_data, sizeof(read_data));
```

### API 3: lisa_i2c_transfer()

组合传输（典型用法：写寄存器地址 + 读寄存器值）：

```c
lisa_i2c_msg_t msgs[2];

/* 消息1：写寄存器地址 */
uint8_t reg_addr = 0x00;
msgs[0].addr = slave_addr;
msgs[0].flags = LISA_I2C_FLAG_NO_STOP;  /* 不发送 STOP */
msgs[0].len = 1;
msgs[0].buf = &reg_addr;

/* 消息2：读取数据 */
uint8_t read_buf[4];
msgs[1].addr = slave_addr;
msgs[1].flags = LISA_I2C_FLAG_READ;     /* 读操作标志 */
msgs[1].len = sizeof(read_buf);
msgs[1].buf = read_buf;

int ret = lisa_i2c_transfer(dev, msgs, 2);
```

## 配置说明

示例已在 `prj.conf` 中配置：

```ini
CONFIG_LISA_DEVICE=y      # 使能 LISA 设备框架
CONFIG_LISA_I2C=y         # 使能 I2C 驱动
CONFIG_LISA_I2C0=y        # 使能 I2C0 控制器
CONFIG_LOG=y              # 使能日志功能
```

## 注意事项

1. **读操作标志**：使用 `lisa_i2c_transfer()` 进行读操作时，必须设置 `LISA_I2C_FLAG_READ` 标志，否则会被当作写操作

2. **写后读操作**：组合传输时，第一个消息需要设置 `LISA_I2C_FLAG_NO_STOP`，避免发送 STOP 条件

3. **返回值处理**：
   - `LISA_DEVICE_OK` (0): 成功
   - `LISA_DEVICE_ERR_NACK` (-13): 设备无响应（地址错误或设备不存在）
   - 其他负值: 其他错误

4. **引脚上拉**：I2C 总线需要外接上拉电阻（通常 4.7kΩ），或确认开发板已提供默认上拉

5. **引脚配置**：示例中通过 pinmux 重定向自定义引脚配置，会覆盖 `boards/arcs_evb/pinmux.c` 中的默认实现（默认实现仅配置了 PA23，缺少 PA22）

6. **设备地址**：修改 `DEVICE_ADDR` 宏以匹配实际设备地址

## 实际应用场景

修改 `DEVICE_ADDR` 后可用于：

| 设备类型 | 典型地址 | 用途 |
|---------|---------|------|
| AT24C02 EEPROM | 0x50 | 数据存储 |
| BME280 传感器 | 0x76/0x77 | 温湿度气压 |
| MPU6050 IMU | 0x68/0x69 | 加速度陀螺仪 |
| DS1307 RTC | 0x68 | 实时时钟 |
| PCF8574 IO扩展 | 0x20-0x27 | GPIO 扩展 |
