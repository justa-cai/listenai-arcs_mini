# 人脸识别演示

## 功能说明

本演示是一个完整的人脸识别应用，集成了摄像头图像采集、人脸检测、活体检测、特征提取、人脸识别和可视化显示等功能。用户可通过按键来体验人脸注册和人脸识别的功能。

## 系统架构

```
┌─────────────┐
│  摄像头模块   │ GC0328 采集图像 (320x240)
└─────────────┘
       │
       ├──────────────────────────────────┐
       │                                  │
       │ (实时图像)                        │ (部分图像)
       ▼                                  ▼
┌─────────────┐                  ┌────────────────────────┐
│  显示模块    │                  │ 人脸识别算法模块 (AP)     │
│ (ST7789 LCD) │◄────────────────│ 检测/对齐/活体/特征/对比  │
└─────────────┘   (人脸结果)      └────────────────────────┘
       ▲                                  ▲
       │                                  │
       │                                  │ (控制命令)
       │ (显示控制)                        │
       │                                  │
┌─────────────┐                           │
│  按键模块    │───────────────────────────┘
│ (ADC 检测)  │  K1:人脸注册
└─────────────┘
```

## 主要功能

#### 1. 实时图像采集
- GC0328 摄像头通过 DVP 接口连续采集 320x240 分辨率图像
- 支持输出 YUYV422彩色模式、YUYV422灰度模式(其中Cb=0, Cr=0)、RGB565模式，最大支持640*480分辨率
- 输出的每帧图片实时发给显示屏显示，同时部分图片也通过 IPC 自动发送到 AP 核进行算法处理

#### 2. 人脸识别算法
人脸识别算法在 AP 核上运行，对每帧图像执行完整的人脸识别流程：
- **人脸检测**：检测图像中的人脸位置和大小，输出人脸矩形框坐标和置信度得分
- **人脸对齐**：定位 68 个人脸关键点（眼睛、鼻子、嘴巴、轮廓等），计算头部姿态角（yaw/pitch/roll）
- **活体检测**：判断人脸是否为真人，输出活体检测结果（0、1）和得分，防止照片、视频攻击
- **特征提取**：提取 384 维人脸特征向量，用于唯一标识一个人脸
- **特征比对**：将提取的特征与已注册人脸进行相似度比对，输出比对得分（ 阈值由用户决定，超过阈值则认为是同一人）

目前暂时支持注册最多 10 个人脸特征到内存库，可配置活体检测阈值以适应不同应用场景。

#### 3. 交互式操作
- **K1 键**：抓取当前检测到的人脸特征（需先检测到人脸），如果符合条件（比如活体检测通过、头部姿势正常），就把它存储下来，并且加载到算法对比库中。如果不满足条件，需要用户重新按K1键抓取

#### 4. 可视化显示
- LCD 显示字符来提示用户怎么操作
- LCD 实时显示摄像头采集的画面
- LCD 实时显示人脸检测算法的处理结果，包括检测到的人脸个数、人脸框、活体状态，活体得分等信息；
- 如果已注册有人脸，LCD也会显示检测到人脸的比对得分，超过对比阈值则人脸框显示为绿色（并且显示人脸ID），否则显示为红色
- LCD 显示K1按键的人脸注册个数

## 硬件连接
- **开发板**： ARCS-EVB开发板
- **摄像头**: GC0328 摄像头模块，通过 DVP 接口连接
- **显示屏**: ST7789 LCD 显示屏，通过 SPI 接口连接
- **按键**: 通过 ADC 引脚检测，支持 2 个按键（目前仅K1按键可用）
  - **K1**: 注册人脸特征
  - **K2**: 人脸识别（与注册人脸比对）（需要CONFIG_ONLY_FACE_REGISTER=n才能生效）
- **调试串口**: 
  - CP 核 UART0 (PA3)，波特率 921600
  - AP 核 UART1 (PA21)，波特率 921600

## 使用说明

1. 上电后系统自动初始化，LCD 显示摄像头实时画面
2. 检测到人脸时，屏幕显示绿色矩形框
3. **K1 键**: 注册当前人脸特征（最多 10 个）
4. **K2 键**: 触发人脸识别，显示比对得分
5. **K3 键**: 切换摄像头彩色/灰度模式

## 编译运行

```bash
./build.sh -C -DBOARD=arcs_evb
```

## 烧录固件

### 1. 烧录 AP 核固件（Boot Core）

```bash
cskburn -s /dev/ttyACM0 -b 3000000 -C arcs 0x00 ./res/ap.bin
```

### 2. 烧录算法资源到 Flash

```bash
# 人脸检测模型
cskburn -s /dev/ttyACM0 -b 3000000 -C arcs 0x100000 ./res/algo/face_detect_thinker.bin

# 人脸对齐模型
cskburn -s /dev/ttyACM0 -b 3000000 -C arcs 0x170000 ./res/algo/face_align_thinker.bin

# 活体检测模型
cskburn -s /dev/ttyACM0 -b 3000000 -C arcs 0x300000 ./res/algo/face_nir_thinker_lineart_split.bin

# 人脸特征提取模型
cskburn -s /dev/ttyACM0 -b 3000000 -C arcs 0x410000 ./res/algo/face_verify_thinker.bin
```

### 3. 烧录 CP 核固件

```bash
cskburn -s /dev/ttyACM0 -b 3000000 -C arcs 0x800000 ./build/arcs.bin
```

## 预期输出

**CP 核串口输出:**

```
********Arcs SDK 0.1.1 @ v0.0.1-860-gd85bab268324********
Running on hart-id: 1
I/lisa_adc_arcs   [00:00:00.035 1 none] ADC0 initialized successfully
I/lisa_i2c_arcs   [00:00:00.037 1 none] I2C0 initialized successfully
I/lisa_pwm_arcs   [00:00:00.037 1 none] PWM0 initialized successfully (max_channels=8)
I/lisa_touch_cst328 [00:00:00.039 1 none] CST328 touch device initialized successfully
I/lisa_dvp        [00:00:00.040 1 none] DVP driver initialized
CP=======! Hard ID: 1
I/main            [00:00:00.042 1 main] ic_message_init done!
I/acomp_ipc       [00:00:02.044 1 rpc_client] [0]acomp remote dev index 1 name acomp.fd
I/acomp_fd        [00:00:02.045 1 main] acomp fd init enter
I/acomp_fd        [00:00:02.046 1 main] acomp fd dev index 1,name:acomp.fd
I/acomp_fd        [00:00:02.047 1 main] acomp fd dev index 1,name:acomp.fd
I/acomp_fd        [00:00:02.049 1 main] acomp fd init exit
I/acomp_fd        [00:00:02.050 1 main] acomp fd prepare enter
I/acomp_fd        [00:00:02.051 1 main] acomp fd prepare:0x2895ea60, size:128
I/acomp_fd        [00:00:02.053 1 main] acomp fd prepare exit
I/acomp_fd        [00:00:02.054 1 main] acomp fd start enter
I/acomp_fd        [00:00:02.254 1 main] acomp fd start exit
I/acomp_fd        [00:00:02.255 1 main] acomp fd add callback enter
I/acomp_fd        [00:00:02.256 1 main] acomp fd add callback exit
I/acomp_fd        [00:00:02.257 1 main] acomp_fd_live_detect_mode_set enter
I/acomp_fd        [00:00:02.259 1 main] acomp_fd_live_detect_mode_set exit
INF:[acomp_stream_channel_create]Acomp stream channel 0x289fcbb4,0x2001b450,'stream.fd_image' created successfully ,vring phy addr=0x28966aa0, num_descs=4, buffer_size=153632, direction=M2R, role=Master

I/acomp_fd        [00:00:02.272 1 main] acomp_fd_stream_ch_enable chn(stream.fd_image) index(0),desc(0x2892e280)
I/lvgl_heap       [00:00:02.274 1 main] LVGL heap initialized: 1048576 bytes at 0x0x28825800
spi_set_bus_speed: sclk_div = 0 (sclk: 50000000), cs2sclk = 1

I/panel.st7789p3  [00:00:02.536 1 main] ST7789P3 initialized
I/lisa_i2c_arcs   [00:00:02.540 1 main] I2C configured: speed=100000 Hz, mode=master
I/lisa_touch_cst328 [00:00:02.541 1 main] I2C auto-configured (speed: 100000 Hz)
I/lisa_touch_cst328 [00:00:02.762 1 main] CST328 hardware reset completed
I/lisa_touch_cst328 [00:00:02.763 1 main] CST328 interrupt pin configured
I/lisa_touch_cst328 [00:00:02.764 1 main] CST328 touch bus attached successfully
I/lisa_touch_cst328 [00:00:02.765 1 main] CST328 read task created with priority 2
I/lisa_touch_cst328 [00:00:02.766 1 main] CST328 interrupt mode enabled
I/lisa_touch_cst328 [00:00:02.767 1 main] CST328 touch device enabled
I/lvgl8_demo_widgets [00:00:02.774 1 task_ui] LVGL screen loaded and started

I/sample          [00:00:02.788 1 main] === LISA ADC read example ===

I/sample          [00:00:02.789 1 main] adc0 device ready

I/lisa_adc_arcs   [00:00:02.790 1 main] Channel 4 configured: ref=1, 10-bit, vref_sel=0, vin_buf=1
I/sample          [00:00:02.791 1 main] Channel 4 configured: 3.6V reference, 10-bit resolution

I/btn             [00:00:02.793 1 btn] lisa_btn_task enter
I/btn             [00:00:02.794 1 main] lisa btn init done
I/video_camera    [00:00:02.795 1 main] camera config: w:320, h:240, format:2
I/video_camera    [00:00:02.796 1 main] camera device ready
I/video_camera    [00:00:02.797 1 main] Setting up camera...
I/lisa_camera     [00:00:02.799 1 main] PWDN pin initialized (pin=7)
I/lisa_camera     [00:00:02.800 1 main] Enabling XCLK output: 8000000 Hz
I/lisa_i2c_arcs   [00:00:02.802 1 main] I2C configured: speed=400000 Hz, mode=master
I/lisa_camera     [00:00:02.805 1 main] Sensor detected: PID=0x009D, addr=0x21
I/lisa_camera     [00:00:03.308 1 main] Detected sensor frame size: 640x480
I/lisa_camera     [00:00:03.311 1 main] Detected sensor pixel format: 0
I/lisa_camera     [00:00:03.312 1 main] Frame buffers initialized: count=4, size=614400
I/lisa_camera     [00:00:03.313 1 main] Camera setup completed
I/video_camera    [00:00:03.314 1 main] Camera capabilities: max_width=640, max_height=480, supported_formats=0x00000005
I/video_camera    [00:00:03.316 1 main] crop:x:160, y:120, w:320, h:240
I/lisa_camera     [00:00:03.325 1 main] Frame buffers re-initialized: count=4, size=153600
I/lisa_camera     [00:00:03.390 1 main] Frame buffers re-initialized: count=4, size=153600
I/video_camera    [00:00:03.471 1 main] camera_set_gray on success

I/lisa_dvp        [00:00:03.473 1 main] DVP initialized successfully with GPDMA channel 2
I/lisa_dvp        [00:00:03.474 1 main] DVP clockout enabled successfully, freq: 8000000 Hz
I/video_camera    [00:00:03.476 1 camera_task] Capture task started
I/video_camera    [00:00:03.477 1 main] Camera capture started with internal task
I/video_camera    [00:00:03.479 1 main] Frame callback registered
I/lisa_dvp        [00:00:03.480 1 main] DVP capture started
I/camera_bus_dvp  [00:00:03.481 1 main] DVP Normal capture started
I/lisa_camera     [00:00:03.482 1 main] Camera started
I/camera_bus_dvp  [00:00:03.754 1 isr] dvp_event_callback: event:1
I/camera_bus_dvp  [00:00:03.881 1 isr] dvp_event_callback: event:1
I/camera_bus_dvp  [00:00:04.008 1 isr] dvp_event_callback: event:1
I/main            [00:00:04.102 1 rpc_client] fd result:2732,
I/camera_bus_dvp  [00:00:04.135 1 isr] dvp_event_callback: event:1
I/camera_bus_dvp  [00:00:04.261 1 isr] dvp_event_callback: event:1
I/main            [00:00:04.361 1 rpc_client] fd result:2732,

```

**LCD 屏幕显示:**
- 实时显示摄像头采集的画面
- 实时显示检测到的人脸的黄色方框
- 屏幕左上方会实时显示检测到的人脸个数、活体状态和活体得分
- 按下K1键时，如果检测到人脸符合注册条件，会注册人脸特征，并在屏幕左下角显示"Registered: "个数会增加
- 如果已注册有人脸，LCD也会显示检测到人脸的比对得分，超过对比阈值则人脸框显示为绿色（并且显示人脸ID），否则显示为红色

## 核心 API

| API | 说明 |
|-----|------|
| `acomp_fd_init()` | 初始化人脸识别组件 |
| `acomp_fd_prepare()` | 准备人脸识别算法资源 |
| `acomp_fd_start()` | 启动人脸识别算法 |
| `acomp_fd_add_callback()` | 注册人脸识别结果回调函数 |
| `acomp_fd_live_detect_mode_set()` | 配置活体检测模式 |
| `acomp_fd_stream_ch_enable()` | 使能图像输入数据流通道 |
| `acomp_fd_stream_tx_buffer_alloc()` | 分配发送缓冲区 |
| `acomp_fd_stream_tx_buffer_submit()` | 提交图像数据到算法 |
| `acomp_fd_features_load()` | 导入人脸特征到注册库 |

## 关键代码

```c
/* 1. 初始化 */
acomp_init();
acomp_fd_init();
acomp_fd_prepare();
acomp_fd_start();
acomp_fd_add_callback(FD_CB_EVENT_ENGINE_RLT, fd_event_handler, NULL);

/* 2. 配置活体检测 */
acomp_fd_live_detect_mode_t mode = {.enable = true, .score_threshold = {0.51, 0.1}};
acomp_fd_live_detect_mode_set(&mode);

/* 3. 摄像头回调 - 发送图像到算法 */
void video_camera_cb(uint8_t *data, uint32_t len) {
    uint8_t *buffer = acomp_fd_stream_tx_buffer_alloc(0, &buf_size, &desc_idx);
    acomp_fd_input_frame_t *frame = (acomp_fd_input_frame_t *)buffer;
    frame->format = PIX_FMT_YUV422_YUYV_PACKED;
    frame->width = 320; frame->height = 240; frame->length = len;
    memcpy(frame->data, data, len);
    acomp_fd_stream_tx_buffer_submit(0, buffer, buf_size, desc_idx);
}

/* 4. 结果回调 - 更新显示 */
void fd_event_handler(uint32_t event, void *event_data, uint32_t len, void *priv) {
    if (event & FD_CB_EVENT_ENGINE_RLT) {
        acomp_fd_result_info_t *info = (acomp_fd_result_info_t *)event_data;
        if (info->results_cnt > 0) {
            screen_update_fd_info(info, image_data, width, height, len);
        }
    }
}
```

## 配置说明

### 算法资源地址配置

在 `prj.conf` 中配置算法模型在 Flash 中的地址:

```
CONFIG_ACOMP_FD_RES_FACE_DETECT_ADDRESS=0x30100000
CONFIG_ACOMP_FD_RES_FACE_DETECT_LENGTH=450936
CONFIG_ACOMP_FD_RES_FACE_ALIGN_ADDRESS=0x30170000
CONFIG_ACOMP_FD_RES_FACE_ALIGN_LENGTH=1588664
CONFIG_ACOMP_FD_RES_FACE_LIVE_ADDRESS=0x30300000
CONFIG_ACOMP_FD_RES_FACE_LIVE_LENGTH=1095288
CONFIG_ACOMP_FD_FACE_VERIFY_ADDRESS=0x30410000
CONFIG_ACOMP_FD_FACE_VERIFY_LENGTH=2953752
```

### 摄像头配置

```
CONFIG_IMAGE_WIDTH=320
CONFIG_IMAGE_HEIGHT=240
CONFIG_IMAGE_FORMAT=2  # YUYV422
CONFIG_FACE_LIVE_DETECT_ENABLE=y # 启用活体检测
CONFIG_FACE_LIVE_DETECT_THRESHOLD=50 # 活体检测阈值
CONFIG_FACE_COMPARE_SCORE_THRESHOLD=90 # 人脸比对得分阈值
CONFIG_ONLY_FACE_REGISTER=y # 只注册人脸,后续自动进行人脸识别(不需要用K2按键触发)
```

### PSRAM 堆大小配置

```
CONFIG_PSRAM_HEAP_SIZE=0x680000
```

## 注意事项

1. **算法资源烧录**: 必须先烧录 AP 核固件和 4 个算法模型资源到 Flash，否则人脸识别算法无法正常运行
2. **内存配置**: 人脸识别算法需要较大的 PSRAM 空间，建议配置 `CONFIG_PSRAM_HEAP_SIZE` 不小于 0x680000
3. **摄像头连接**: 确保 GC0328 摄像头正确连接到 DVP 接口，否则无法采集图像
4. **显示屏连接**: 确保 ST7789 LCD 正确连接到 SPI 接口，否则无法显示画面
5. **活体检测阈值**: 活体得分阈值需要根据实际应用场景调整，阈值越高误识率越低但拒识率越高
6. **双核协作**: 本示例运行在 CP 核 (HARTID=1)，需要 AP 核 (HARTID=0) 提供算法支持
7. **特征注册**: 最多支持 10 个注册人脸，每个人脸特征为 384 维浮点数
8. **回调函数**: 人脸识别结果通过回调函数异步返回，不要在回调中执行耗时操作
