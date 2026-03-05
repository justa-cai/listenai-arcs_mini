# DMA2D 示例

本示例演示如何使用 DMA2D 实现 YUV444 到 RGB888 的图像格式转换功能。

## 📖 示例说明

### 功能演示

本示例演示以下功能：

- **图像格式转换**：将 YUV444 格式图像转换为 RGB888 格式
- **DMA2D 配置**：配置 DMA2D 通道、传输模式、地址模式等参数
- **图像参数配置**：设置图像宽度、高度、输入输出格式
- **转换验证**：验证转换后的 RGB 数据是否正确
- **事件回调机制**：注册并处理 DMA2D 传输完成事件

### 硬件要求

- ARCS 系列开发板
- USB 转串口工具（用于查看日志输出）

## 🚀 快速开始

### 1. 构建项目

在示例目录下执行构建脚本：

```bash
cd samples/drivers/dma2d
./build.sh
```

或者在 SDK 根目录执行：

```bash
./build.sh -S samples/drivers/dma2d -C
```

构建成功后，会在 `build` 目录下生成 `arcs.bin` 文件。

```shell
cskburn -s /dev/ttyUSB0 -b 3000000 0x0 build/arcs.bin -C arcs
```

参数说明：
- `-s /dev/ttyUSB0`：串口设备路径（根据实际情况修改）
- `-b 3000000`：烧录波特率
- `0x0`：烧录起始地址
- `build/arcs.bin`：烧录固件路径

> 💡 详细烧录步骤请参考 {ref}`快速开始 - 烧录运行 <flashing>`。

### 3. 查看输出

烧录完成后，复位开发板，通过串口工具（如 minicom）查看日志输出。

## 🔧 配置说明

### 项目配置

本示例在 `prj.conf` 中配置：

```conf
CONFIG_MEM_CONFIG=y
```

### 图像参数配置

| 参数 | 值 | 说明 |
|------|-----|------|
| `DMA2D_IMAGE_WIDTH_SIZE` | 64 | 图像宽度（像素） |
| `DMA2D_IMAGE_HEIGHT_SIZE` | 64 | 图像高度（像素） |
| `DEF_Y` | 0xAA | Y 分量（亮度）默认值 |
| `DEF_U` | 0x88 | U 分量（色度）默认值 |
| `DEF_V` | 0x33 | V 分量（色度）默认值 |

### DMA2D 参数配置

```c
csk_dma2d_init_t dma2d_para = {
    .dma_ch = dma_2d_ch9,                  // DMA2D 通道 9
    .burst_len = dma2d_burst_len_8spl,     // 突发长度：8 采样
    .src_mode = address_mode_normal,       // 源地址模式：正常
    .dst_mode = address_mode_normal,       // 目标地址模式：正常
    .sample_unit = dma2d_sample_unit_word, // 采样单元：字
    .tfr_mode = tfr_mode_m2m,              // 传输模式：内存到内存
    .src_inc_mode = inc_mode_increase,     // 源地址递增模式
    .dst_inc_mode = inc_mode_increase,     // 目标地址递增模式
    .prio_lvl = prio_mode_vhigh,           // 优先级：非常高
    .rd_done_ack = read_done_ack_enable,   // 读完成应答：使能
    .handshake = hs_none,                  // 握手：无
};
```

### 图像格式配置

```c
csk_dma_2d_image_cfg_t dma2d_img_cfg = {
    .img_input_format = csk_image_format_yuv444,                      // 输入：YUV444
    .img_height = DMA2D_IMAGE_HEIGHT_SIZE,                            // 图像高度
    .img_width = DMA2D_IMAGE_WIDTH_SIZE,                              // 图像宽度
    .img_output_fromat_transfer = csk_image_format_transfer_yuv444_xrgb, // 转换：YUV444->XRGB
    .img_rgb888_format = csk_image_rgb888_format,                     // 输出：RGB888
};
```

## 📋 代码解析

### 关键代码段

#### 1. DMA2D 初始化

```c
void DMA2D_Init(void)
{
    // 定义 DMA2D 参数结构
    csk_dma2d_init_t dma2d_para = { /* ... */ };
    
    // 定义图像配置结构
    csk_dma_2d_image_cfg_t dma2d_img_cfg = { /* ... */ };
    
    // 初始化 DMA2D
    DMA2D_Initialize();
    
    // 配置 DMA2D 并注册回调
    DMA2D_Config(&dma2d_para, dma2d_image_callback, NULL);
    
    // 配置图像转换参数
    DMA2D_Image_Config_Extend(dma_2d_ch9, &dma2d_img_cfg);
}
```

#### 2. YUV444 到 RGB 转换宏

```c
#define IMAGE_YUV444_COMPONENT(y, u, v) (((0x00) << 24) | (v << 16) | (u << 8) | y)

#define clip(value) ((value) < 0 ? 0 : ((value) > 255 ? 255 : (value)))

#define YUV444ToRGB(Y, Cb, Cr)                                                    \
    ((clip((256 * Y + 359 * (Cr - 128)) >> 8)) |                                 \
     (clip((256 * Y - 183 * (Cr - 128) - 88 * (Cb - 128)) >> 8) << 8) |         \
     (clip((256 * Y + 444 * (Cb - 128)) >> 8) << 16))
```

#### 3. DMA2D 事件回调

```c
static void dma2d_image_callback(uint32_t event, void *workspace)
{
    dma2d_image_event = event;
}
```

#### 4. 图像格式转换

```c
static void DMA2D_Image_YUV444_to_RGB888(void)
{
    LOGI("[DMA2D][IMAGE] YUV444 Transfer to RGB888 with DMA2D function, "
         "the image size[%d, %d], %s",
         DMA2D_IMAGE_WIDTH_SIZE, DMA2D_IMAGE_HEIGHT_SIZE, __FUNCTION__);
    
    dma2d_image_event = 0;
    
    // 清空缓冲区
    memset(golden_yuv444_format_image, 0, 
           sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 4 / 4);
    memset(golden_xrgb_format_image, 0, 
           sizeof(uint32_t) * DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 4);
    
    // 生成 YUV444 格式图像
    for (uint32_t i = 0; i < DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE; i++) {
        golden_yuv444_format_image[i] = IMAGE_YUV444_COMPONENT(DEF_Y, DEF_U, DEF_V);
    }
    
    // 启动 DMA2D 转换
    DMA2D_Start_Normal(dma_2d_ch9, golden_yuv444_format_image, golden_xrgb_format_image,
                       DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE);
    
    // 等待传输完成
    while (!dma2d_image_event);
}
```

#### 5. 转换结果验证

```c
// 计算期望的 RGB 值
volatile uint32_t rgb = YUV444ToRGB(DEF_Y, DEF_U, DEF_V);
LOGI("rgb_value = 0x%08x", rgb);

// 生成三种不同的字节序列（用于验证 RGB888 打包格式）
volatile uint32_t brgb = (rgb << 24) | rgb;
volatile uint32_t gbrg = (rgb << 16) | (rgb >> 8);
volatile uint32_t rgbr = (rgb << 8) | (rgb >> 16);

// 验证每个像素的 RGB 值
for (uint32_t i = 0; i < (DMA2D_IMAGE_WIDTH_SIZE * DMA2D_IMAGE_HEIGHT_SIZE * 3 / 4); i++) {
    if (i % 3 == 0) {
        if (golden_xrgb_format_image[i] != brgb) {
            ret = -1;
        }
    }
    else if (i % 3 == 1) {
        if (golden_xrgb_format_image[i] != gbrg) {
            ret = -2;
        }
    }
    else if (i % 3 == 2) {
        if (golden_xrgb_format_image[i] != rgbr) {
            ret = -3;
        }
    }
    
    // 错误检查
    if (ret != 0) {
        LOGI("[DMA2D][FORMAT] Index->%d YUV444->XRGB compare error\n", i);
        return;
    }
}

LOGI("YUV444 Transfer to RGB888 with DMA2D function Success!\r\n");
```

#### 6. 主函数

```c
int main(int argc, char **argv)
{
    LOGI("Hello, world! Dma2d");
    
    // 使能全局中断
    enable_GINT();
    
    // 初始化 DMA2D
    DMA2D_Init();
    
    // 执行 YUV444 到 RGB888 转换
    DMA2D_Image_YUV444_to_RGB888();
}
```

## 🔍 预期输出

程序运行后，串口将输出以下日志：

```
I/elog            [00:00:00.000 1 elog_async] EasyLogger V2.2.99 is initialize success.
I/Dma2d Sample    [00:00:00.001 1 main] Hello, world! Dma2d
I/Dma2d Sample    [00:00:00.001 1 main] [DMA2D][IMAGE] YUV444 Transfer to RGB888 with DMA2D function, the image size[64, 64], DMA2D_Image_YUV444_to_RGB888
I/Dma2d Sample    [00:00:00.001 1 main] rgb_value = 0x00b7de3e
I/Dma2d Sample    [00:00:00.001 1 main] YUV444 Transfer to RGB888 with DMA2D function Success!
```

输出说明：
- `EasyLogger V2.2.99 is initialize success`：日志系统初始化成功
- `Hello, world! Dma2d`：程序启动信息
- `[DMA2D][IMAGE]...`：开始 YUV444 到 RGB888 转换，图像尺寸 64×64
- `rgb_value = 0x00b7de3e`：转换后的 RGB 值
  - R = 0x3e（62）
  - G = 0xde（222）
  - B = 0xb7（183）
- `YUV444 Transfer to RGB888 with DMA2D function Success!`：转换成功

## ⚠️ 注意事项

1. **图像尺寸**：
   - 代码中定义图像尺寸为 64×64 像素


2. **YUV444 格式**：
   - 每个像素占用 4 字节（0x00VVUUYY）
   - Y：亮度分量
   - U、V：色度分量
   - 第 4 字节固定为 0x00

3. **RGB888 格式**：
   - 每个像素占用 3 字节（RGB）
   - 数据按照特定字节序打包到 32 位字中
   - 验证时需要考虑字节对齐和打包顺序

4. **YUV 到 RGB 转换公式**：
   ```
   R = Y + 1.402 × (Cr - 128)
   G = Y - 0.344 × (Cb - 128) - 0.714 × (Cr - 128)
   B = Y + 1.772 × (Cb - 128)
   ```
   代码中使用整数运算优化该公式

5. **DMA2D 通道**：
   - 使用通道 9（dma_2d_ch9）
   - 配置为内存到内存传输模式
   - 优先级设置为非常高

6. **缓冲区大小**：
   - YUV444 缓冲区：64 × 64 × 4 字节 = 16384 字节
   - RGB888 缓冲区：64 × 64 × 3 字节 = 12288 字节
   - 使用 32 位字数组存储

7. **验证方法**：
   - 使用 YUV444ToRGB 宏计算期望的 RGB 值
   - 考虑 RGB888 数据在 32 位字中的打包方式
   - 逐字验证转换结果

8. **日志系统**：
   - 使用 EasyLogger 日志系统
   - 日志标签：`Dma2d Sample`
   - 使用 `LOGI()` 宏输出信息级别日志
