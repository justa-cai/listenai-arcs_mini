# USB UVC 设备示例

## 功能说明

演示如何使用 USB UVC（USB Video Class）设备功能将摄像头采集的视频流通过 USB 接口传输到 PC，使设备作为标准 USB 摄像头被识别和使用。

本示例基于 TinyUSB 协议栈实现 UVC 设备功能，支持将 LISA Camera 采集的 图片格式为YUV422(YCbYCr422) ，图片大小为640*480的视频流实时传输到PC端，可被PC标准视频应用程序（如 ffmpeg、VLC或potplayer等）直接使用。

## 硬件连接

### 开发板
- Arcs-EVB开发板

### 摄像头连接

- **摄像头模组**: GC0328 DVP 接口摄像头
- **接口类型**: DVP（Digital Video Port）
- **I2C 配置**: I2C0 用于摄像头配置

### USB 连接

- **连接方式**: Arcs-EVB开发板的USB_ARCS的USB口通过USB线缆连接到PC

## 示例内容

1. 初始化 LISA Camera 驱动，配置摄像头分辨率和像素格式
2. 初始化 USB UVC 设备，配置 USB 描述符和 UVC 类参数
3. 注册摄像头帧回调函数，接收采集的视频帧
4. 启动摄像头采集，通过 UVC 接口将视频流传输到 PC
5. PC 端识别为标准 USB 摄像头设备

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 烧录
```bash
# 烧录固件(如下为linux系统烧录),烧录地址：0x30000000
cskburn -s /dev/ttyACM0 -b 2000000 -C arcs 0x0 ./build/arcs.bin
```

## 预期输出

**设备端日志:**
```
********Arcs SDK 0.1.0 @ v0.0.1-779-g94ba42f861c1********
Running on hart-id: 1
I/elog            [1034:42:44.160 1 elog_async] EasyLogger V2.2.99 is initialize success.
I/lisa_i2c_arcs   [1034:42:44.161 1 elog_async] I2C0 initialized successfully
I/lisa_dvp        [1034:42:44.161 1 elog_async] DVP driver initialized
I/main            [1034:42:44.163 1 main] === Camera UVC Streaming Demo
I/main            [1034:42:44.163 1 main] Resolution: 640x480 @ 15 fps
I/video_camera    [1034:42:44.163 1 main] camera device ready
I/video_camera    [1034:42:44.163 1 main] Setting up camera...
I/lisa_camera     [1034:42:44.165 1 main] PWDN pin initialized (pin=7)
I/lisa_camera     [1034:42:44.165 1 main] Enabling XCLK output: 12000000 Hz
I/lisa_i2c_arcs   [1034:42:44.167 1 main] I2C configured: speed=400000 Hz, mode=master
I/lisa_camera     [1034:42:44.169 1 main] Sensor detected: PID=0x009D, addr=0x21
I/lisa_camera     [1034:42:44.655 1 main] Detected sensor frame size: 640x480
I/lisa_camera     [1034:42:44.657 1 main] Detected sensor pixel format: 0
I/lisa_camera     [1034:42:44.657 1 main] Frame buffers initialized: count=5, size=614400
I/lisa_camera     [1034:42:44.657 1 main] Camera setup completed
I/video_camera    [1034:42:44.657 1 main] Camera capabilities: max_width=640, max_height=480, supported_formats=0x00000005
I/lisa_camera     [1034:42:44.666 1 main] Frame buffers re-initialized: count=5, size=614400
I/lisa_camera     [1034:42:44.670 1 main] Frame buffers re-initialized: count=5, size=614400
I/lisa_dvp        [1034:42:44.670 1 main] DVP initialized successfully with GPDMA channel 2
I/lisa_dvp        [1034:42:44.670 1 main] DVP clockout enabled successfully, freq: 12000000 Hz
I/video_camera    [1034:42:44.671 1 camera_task] Capture task started
I/video_camera    [1034:42:44.671 1 main] Camera capture started with internal task
I/uvc_stream      [1034:42:44.671 1 main] TinyUSB UVC class ready

I/video_camera    [1034:42:44.673 1 main] Frame callback registered
I/lisa_dvp        [1034:42:44.673 1 main] DVP capture started
I/camera_bus_dvp  [1034:42:44.673 1 main] DVP Normal capture started
I/lisa_camera     [1034:42:44.673 1 main] Camera started
I/main            [1034:42:44.673 1 main] Camera capture started
I/camera_bus_dvp  [1034:42:44.863 1 isr] dvp_event_callback: event:1
I/camera_bus_dvp  [1034:42:44.950 1 isr] dvp_event_callback: event:1
I/camera_bus_dvp  [1034:42:45.037 1 isr] dvp_event_callback: event:1
I/camera_bus_dvp  [1034:42:45.123 1 isr] dvp_event_callback: event:1
I/camera_bus_dvp  [1034:42:45.210 1 isr] dvp_event_callback: event:1
I/uvc_stream      [1034:42:45.258 1 usbd] USB mounted
I/camera_bus_dvp  [1034:42:45.297 1 isr] dvp_event_callback: event:1
I/camera_bus_dvp  [1034:42:45.383 1 isr] dvp_event_callback: event:1
I/camera_bus_dvp  [1034:42:45.470 1 isr] dvp_event_callback: event:1
```

## PC 端测试方法

### Windows

> 正常情况下设备管理器中查看"照相机"或"图像设备"分类下面是否有一个UVC Control的设备，如果有则进行下面操作

```bash
# 打开设备查看图片数据
ffplay.exe -f dshow -pixel_format yuyv422 -video_size 640x480 -i video="UVC Control"

# 保存图片数据到文件
ffmpeg.exe -f dshow -video_size 640x480 -pixel_format yuyv422 -i video="UVC Control" -c:v rawvideo out_yuyv.avi
```

### Linux

```bash
# 查看video设备
➜ v4l2-ctl --list-devices  # 得到名字为TinyUSB的设备，实际设备为/dev/video2，如下所示
TinyUSB Device: UVC Control (usb-0000:00:14.0-3.3.1.3):
    /dev/video2
    /dev/video3
    /dev/media1

# 使用ffplay播放视频(具体设备由上面命令得出，此处假设设备为/dev/video2)
ffplay \
  -f v4l2 \
  -input_format yuyv422 \
  -video_size 640x480 \
  -i /dev/video2

# 保存YUYV422原始图片数据到文件(具体设备由上面命令得出，此处假设设备为/dev/video2)
ffmpeg \
  -f v4l2 \
  -input_format yuyv422 \
  -video_size 640x480 \
  -i /dev/video2 \
  -c:v rawvideo \
  out_yuyv_640x480.avi
```

## 核心 API

### Camera API

| API | 说明 |
|-----|------|
| `video_camera_init()` | 初始化摄像头，配置分辨率和格式 |
| `video_camera_register_callback()` | 注册帧回调函数 |
| `video_camera_start_capture()` | 启动摄像头采集 |
| `video_camera_stop_capture()` | 停止摄像头采集 |

### UVC API

| API | 说明 |
|-----|------|
| `uvc_stream_init()` | 初始化 UVC 设备 |
| `uvc_stream_send_data()` | 发送视频帧数据到 UVC |

## 关键代码

### 摄像头初始化

```c
/* 配置摄像头参数 */
camera_config_t config = {
    .width = CONFIG_IMAGE_WIDTH,
    .height = CONFIG_IMAGE_HEIGHT,
    .format = LISA_CAMERA_PIXFMT_YUV422,
};
ret = video_camera_init(&config);
```

### 帧回调处理

```c
/* 摄像头帧回调函数 */
static void on_frame_captured(camera_frame_t *frame, void *user_data)
{
    /* 将摄像头帧数据发送到 UVC */
    uvc_stream_send_data((video_info_t *)frame);
}

/* 注册回调 */
video_camera_register_callback(on_frame_captured, NULL);
```

### UVC 数据传输

```c
/* 初始化 UVC */
ret = uvc_stream_init();

/* 启动采集 */
ret = video_camera_start_capture();
```

## 注意事项

1. **摄像头型号**: 目前仅测试GC0328
2. **内存需求**: UVC 传输YUYV422原始图片需要大量内存缓冲，确保 PSRAM 配置足够
3. **USB 连接**: 需确保 USB 线缆支持USB2.0的480Mbps的数据传输速度
4. **帧率限制**: 实际帧率受MCU处理性能限制，低于配置值，实测FPS=11.5(在linux平台测试)
5. **像素格式**: 当前示例仅使用 YUV422(YCbYCr422) 格式，帧大小为640*480，用户可以扩展其他帧格式(如mpeg)和帧大小(如320*240)
6. **驱动支持**: Windows 10/11、Linux均原生支持 UVC 设备，无需额外驱动
7. **回调线程**: 帧回调在摄像头驱动线程中执行，避免在回调中执行耗时操作
