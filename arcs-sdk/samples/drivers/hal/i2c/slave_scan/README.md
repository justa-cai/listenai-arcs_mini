# I2C 从设备地址扫描示例

本示例演示如何使用 I2C 驱动扫描总线上的所有从设备地址，并识别连接的 I2C 设备。

## 📖 示例说明

### 功能演示

本示例演示以下功能：

- **I2C 主机模式配置**：配置 I2C0 控制器为主机模式
- **地址扫描**：遍历所有可能的 7 位 I2C 地址（0x01 ~ 0x7F）
- **设备检测**：通过地址应答（ACK）检测从设备存在
- **引脚复用配置**：配置 I2C 的 SCL 和 SDA 引脚
- **总线速度配置**：配置为标准速度模式（100kHz）

### 硬件要求

- ARCS 系列开发板
- I2C 从设备（用于测试扫描功能）
- USB 转串口工具（用于查看日志输出）
- 硬件连接：
  - I2C0 SCL：GPIO A23
  - I2C0 SDA：GPIO A22
  - GND：与 I2C 从设备共地

## 🚀 快速开始

### 1. 硬件连接

将 I2C 从设备连接到开发板：
- I2C 从设备 SCL → 开发板 GPIO A23
- I2C 从设备 SDA → 开发板 GPIO A22
- I2C 从设备 GND → 开发板 GND

### 2. 构建项目

在示例目录下执行构建脚本：

```bash
cd samples/drivers/i2c/slave_scan
./build.sh
```

或者在 SDK 根目录执行：

```bash
./build.sh -S samples/drivers/i2c/slave_scan -C
```

构建成功后，会在 `build` 目录下生成 `arcs.bin` 文件。

### 3. 烧录运行

将开发板连接到 PC，执行烧录命令：

```bash
cskburn -s /dev/ttyUSB0 -b 3000000 0x0 build/i2c.bin
```

参数说明：
- `-s /dev/ttyUSB0`：串口设备路径（根据实际情况修改）
- `-b 3000000`：烧录波特率
- `0x0`：烧录起始地址
- `build/i2c.bin`：烧录固件路径

> 💡 详细烧录步骤请参考 {ref}`快速开始 - 烧录运行 <flashing>`。

### 4. 查看输出

烧录完成后，复位开发板，通过串口工具查看日志输出。

## 🔧 配置说明

### 项目配置

本示例在 `prj.conf` 中配置：

```conf
CONFIG_MEM_CONFIG=y
```

### I2C 参数配置

| 参数 | 值 | 说明 |
|------|-----|------|
| I2C 控制器 | I2C0 | 使用 I2C0 外设 |
| SCL 引脚 | GPIO A23 | 时钟线引脚 |
| SDA 引脚 | GPIO A22 | 数据线引脚 |
| 工作模式 | 主机模式 | 作为 I2C 主机 |
| 总线速度 | 标准模式 | 100kHz |
| 扫描地址范围 | 0x01 ~ 0x7F | 7 位地址空间 |

## 📋 代码解析

### 关键代码段

#### 1. I2C 事件回调

```c
static void *gI2CDev = NULL;
static volatile uint32_t I2C_M_Event = 0;

static void i2c_cb(uint32_t event, void *workspace)
{
    I2C_M_Event |= event;
}
```

#### 2. I2C 引脚配置

```c
#define IIC0_GPIO_SCL      (23)
#define IIC0_GPIO_SDA      (22)

static void board_init(void)
{
    // 配置 SDA 引脚为 I2C 功能
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, IIC0_GPIO_SDA, CSK_IOMUX_FUNC_ALTER8);
    // 配置 SCL 引脚为 I2C 功能
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, IIC0_GPIO_SCL, CSK_IOMUX_FUNC_ALTER8);
}
```

#### 3. I2C 初始化

```c
// 获取 I2C0 句柄
gI2CDev = I2C0();

// 初始化 I2C，注册回调函数
I2C_Initialize(gI2CDev, i2c_cb, NULL);

// 使能 I2C 电源
I2C_PowerControl(gI2CDev, CSK_POWER_FULL);

// 设置为发送模式
I2C_Control(gI2CDev, CSK_I2C_TRANSMIT_MODE, 0);

// 设置总线速度为标准模式（100kHz）
I2C_Control(gI2CDev, CSK_I2C_BUS_SPEED, CSK_I2C_BUS_SPEED_STANDARD);

// 清除总线
I2C_Control(gI2CDev, CSK_I2C_BUS_CLEAR, 0);
```

#### 4. I2C 地址扫描

```c
int main(int argc, char **argv)
{
    board_init();
    
    printf("Start scanning I2C bus...\n");
    
    // 遍历所有 7 位 I2C 地址（1 ~ 127）
    for (uint8_t i = 1; i < 128; i++) {
        I2C_M_Event = 0;
        
        // 尝试向该地址发送 0 字节数据
        I2C_MasterTransmit(gI2CDev, i, NULL, 0, 0);
        
        // 等待 I2C 事件
        while (I2C_M_Event == 0) {
            ;
        }
        
        // 检查是否收到地址应答（ACK）
        if ((I2C_M_Event & CSK_I2C_EVENT_ADDRESS_ACK) == CSK_I2C_EVENT_ADDRESS_ACK) {
            printf("Found I2C device at address: 0x%02X\n", i);
        }
    }
    
    printf("I2C bus scan finished.\n");
    
    // 关闭 I2C
    I2C_PowerControl(gI2CDev, CSK_POWER_OFF);
    I2C_Uninitialize(gI2CDev);
    
    return 0;
}
```

## 🔍 预期输出

程序运行后，串口将输出以下日志（具体地址取决于连接的 I2C 从设备）：

```
Start scanning I2C bus...
Found I2C device at address: 0x5A
Found I2C device at address: 0x68
I2C bus scan finished.
```

输出说明：
- `Start scanning I2C bus...`：开始扫描 I2C 总线
- `Found I2C device at address: 0x5A`：在地址 0x5A 发现 I2C 设备
- `Found I2C device at address: 0x68`：在地址 0x68 发现 I2C 设备
- `I2C bus scan finished.`：扫描完成

如果没有连接任何 I2C 设备，将只输出开始和结束消息。

## ⚠️ 注意事项

1. **扫描地址范围**：
   - 本示例扫描地址范围：0x01 ~ 0x7F（1 ~ 127）
   - 代码中使用循环遍历所有地址

2. **设备检测**：
   - 通过 `CSK_I2C_EVENT_ADDRESS_ACK` 事件检测设备是否存在
   - 收到地址应答（ACK）表示该地址有设备

3. **引脚复用**：
   - SCL 和 SDA 需要配置为 I2C 功能（ALTER8）
   - 具体复用配置见芯片手册

4. **总线清除**：
   - `CSK_I2C_BUS_CLEAR` 用于清除总线异常状态
   - 在初始化时调用

5. **总线速度**：
   - 本示例配置为标准速度模式（`CSK_I2C_BUS_SPEED_STANDARD`）
