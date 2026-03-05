# LISA Display 显示屏驱动示例（SPI 4-Wire 模式 + PWM 背光）

## 功能说明

演示如何使用 LISA Display 驱动通过 SPI 4-Wire 接口驱动 ST7789P3 LCD 显示屏，实现全屏颜色填充和 PWM 背光亮度控制。

### 新特性

- **SPI 4-Wire 总线**: 使用标准 SPI 接口 + D/C 引脚控制命令/数据模式
- **PWM 背光控制**: 通过 PWM 输出实现背光亮度无级调节
- **动态颜色切换**: 循环显示红、绿、蓝三种颜色
- **亮度渐变**: 背光亮度从 0% 逐步增加到 99% 循环变化

## 硬件连接

### SPI 接口（GPIOB）

- **PB5**: SPI1 CLK（时钟）
- **PB3**: SPI1 MOSI（数据）
- **PB1**: LCD CS（片选，软件控制）
- **PB0**: LCD D/C（数据/命令选择）

### 控制引脚（GPIOA）

- **PA1**: LCD RST（复位）
- **PA0**: LCD 背光 PWM

### 其他引脚（GPIOB）

- **PB8**: LCD TE（帧同步，可选）

## 使用场景

适用于需要驱动 SPI 接口 LCD 显示屏的应用场景，如智能穿戴设备、小型显示终端等。本示例展示了完整的显示屏初始化、全屏刷新和背光控制流程。

## 示例步骤

1. 配置 GPIO 和 SPI 引脚复用
2. 获取 GPIO、SPI、PWM、Display 设备
3. 配置 Display 总线参数（SPI 频率、CS/DC 引脚、背光 PWM）
4. 关联总线配置到 Display 设备
5. 获取显示屏分辨率，分配帧缓冲区
6. 关闭 blanking，开始显示
7. 循环切换颜色并调整背光亮度

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 预期输出

**终端输出：**

```
Display sample started
Display capabilities: 240 x 320
```

**显示屏效果：**

- 屏幕每秒切换一次颜色（红 → 绿 → 蓝 → 红 ...）
- 背光亮度逐渐增加（0% → 99% 循环）

## 核心 API

| API | 说明 |
|-----|------|
| `lisa_device_get()` | 获取设备实例（GPIO、SPI、PWM、Display） |
| `lisa_display_attach_bus()` | 关联总线配置到显示设备 |
| `lisa_display_get_capabilities()` | 获取显示屏分辨率等参数 |
| `lisa_display_blanking_off()` | 关闭 blanking，开始显示 |
| `lisa_display_write()` | 写入帧缓冲区数据到显示屏 |
| `lisa_display_set_brightness()` | 设置背光亮度（0-100） |

## 关键代码

```c
/* 配置 Display 总线 */
lisa_display_config_t display_config = {
    .bus_type = LISA_DISPLAY_BUS_SPI_4WIRE,
    .bus_config = {
        .spi_4wire = {
            .spi_dev = lisa_device_get("spi1"),
            .cs_gpio = gpiob_dev,
            .cs_pin = LCD_CS_PIN,
            .dc_gpio = gpiob_dev,
            .dc_pin = LCD_CD_PIN,
            .spi_freq = 50 * 1000 * 1000, // 50MHz
        }
    },
    .backlight = {
        .type = LISA_DISPLAY_BACKLIGHT_TYPE_PWM,
        .config.pwm = {
            .channel = 0,
            .dev = lisa_device_get("pwm0"),
            .freq = 2000
        }
    },
    .rst_gpio = gpioa_dev,
    .rst_pin = LCD_RST_PIN,
};

/* 关联总线配置 */
lisa_display_attach_bus(display_device, &display_config);

/* 写入帧数据 */
lisa_display_write(display_device, 0, 0, &desc, buffer);

/* 设置背光亮度 */
lisa_display_set_brightness(display_device, brightness);
```

## 配置说明

### Kconfig 配置

```kconfig
CONFIG_LISA_DISPLAY_DEVICE=y           # 启用 Display 设备
CONFIG_LISA_DISPLAY_PANEL_ST7789P3=y   # 使用 ST7789P3 面板驱动
CONFIG_PANEL_ST7789P3_WIDTH=240        # 屏幕宽度
CONFIG_PANEL_ST7789P3_HEIGHT=320       # 屏幕高度
CONFIG_LISA_DISPLAY_BUS_SPI_4WIRE=y    # 启用 SPI 4-Wire 总线
```

### 依赖设备

- **GPIO**: `gpioa`、`gpiob`（用于 RST、CS、DC、TE 引脚控制）
- **SPI**: `spi1`（用于数据传输）
- **PWM**: `pwm0`（用于背光控制）

## 注意事项

1. **引脚配置**: 示例中重写了 `lisa_gpioa_pinmux()`、`lisa_gpiob_pinmux()`、`lisa_spi1_pinmux()` 函数，确保引脚复用正确
2. **内存分配**: 帧缓冲区使用 `lisa_mem_alloc()` 从 PSRAM 分配，需确保 `CONFIG_PSRAM_HEAP_SIZE` 足够大
3. **SPI 频率**: 示例使用 50MHz，实际频率需根据屏幕规格和走线质量调整
4. **背光范围**: `lisa_display_set_brightness()` 参数范围为 0-100，表示百分比
5. **颜色格式**: 使用 RGB565 格式，每像素 2 字节

## 文件说明

- `src/main.c` - 示例主程序
- `prj.conf` - Kconfig 配置
- `CMakeLists.txt` - 构建配置
