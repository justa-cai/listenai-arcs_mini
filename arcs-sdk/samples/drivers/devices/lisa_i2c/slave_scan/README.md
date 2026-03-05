# LISA I2C 总线扫描示例

## 功能说明

本示例展示如何基于 LISA I2C 驱动遍历 7 位地址空间（0x01 ~ 0x7F），通过 ACK/NACK 检测识别总线上的 I2C 从设备。

底层使用 0 字节传输进行设备探测，通过检查 ACK 响应判断设备是否存在。任意 7 位地址的 I2C 从设备都可以被检测到。

## 硬件连接

- **PA22**: I2C0 SDA（连接到待扫描总线的 SDA，并保证上拉）
- **PA23**: I2C0 SCL（连接到待扫描总线的 SCL，并保证上拉）
- **GND**: 与从设备共地

## 示例步骤

1. 获取 `i2c0` 设备并配置为标准速率主机模式
2. 配置 I2C 引脚复用（PA22=SDA, PA23=SCL，通过 pinmux 重定向）
3. 使用 `lisa_i2c_transfer()` 依次向每个地址发送 0 字节消息
4. 根据是否收到 ACK 判断设备是否存在
5. 打印检测到的设备地址

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 预期输出

**终端输出：**

```
=== LISA I2C bus scan example ===
I2C0 SDA@PA22 / SCL@PA23 configured
i2c0 device ready
Start scanning I2C bus...
Found I2C device at address: 0x68
Found I2C device at address: 0x76
I2C bus scan finished
```

> **注意**：如果总线上没有任何设备，会提示 "No I2C devices detected"。

## 核心 API

| API | 说明 |
|-----|------|
| `lisa_device_get()` | 获取 I2C 设备 |
| `lisa_i2c_configure()` | 配置 I2C 总线参数 |
| `lisa_i2c_transfer()` | 发送 0 字节消息进行设备探测 |

## 设备探测说明

`lisa_i2c_transfer()` 在设备探测时的工作原理：

- **0 字节传输**：发送从机地址后不传输数据，只检查 ACK 响应
- **ACK 检测**：如果设备存在，会返回 ACK；如果不存在，会返回 NACK
- **返回值**：
  - `LISA_DEVICE_OK` (0): 设备存在（收到 ACK）
  - `LISA_DEVICE_ERR_NACK` (-13): 设备不存在（收到 NACK）

## 配置说明

示例已在 `prj.conf` 中配置：

```ini
CONFIG_LISA_DEVICE=y      # 使能 LISA 设备框架
CONFIG_LISA_I2C=y         # 使能 I2C 驱动
CONFIG_LISA_I2C0=y        # 使能 I2C0 控制器
```

## 注意事项

1. **引脚上拉**：引脚需外挂上拉电阻，或确认开发板已提供默认上拉

2. **地址范围**：扫描范围为 0x01 ~ 0x7F（7 位地址空间），如需扫描特定地址范围，可以在 `main.c` 中调整循环边界

3. **引脚配置**：示例中通过 pinmux 重定向自定义引脚配置，会覆盖 `boards/arcs_evb/pinmux.c` 中的默认实现（默认实现仅配置了 PA23，缺少 PA22）

4. **进一步操作**：检测到设备后，若要进一步对某个地址进行读写，可参考 `basic_write_read` 示例
