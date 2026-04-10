# USB Host Video（UVC 摄像头）示例

## 功能说明

演示如何使用 CherryUSB 在 ARCS 芯片上实现 USB Host Video（UVC）功能，支持同时连接最多 2 台 USB 摄像头，将采集到的视频画面实时分屏显示在 LCD 屏幕上。

本示例基于 CherryUSB 协议栈实现 UVC Host Bulk 传输，每台摄像头独立运行一个 FreeRTOS 采集任务，帧数据经 PSRAM 缓冲后通过 LVGL 异步渲染到 ST7789P3 屏幕。

### 特性

- **双摄同时支持**: 最多同时连接 2 台 UVC 摄像头，UVC摄像头格式支持 YUV2和MJPEG
- **格式和分辨率自动协商**: 支持自动协商摄像头格式和分辨率（优先选择第一个格式、第一个分辨率）
- **MJPEG 实时解码**: 对于MJPEG格式的摄像头，通过 TJpgDec（LVGL8 内置 Tiny JPEG 解码器）将 MJPEG 帧解码为 RGB565，按比例缩放后写入对应半屏
- **YUV422 实时解码**: 对于YUV2格式的摄像头，通过软件将 YUV422 帧解码为 RGB888，按比例缩放后写入对应半屏
- **分屏实时显示**: 左右分屏——CAM0 显示在左半屏（0~159 列），CAM1 显示在右半屏（160~319 列）；支持任意源分辨率等比缩放到半屏
- **安全热拔插**: 支持热插拔UVC摄像头

> 注意： MJPEG UVC摄像头暂未验证过
## 硬件连接

### 开发板
- ARCS EVB 开发板（Host 端）× 1

### 显示屏
- ST7789P3 LCD（物理分辨率 240×320，SPI 4-wire 接口）
- LVGL 旋转 270° 后逻辑分辨率为 **320×240**

### USB 连接（单摄像头）

将 1 台 UVC 摄像头直接插入 Host 开发板 `USB_ARCS` 口即可。

### USB 连接（双摄像头）

连接 2 台 UVC 摄像头需通过 **USB Hub** 扩展：

```
ARCS EVB (Host)
   └── USB_ARCS (Type-C)
         └── USB Hub
               ├── Port 1 → UVC 摄像头 0 (CAM0，显示在左半屏)
               └── Port 2 → UVC 摄像头 1 (CAM1，显示在右半屏)
```

**USB Hub 要求**：
- 支持 USB 2.0 High-speed（480 Mbps）
- 自供电（摄像头需要足够电流）或有独立电源输入

### USB Host 硬件修改（必须）

开发板 `USB_ARCS` Type-C 接口默认工作在 Device 模式，作为 Host 使用须完成以下硬件修改：

- **VBUS**：直接对 Type-C 接口供电 5V（不依赖 USB 协商）
- **CC1 / CC2**：分别接 22K 上拉电阻到 VBUS（Host 模式标识）

> **注意**：未做硬件修改时无法识别外接 USB 设备。


### 测试用 UVC 摄像头（使用 arcs-evb 模拟）

本示例已在以下测试配置下验证：

| 设备 | 角色 | 固件 |
|------|------|------|
| ARCS EVB #1 | USB Host + 显示 | 本示例（`cherryusb_video`） |
| ARCS EVB #2 | UVC Device (CAM0) | `samples/subsys/usb/device/uvc` |
| ARCS EVB #3（可选） | UVC Device (CAM1) | `samples/subsys/usb/device/uvc` |

UVC Device 固件通过 TinyUSB 模拟标准 UVC 设备，发布 **UNCOMPRESSED（YUY2）640×480 @ 15fps，Bulk 模式**。两台 Device 开发板通过 USB Hub 连接到 Host 开发板。

## 示例内容

1. USB PHY 初始化（配置为 Host 模式，16-bit 数据总线）
2. 初始化 ST7789P3 LCD 和 LVGL 显示框架（逻辑分辨率 320×240）
3. 等待 USB 摄像头热插拔（支持 1~2 台设备同时工作，经由 USB Hub）
4. 设备枚举完成后，解析描述符并打印摄像头支持的格式和分辨率信息
5. 动态选择最佳格式：优先找首选格式（MJPEG）的第一帧；否则退回到任意可用格式/分辨率
6. 通过 UVC Probe/Commit 协议协商格式，确认后开始 Bulk 流传输
7. 格式协商成功后，按实际分辨率从 PSRAM 动态分配帧缓冲区（`width × height × 2 B`），启动独立采集任务
8. 持续接收视频帧并通过异步 workqueue 更新显示：
   - UNCOMPRESSED（YUV422）：YUYV → RGB565 缩放转换，按比例写入对应半屏
   - MJPEG：TJpgDec 解码为 RGB888，按比例缩放写入对应半屏（需 `CONFIG_LV_USE_SJPG=1`）
9. 每 30 帧打印一次帧序号、帧大小和累计接收字节数
10. 摄像头拔出时安全停止视频流，释放 PSRAM 资源，等待下次插入

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 烧录

```{eval-rst}
.. include:: /sample_flash.rst
```

如需使用 ARCS EVB 作为 UVC 摄像头，同样烧录 Device 固件：

```bash
# 构建 UVC Device 固件
./build.sh -S samples/subsys/usb/device/uvc -DBOARD=arcs_evb

# 烧录到第二台开发板
cskburn -s /dev/ttyACM1 -b 3000000 -C arcs 0x0 ./build/arcs.bin
```

## 预期输出

**Host 端日志（插入一台 UVC UNCOMPRESSED 摄像头）：**

```
========================================
  CherryUSB Host Video Example
  Preferred format: MJPEG (auto-negotiated from device)
  Max devices: 2
========================================

[INFO] Initializing display...
[INFO] Initializing USB Host...
[INFO] USB Host initialized, please connect USB cameras

[I/usbh_hub] New full-speed device on Bus 0, Hub 1, Port 1 connected
[I/usbh_core] New device found,idVendor:xxxx,idProduct:xxxx,bcdDevice:0150
[I/usbh_core] Enumeration success, start loading class driver
[I/usbh_video] ============= Video device information ===================
[I/usbh_video] bNumFormats:1
[I/usbh_video]   FormatIndex:1
[I/usbh_video]   FormatType:uncompressed
[I/usbh_video]   bNumFrames:1
[I/usbh_video]       FrameIndex:1
[I/usbh_video]       wWidth: 640, wHeight: 480, fps: 15

[VIDEO0] Camera connected: video0
[VIDEO0] Opening device (format=UNCOMPRESSED 640x480)...
[VIDEO0] frame_buf allocated: 614400 B (640x480)
[VIDEO0] chunk_buf allocated: 512 B
[VIDEO0] Starting bulk streaming...
[VIDEO0] Streaming started
[VIDEO0] frame #30  size=614400 B  total=17973 KB
[VIDEO0] Running: 30 frames, 17973 KB total
...
```

**插入第二台摄像头（CAM1，需 USB Hub）时追加输出：**

```
[I/usbh_hub] New full-speed device on Bus 0, Hub 1, Port 2 connected
...
[VIDEO1] Camera connected: video1
[VIDEO1] Opening device (format=UNCOMPRESSED 640x480)...
[VIDEO1] Streaming started
...
```

**LCD 显示效果：**

- UNCOMPRESSED（YUV422）摄像头：对应半屏实时更新视频画面（YUYV→RGB565 缩放渲染，任意源分辨率 → 160×240）
- MJPEG 摄像头：TJpgDec 实时解码，对应半屏实时更新（标签显示 "CAM0(MJPEG)"）；解码失败时保持上一帧

## 核心 API

### USB Host 初始化

| API | 说明 |
|-----|------|
| `usbh_initialize()` | 初始化 USB Host 协议栈 |
| `usb_host_init()` | 初始化 USB Host PHY（由 SYS_INIT 自动调用） |

### Video Host API

| API | 说明 |
|-----|------|
| `usbh_video_list_info()` | 打印摄像头支持的格式和分辨率列表 |
| `usbh_video_open()` | 打开视频设备并通过 UVC Probe/Commit 协商格式和分辨率 |
| `usbh_video_close()` | 关闭视频设备 |
| `usbh_video_start_streaming()` | 启动 Bulk 视频流（注册帧回调，提交 URB） |
| `usbh_video_stop_streaming()` | 停止视频流并 kill 所有 URB |

### 用户回调（弱符号，需覆盖实现）

| 回调函数 | 说明 |
|---------|------|
| `usbh_video_run()` | 摄像头插入、枚举完成后由框架调用；用于启动采集任务 |
| `usbh_video_stop()` | 摄像头拔出时由框架调用；必须在返回前完成 URB kill |

## 关键代码

### 动态格式协商（不再硬编码分辨率）

```c
/*
 * 遍历设备格式表，优先找首选格式（MJPEG）的第一帧；
 * 若不支持则退回任意可用格式。
 * usbh_video_open() 传入从设备描述符读出的实际宽高，而非硬编码值。
 */
static bool find_best_format(struct usbh_video *vc, uint8_t pref_fmt,
                              uint8_t *out_fmt, uint16_t *out_w, uint16_t *out_h)
{
    for (uint8_t i = 0; i < vc->num_of_formats; i++) {
        if (vc->format[i].format_type == pref_fmt && vc->format[i].num_of_frames > 0) {
            *out_fmt = pref_fmt;
            *out_w   = vc->format[i].frame[0].wWidth;
            *out_h   = vc->format[i].frame[0].wHeight;
            return true;
        }
    }
    for (uint8_t i = 0; i < vc->num_of_formats; i++) {
        if (vc->format[i].num_of_frames > 0) {
            *out_fmt = vc->format[i].format_type;
            *out_w   = vc->format[i].frame[0].wWidth;
            *out_h   = vc->format[i].frame[0].wHeight;
            return true;
        }
    }
    return false;
}

/* 在采集任务中使用 */
uint8_t actual_fmt; uint16_t actual_w, actual_h;
if (!find_best_format(video_class, VIDEO_FORMAT_PREF, &actual_fmt, &actual_w, &actual_h)) {
    goto out_no_stream;
}
ret = usbh_video_open(video_class, actual_fmt, actual_w, actual_h, 0);
ctx->width  = actual_w;   /* 保存协商结果，帧回调中传给显示模块 */
ctx->height = actual_h;
```

### 每设备上下文与设备插入回调

```c
struct video_dev_ctx {
    struct usbh_video *video_class;
    uint8_t           *frame_buf;    /* PSRAM 帧缓冲区（≈600KB） */
    uint8_t           *chunk_buf;    /* PSRAM DMA chunk 缓冲区 */
    volatile bool      running;
    volatile bool      disconnected;
    TaskHandle_t       task_handle;
    uint32_t           frame_count;
    uint32_t           total_bytes;
    uint16_t           width;        /* 实际协商的视频宽度 */
    uint16_t           height;       /* 实际协商的视频高度 */
};

/* 摄像头插入回调：启动采集任务（帧缓冲区将在格式协商后按实际分辨率动态分配） */
void usbh_video_run(struct usbh_video *video_class)
{
    uint8_t idx = video_class->minor;
    struct video_dev_ctx *ctx = &g_devs[idx];

    ctx->video_class  = video_class;
    ctx->frame_buf    = NULL;   /* 格式协商后按实际分辨率动态分配 */
    ctx->running      = true;
    ctx->disconnected = false;

    char task_name[16];
    snprintf(task_name, sizeof(task_name), "video%u", (unsigned)idx);
    xTaskCreate(video_stream_task, task_name, 4096, ctx,
                CONFIG_USBHOST_PSC_PRIO + 1, NULL);
}
```

### 摄像头断开回调

```c
/*
 * 调用链：usbh_video_ctrl_disconnect()
 *           → usbh_video_stop()       ← 本函数
 *           → usbh_video_class_free() ← memset(video_class, 0)
 *
 * 若返回前未 kill URB，usbh_video_class_free 将 bulkin_urb.complete
 * 清零，MUSB 中断触发时访问 NULL 回调 → crash。
 */
void usbh_video_stop(struct usbh_video *video_class)
{
    usbh_video_stop_streaming(video_class);  /* 关键：先 kill URB */

    struct video_dev_ctx *ctx = &g_devs[video_class->minor];
    ctx->disconnected = true;
    if (ctx->task_handle) {
        xTaskNotifyGive(ctx->task_handle);
    }
    vTaskDelay(pdMS_TO_TICKS(50));
}
```

## 配置说明

### prj.conf 关键配置项

| 配置项 | 值 | 说明 |
|--------|----|------|
| `CONFIG_CHERRYUSB_HOST_VIDEO` | `y` | 启用 CherryUSB USB Video Host 驱动 |
| `CONFIG_CHERRYUSB_HOST_MUSB_LISA` | `y` | 启用 MUSB 控制器（ARCS 平台） |
| `CONFIG_PSRAM_HEAP` | `y` | 启用 PSRAM 动态堆（帧缓冲区来源） |
| `CONFIG_PSRAM_HEAP_SIZE` | `0x500000` | PSRAM 堆大小（5MB，支持双摄各 ≈600KB 帧缓冲） |
| `CONFIG_LISA_DISPLAY_PANEL_ST7789P3` | `y` | 启用 ST7789P3 LCD 驱动 |
| `CONFIG_LV_DRIVER_ROTATE_270` | `y` | LVGL 旋转 270°（物理 240×320 → 逻辑 320×240） |
| `CONFIG_LV_USE_GPU_CSK_DMA2D` | `y` | 启用 DMA2D 硬件加速显示 |
| `CONFIG_LV_USE_SJPG` | `1` | 启用 TJpgDec（MJPEG 实时解码显示） |
| `CONFIG_WORK_QUEUE` | `y` | 启用 workqueue（显示更新异步派发） |

### usb_config.h 关键配置项

| 配置项 | 值 | 说明 |
|--------|----|------|
| `CONFIG_USBHOST_MAX_VIDEO_CLASS` | `2` | 最多同时支持 2 台 UVC 摄像头 |
| `CONFIG_USB_ALIGN_SIZE` | `32` | DMA 缓冲区对齐字节数（dcache 开启时须 32B） |
| `CONFIG_USBH_VIDEO_BULK_CHUNK_SIZE` | `512` | Bulk 单次 URB 接收长度（默认值，`dwMaxPayloadTransferSize=0` 时使用） |

### main.c 视频参数

```c
/* 首选视频格式（优先尝试；若不支持则自动选择其他可用格式） */
#define VIDEO_FORMAT_PREF    USBH_VIDEO_FORMAT_MJPEG
```

帧缓冲区大小在格式协商完成后动态计算：`frame_bufsize = actual_w * actual_h * 2`，无需手动调整。

## 注意事项

1. **硬件修改必须**：开发板 USB 口默认为 Device 模式，作为 Host 使用必须完成 VBUS 5V 供电和 CC1/CC2 上拉电阻修改，否则无法识别外接设备。

2. **双摄需 USB Hub**：ARCS EVB 仅有一个 USB 口，同时接入 2 台摄像头必须经过 USB Hub。Hub 建议使用自供电款，以保证摄像头有足够的工作电流。

3. **仅支持 Bulk 传输**：ARCS MUSB 控制器当前未实现 ISO 传输，示例会检测 `video_class->is_bulk`，若摄像头要求 ISO 模式则自动中止并提示。请使用支持 Bulk 传输的 UVC 摄像头。

4. **MJPEG 解码显示（未经硬件验证）**：MJPEG 解码路径（TJpgDec，`CONFIG_LV_USE_SJPG=1`）已实现但**尚未在实际硬件上验证**，可能存在未知问题。若遇到异常，可将 `VIDEO_FORMAT_PREF` 改为 `USBH_VIDEO_FORMAT_UNCOMPRESSED` 退回经过验证的 YUV422 路径。TJpgDec 每帧从 PSRAM 申请 4KB 工作内存池；解码失败时 LCD 保持上一帧。

5. **内存分配**：帧缓冲区在格式协商后按实际分辨率动态分配（`width × height × 2 B`，如 640×480 ≈ 600KB），chunk 缓冲区同样来自 PSRAM。建议 `CONFIG_PSRAM_HEAP_SIZE ≥ 5MB`（支持双摄各 ≈ 600KB + MJPEG 解码各 4KB 工作内存池）。

6. **设备兼容性**：已测试 ARCS EVB UVC Device 固件（UNCOMPRESSED YUY2 640×480 Bulk），其他设备理论支持但未实测。
