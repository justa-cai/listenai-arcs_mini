# Pinmux

## 什么是 Pinmux

引脚复用配置（Pinmux）用于将芯片的物理引脚配置为特定外设功能。例如，将某个引脚配置为 UART TX、SPI CLK 或 ADC 输入等。

在 ARCS SDK 中，驱动在初始化时会自动调用板型目录下定义的 pinmux 函数来完成引脚配置，**用户无需在应用代码中手动配置**。

## 板型中的 Pinmux 配置

### 文件位置

Pinmux 配置位于板型目录下：

```
boards/<板型名>/
├── pinmux.h          # pinmux 函数声明
└── pinmux.c          # pinmux 函数实现
```

### 函数命名规范

每个外设的 pinmux 函数按以下规则命名：

```c
void lisa_<外设名>_pinmux(void);
```

常用的 pinmux 函数：

| 函数名 | 说明 |
|--------|------|
| `lisa_uart0_pinmux()` | UART0 引脚配置 |
| `lisa_uart1_pinmux()` | UART1 引脚配置 |
| `lisa_spi0_pinmux()` | SPI0 引脚配置 |
| `lisa_spi1_pinmux()` | SPI1 引脚配置 |
| `lisa_i2c0_pinmux()` | I2C0 引脚配置 |
| `lisa_i2c1_pinmux()` | I2C1 引脚配置 |
| `lisa_adc_pinmux()` | ADC 引脚配置 |
| `lisa_pwm_pinmux()` | PWM 引脚配置 |
| `lisa_gpioa_pinmux()` | GPIOA 引脚配置 |
| `lisa_gpiob_pinmux()` | GPIOB 引脚配置 |
| `lisa_sdio_pinmux()` | SDIO 引脚配置 |
| `lisa_dvp_pinmux()` | DVP 摄像头接口引脚配置 |

### 自动调用机制

驱动在设备初始化时会自动调用对应的 pinmux 函数。例如：
- UART0 驱动初始化时调用 `lisa_uart0_pinmux()`
- ADC 驱动初始化时调用 `lisa_adc_pinmux()`
- SPI0 驱动初始化时调用 `lisa_spi0_pinmux()`

**重要：应用层代码无需手动调用这些函数！**

## 查看板型的 Pinmux 配置

### 示例：arcs_evb 板型

查看 `boards/arcs_evb/pinmux.c` 可以了解该板型的引脚配置：

```c
void lisa_uart0_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CP_LOG_RX_PIN, 2);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, CP_LOG_TX_PIN, 2);
}

void lisa_adc_pinmux()
{
    AON_IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 6, 3);
}

void lisa_spi1_pinmux()
{
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 5, 6);  // SCK
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 3, 6);  // MISO
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_B, 1, 6);  // MOSI
}
```

查看 `boards/arcs_evb/pinmux.h` 可以看到引脚别名定义：

```c
#define CP_LOG_RX_PIN 2
#define CP_LOG_TX_PIN 3
#define AP_LOG_TX_PIN 21
// ...
```

## 自定义板型时如何配置 Pinmux

### 使用 Pinmux 配置工具

**重要提示：`pinmux.h` 和 `pinmux.c` 由 Pinmux 配置工具自动生成，不建议手动编辑。**

#### 配置步骤

1. **访问 Pinmux 配置工具**
   - 工具地址：https://tool.listenai.com/ls-pinmux-tool/

2. **选择芯片型号**
   - 根据你的硬件选择对应的芯片型号

3. **配置外设引脚**
   - 根据硬件原理图配置项目中使用的外设（如 UART0、SPI0、I2C0、ADC 等）
   - 为每个外设选择对应的引脚映射

4. **生成代码**
   - 工具会自动生成 `pinmux.h` 和 `pinmux.c` 文件
   - 生成的文件包含所有已配置外设的 pinmux 函数实现

5. **集成到板型目录**
   - 将生成的 `pinmux.h` 和 `pinmux.c` 保存到 `boards/<你的板型>/` 目录下
   - 重新编译项目

#### 修改已有配置

如果需要修改引脚配置：
1. 在 Pinmux 配置工具中重新配置
2. 重新生成 `pinmux.h` 和 `pinmux.c`
3. 替换板型目录下的对应文件
4. 虽然不建议，但可以手动调整pinmux代码，参考官方提供的IO MAP表


## 配置参数说明

### IOMuxManager_PinConfigure

用于配置普通电源域的引脚：

```c
void IOMuxManager_PinConfigure(pad_group, pin_number, function);
```

参数：
- `pad_group`: 引脚组，可选值：
  - `CSK_IOMUX_PAD_A` - PAD_A 组
  - `CSK_IOMUX_PAD_B` - PAD_B 组
- `pin_number`: 引脚编号（0-31）
- `function`: 功能码（0-31，具体值参考芯片手册）

### AON_IOMuxManager_PinConfigure

用于配置 Always-On 电源域的引脚（通常用于 ADC、部分 GPIO）：

```c
void AON_IOMuxManager_PinConfigure(pad_group, pin_number, function);
```

参数与 `IOMuxManager_PinConfigure` 相同。

**如何判断使用哪个？**
- 查看芯片手册中引脚所属的电源域
- 参考 SDK 内置板型（arcs_evb、arcs_mini）的配置
- 通常 ADC 引脚使用 `AON_IOMuxManager`，其他外设使用 `IOMuxManager`

## 常见问题

### Q: 应用代码中需要调用 pinmux 函数吗？

**不需要。** 驱动初始化时会自动调用，应用层只需要使用设备 API 即可。

### Q: 修改了 pinmux 配置后需要重新编译吗？

**是的。** Pinmux 配置是板型的一部分，修改后需要重新编译整个项目。

### Q: 如何确认我的 pinmux 配置是否正确？

1. 仔细对照硬件原理图和芯片手册
2. 参考 SDK 内置板型的配置示例
3. 编译运行后测试对应外设功能是否正常

### Q: 如果不使用某个外设，需要提供 pinmux 函数吗？

**不需要。** 如果不使用某个外设（如 SPI0），只需在 Kconfig 中不使能对应的驱动即可：

```kconfig
# CONFIG_LISA_SPI is not set
```

这样驱动不会被编译，也不会调用对应的 pinmux 函数，不会产生链接错误。

### Q: 可以在运行时动态改变引脚功能吗？

技术上可以再次调用 `IOMuxManager_PinConfigure` 来改变引脚功能，但**不建议**这样做，因为可能导致外设状态混乱。Pinmux 配置应该在初始化阶段完成并保持不变。

