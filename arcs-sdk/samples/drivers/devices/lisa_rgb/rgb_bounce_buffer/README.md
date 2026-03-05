# LISA RGB Bounce Buffer 显示示例

## 功能说明

演示如何使用 RGB 驱动的 Bounce Buffer 模式实现高效的彩色图案显示，展示双缓冲机制和自动帧切换功能。

底层采用 Ping-Pong DMA 和双帧缓冲区机制，应用层通过 PSRAM 分配大帧缓冲区，驱动层自动管理缓冲区切换，实现流畅的显示效果。

### 新特性

- **双缓冲机制**：使用两个独立的显示缓冲区，支持无撕裂的帧切换
- **Ping-Pong DMA**：硬件自动在两个 Bounce Buffer 间切换，提高传输效率
- **自动帧切换**：DMA 中断自动管理缓冲区切换，应用层无需手动控制
- **PSRAM 大缓冲**：帧缓冲区分配在 PSRAM，内部 SRAM 用于 Bounce Buffer，优化内存使用
- **高分辨率支持**：演示 480x480 分辨率全屏显示，适合高像素密度屏幕

## 硬件连接

使用 RGB 并行接口连接屏幕，具体引脚配置由 Device Tree 或板级配置文件定义。

典型连接包括：
- **RGB 数据线**：16 位并行数据 (RGB565 格式)
- **时钟信号**：PCLK (像素时钟)
- **同步信号**：HSYNC (行同步), VSYNC (帧同步), DE (数据使能)
- **背光控制**：PWM 或 GPIO 控制 (可选)

**注意**：RGB 屏幕还需要独立的命令通道（3-Wire SPI 或 I2C）用于初始化配置，由 Display Panel 驱动层自动管理。

## 使用场景

适用于需要高分辨率、流畅显示效果的应用场景，如：
- 彩色图形用户界面 (GUI)
- 视频播放和图片浏览
- 实时数据可视化
- 动画和过渡效果

Bounce Buffer 模式特别适合内存受限的嵌入式系统，通过 DMA 分块传输大帧缓冲区，降低内部 SRAM 需求。

## 示例步骤

1. 获取 RGB 控制器设备 (`rgb0`)
2. 从 PSRAM 分配帧缓冲区 (480x480x2 字节)
3. 循环绘制不同的彩色图案（纯色、彩条、渐变）
4. 调用 `lisa_rgb_update_framebuffer()` 更新显示
5. DMA 中断自动处理缓冲区切换和数据传输
6. 延时后切换到下一个图案

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 预期输出

**终端输出：**
```
=== LISA RGB Bounce Buffer 模式示例 ===
RGB device ready
Framebuffer allocated: 0x20000000 (size: 460800 bytes)
Starting pattern display loop...

Pattern 0: Red screen
Pattern 1: Green screen
Pattern 2: Blue screen
Pattern 3: White screen
Pattern 4: Color bars
Pattern 5: Gradient
Pattern 6: Red screen
...
```

**屏幕显示：**
- 每 2 秒切换一次图案
- 依次显示：红色 → 绿色 → 蓝色 → 白色 → 彩色条纹 → 渐变效果
- 画面切换流畅，无撕裂现象

## 核心 API

| API | 说明 |
|-----|------|
| `lisa_device_get()` | 获取 RGB 控制器设备 |
| `lisa_mem_alloc()` | 从 PSRAM 分配内存 |
| `lisa_rgb_update_framebuffer()` | 更新显示帧缓冲区（拷贝到非显示缓冲区） |
| `lisa_os_delay_ms()` | 延时函数 |
| `lisa_mem_free()` | 释放 PSRAM 内存 |

## Bounce Buffer 说明

### 工作原理

`lisa_rgb_update_framebuffer()` 在 Bounce Buffer 模式下的工作流程：

1. **确定目标缓冲区**：自动选择当前非显示缓冲区作为拷贝目标
2. **DMA 分块传输**：将应用帧缓冲区通过 DMA 分块拷贝到目标缓冲区
3. **标记待切换**：设置 `swap_pending` 标志，等待下一帧开始时切换
4. **中断自动切换**：DMA 传输完成中断自动交换 `display_idx` 和 `write_idx`

### 双缓冲机制

- **fb_buf[0]** 和 **fb_buf[1]**：两个独立的完整帧缓冲区（PSRAM）
- **display_idx**：当前正在扫描输出到屏幕的缓冲区索引
- **write_idx**：应用层可以写入的缓冲区索引（等于 `!display_idx`）
- **swap_pending**：标记是否有待切换的新帧数据

### Ping-Pong DMA

- **bb_buf[0]** 和 **bb_buf[1]**：两个 Bounce Buffer（内部 SRAM）
- **bb_idx**：当前 DMA 正在使用的 Bounce Buffer 索引
- 硬件自动在两个 Bounce Buffer 间切换，一个 DMA 传输时另一个 RGB 输出

### 返回值

- **成功时返回 0** (`LISA_DEVICE_OK`)
- **失败时返回负数错误码**：
  - `-EINVAL`：无效参数（设备为空、数据为空、长度错误）
  - `-EBUSY`：设备繁忙（上一次传输尚未完成）

## 关键代码

### 帧缓冲区分配

```c
/* 从 PSRAM 分配帧缓冲区（RGB565，每像素 2 字节） */
#define FRAME_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT * 2)
uint16_t *framebuffer = (uint16_t *)lisa_mem_alloc(FRAME_SIZE);
```

### 绘制彩色图案

```c
/* 填充纯色 */
static void fill_color(uint16_t *fb, uint16_t color)
{
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
        fb[i] = color;
    }
}

/* RGB565 颜色定义 */
#define COLOR_RED     0xF800  // R:5位(31), G:6位(0), B:5位(0)
#define COLOR_GREEN   0x07E0  // R:5位(0), G:6位(63), B:5位(0)
#define COLOR_BLUE    0x001F  // R:5位(0), G:6位(0), B:5位(31)
```

### 更新显示

```c
/* 拷贝到非显示缓冲区并标记待切换 */
int ret = lisa_rgb_update_framebuffer(rgb, framebuffer, FRAME_SIZE);
if (ret != LISA_DEVICE_OK) {
    LOGE("Failed to update framebuffer: %d", ret);
}

/* DMA 中断会在下一帧传输完成时自动切换显示缓冲区 */
/* 应用层无需手动管理缓冲区切换 */
```

## 配置说明

### PSRAM 配置

示例需要足够的 PSRAM 用于分配帧缓冲区：

- **prj.conf 配置**：`CONFIG_PSRAM_HEAP_SIZE=0x500000` (5 MB)
- **实际需求**：480x480x2 = 460800 字节 ≈ 450 KB（单帧）
- **建议配置**：至少 1 MB 以支持双帧缓冲和其他 PSRAM 分配

### Bounce Buffer 配置

Bounce Buffer 大小由设备配置决定，典型配置：

```c
lisa_rgb_config_t config = {
    .bounce_buffer_size = 19200,  // 像素数量（如 240x80 = 19200）
    // 实际字节数 = bounce_buffer_size * bpp / 8
};
```

**注意**：`bounce_buffer_size` 表示像素数量，不是字节数！

### RGB565 格式

- **R 分量**：5 位 (位 15-11)，值范围 0-31
- **G 分量**：6 位 (位 10-5)，值范围 0-63
- **B 分量**：5 位 (位 4-0)，值范围 0-31
- **合成公式**：`color = (r << 11) | (g << 5) | b`

## 注意事项

1. **设备对象区分**：应使用 RGB 控制器设备 (`rgb0`)，而不是 RGB 总线设备 (`panel_bus_rgb`)
2. **内存对齐**：帧缓冲区内存需要满足 DMA 对齐要求（通常 32 字节对齐）
3. **缓冲区生命周期**：在更新完成前不要释放或修改帧缓冲区内容
4. **并发控制**：如需连续更新帧，应等待上一次传输完成后再发起下一次更新
5. **PSRAM 配置**：确保 `CONFIG_PSRAM_HEAP_SIZE` 足够大，否则 `lisa_mem_alloc()` 会失败
6. **RGB 初始化**：示例假设 RGB 设备已在 Display Bus attach 时初始化，如直接使用 RGB API 需先调用 `lisa_rgb_setup()` 和 `lisa_rgb_start()`
7. **颜色格式**：确认屏幕配置为 RGB565 格式，其他格式需调整像素数据结构
8. **性能优化**：对于高帧率应用，可减小 Bounce Buffer 以降低单次 DMA 延迟，但会增加 DMA 中断频率
