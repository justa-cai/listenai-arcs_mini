# Display LCD 示例

该示例演示如何使用 ST7789P3 驱动，进行简单的全屏刷图操作。

## 📖 示例说明

### 功能演示

本示例演示以下功能：

- **LCD 初始化**：初始化 ST7789P3 显示屏
- **全屏刷图**：填充整个屏幕为纯色
- **颜色切换**：循环显示红、绿、蓝三种颜色
- **亮度调节**：动态调节背光亮度（PWM）
- **RGB565 格式**：使用 RGB565 像素格式

### LCD 引脚配置

| 引脚名称 | 引脚号 |
| --- | --- |
| LCD_CS | PB05 |
| LCD_CLK | PB03 |
| LCD_SDA(MOSI) | PB01 |
| LCD_DC | PB00 |
| LCD_RST | PA01 |
| LCD_BL(PWM) | PA00 |

### 硬件要求

- CSK 系列开发板
- ST7789P3 显示屏（SPI 四线，240×320）

## 🚀 快速开始

### 1. 构建项目

在示例目录下执行构建脚本：

```bash
cd samples/drivers/display/lcd
./build.sh
```

构建成功后，会在 `build` 目录下生成 `arcs.bin` 文件。

### 2. 烧录运行

将开发板连接到 PC，执行烧录命令：

```bash
cskburn -s /dev/ttyUSB0 -b 3000000 0x0 build/lcd.bin
```

参数说明：
- `-s /dev/ttyUSB0`：串口设备路径（根据实际情况修改）
- `-b 3000000`：烧录波特率
- `0x0`：烧录起始地址
- `build/lcd.bin`：烧录固件路径

> 💡 详细烧录步骤请参考 {ref}`快速开始 - 烧录运行 <flashing>`。

### 3. 查看输出

烧录完成后，复位开发板，观察屏幕显示红、绿、蓝三色循环切换。

## 🔧 配置说明

### 项目配置

本示例在 `prj.conf` 中配置：

```conf
CONFIG_LISA_DISPLAY=y
CONFIG_LISA_DISPLAY_ST7789P3=y
CONFIG_LISA_DISPLAY_INTERFACE_SPI_4_LINE=y
CONFIG_DISPLAY_ST7789P3_WIDTH=240

CONFIG_LOG=y
CONFIG_BACK_TRACE=y
CONFIG_MODULE_HEAP=y
CONFIG_PSRAM_HEAP_SIZE=0x3b0000

CONFIG_SDK_MODULE_COLLECTIONS_C=y
CONFIG_LISA_OS=y
```

### 显示参数

| 参数 | 值 |
|------|-----|
| 显示屏 | ST7789P3 |
| 接口类型 | SPI 四线 |
| 屏幕宽度 | 240 |
| 像素格式 | RGB565 |
| 背光 PWM 频率 | 1000 Hz |

## 📋 代码解析

### 关键代码段

#### 1. 颜色枚举定义

```c
enum color {
    COLOR_RED,
    COLOR_GREEN,
    COLOR_BLUE,
    COLOR_CUSTOM
};
```

#### 2. RGB565 颜色转换

```c
static uint16_t get_rgb565_color(enum color rgb, uint8_t grey)
{
    uint16_t color = 0;
    uint16_t grey_5bit;

    switch (rgb) {
    case COLOR_RED:
        color = 0xF800u;
        break;
    case COLOR_GREEN:
        color = 0x07E0u;
        break;
    case COLOR_BLUE:
        color = 0x001Fu;
        break;
    case COLOR_CUSTOM:
        grey_5bit = grey & 0x1Fu;
        /* shift the green an extra bit, it has 6 bits */
        color = grey_5bit << 11 | grey_5bit << (5 + 1) | grey_5bit;
        break;
    }
    return color;
}
```

#### 3. 填充缓冲区

```c
static void fill_buffer_rgb565(enum color rgb, uint8_t grey, uint8_t *buf, size_t buf_size)
{
    uint16_t color = get_rgb565_color(rgb, grey);

    // 小端模式
    for (size_t idx = 0; idx < buf_size; idx += 2) {
        *(buf + idx + 1) = (color >> 8) & 0xFFu;
        *(buf + idx + 0) = (color >> 0) & 0xFFu;
    }
}
```

#### 4. 硬件配置

```c
display_hw_config_t config = {
    .reset = {
        .pad = CSK_IOMUX_PAD_A,     /* RST -> PA01 */
        .pin = 1,
        .func = CSK_IOMUX_FUNC_ALTER1,
    },
    .blacklight = {
        .pin = {                     /* BL(PWM) -> PA00 */
            .pad = CSK_IOMUX_PAD_A,
            .pin = 0,
            .func = CSK_IOMUX_FUNC_ALTER12,
        },
        .dev = GPT0_PWM(),
        .channel = GPT_CHANNEL0,
        .freq = 1000,
    },
    .trans_config = {
        .spi_4line = {
            .spi_dev = SPI1(),
            .spi_tx_dma_ch = 3,
            .spi_pins = {
                .cs = {               /* CS -> PB05, ALT6 */
                    .pad = CSK_IOMUX_PAD_B,
                    .pin = 5,
                    .func = CSK_IOMUX_FUNC_ALTER6,
                },
                .clk = {              /* SCLK -> PB03, ALT6 */
                    .pad = CSK_IOMUX_PAD_B,
                    .pin = 3,
                    .func = CSK_IOMUX_FUNC_ALTER6,
                },
                .sda = {              /* MOSI -> PB01, ALT6 */
                    .pad = CSK_IOMUX_PAD_B,
                    .pin = 1,
                    .func = CSK_IOMUX_FUNC_ALTER6,
                },
                .dc = {               /* D/C -> PB00, DEFAULT */
                    .pad = CSK_IOMUX_PAD_B,
                    .pin = 0,
                    .func = CSK_IOMUX_FUNC_DEFAULT,
                },
            }
        }
    }
};
```

#### 5. 创建显示设备

```c
display_device = lisa_display_create(&config);
if(display_device == NULL){
    LOGE("lisa_display_create fail \r\n");
    return -1;
}
```

#### 6. 获取显示能力

```c
struct display_capabilities caps;
lisa_display_get_capabilities(display_device, &caps);
if(caps.current_pixel_format != PIXEL_FORMAT_RGB_565){
    LOGE("display format is not rgb565 \r\n");
    return -1;
}

LOGD("display capabilities:width = %d, height = %d format = 0x%x \r\n", 
     caps.x_resolution, caps.y_resolution, caps.current_pixel_format);
```

#### 7. 分配图像缓冲区

```c
struct display_buffer_descriptor desc;
desc.width = caps.x_resolution;
desc.height = caps.y_resolution;
desc.pitch = desc.width * 2;
desc.buf_size = desc.height * desc.pitch;

image_buf = (uint8_t *)exram_malloc(32, desc.buf_size);
if(image_buf == NULL){
    LOGE("[%s] malloc fail \r\n", __func__);
}
```

#### 8. 关闭消隐

```c
lisa_display_blanking_off(display_device);
```

#### 9. 主循环刷新屏幕

```c
uint8_t cnt = 0;
uint8_t pwm = 0;

fill_buffer_rgb565(COLOR_RED, 0x00, image_buf, desc.buf_size);

while (1) {
    switch(cnt) {
        case COLOR_RED:
            fill_buffer_rgb565(COLOR_RED, 0x00, image_buf, desc.buf_size);
            break;
        case COLOR_GREEN:
            fill_buffer_rgb565(COLOR_GREEN, 0x00, image_buf, desc.buf_size);
            break;
        case COLOR_BLUE:
            fill_buffer_rgb565(COLOR_BLUE, 0x00, image_buf, desc.buf_size);
            break;
        default:
            break;
    }

    cnt = (cnt + 1) % 3;
    pwm = (pwm + 1) % 100;
    lisa_display_set_brightness(display_device, pwm);
    
    lisa_display_write(display_device, 0, 0, &desc, image_buf);
    vTaskDelay(pdMS_TO_TICKS(1000));
}
```

## 🔍 预期输出

程序运行后：
- 屏幕循环显示红、绿、蓝三种颜色
- 每秒切换一次颜色
- 背光亮度从 0 到 99 循环变化

## ⚠️ 注意事项

1. **引脚配置**：
   - 代码中的引脚配置与原 README 中的引脚表格不一致
   - 请以代码中的 `display_hw_config_t` 配置为准

2. **SPI 接口**：
   - 使用 SPI1 外设
   - SPI DMA 通道为 3

3. **背光控制**：
   - 使用 GPT0_PWM 的 CHANNEL0
   - PWM 频率为 1000 Hz
   - 亮度值范围 0-99

4. **像素格式**：
   - 使用 RGB565 格式
   - 每个像素 2 字节
   - 小端模式存储

5. **内存分配**：
   - 使用 `exram_malloc()` 分配图像缓冲区
   - 缓冲区大小：`width × height × 2` 字节

6. **颜色循环**：
   - 使用 `cnt = (cnt + 1) % 3` 循环红绿蓝
   - 使用 `pwm = (pwm + 1) % 100` 循环亮度值

7. **刷新周期**：
   - 每秒刷新一次（`vTaskDelay(pdMS_TO_TICKS(1000))`）
