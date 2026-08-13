# VenusA 人脸识别演示

## 功能说明

本演示是运行在 VenusA `csk7s_yt_evb` 上的 CP 侧人脸识别应用，参考 ARCS 人脸识别 demo 的流程，集成了摄像头图像采集、UVC 预览输出、人脸检测、人脸对齐、特征提取、人脸比对和按键触发统计等功能。CP 侧通过 ACOMP FD IPC 通路与 `apps/remote-ap` 通信，由 AP 侧运行人脸算法资源。

与 ARCS EVB 版本不同，`csk7s_yt_evb` 当前没有接入 LCD/触摸/ADC 按键，本 demo 使用 DVP 摄像头输入、CherryUSB UVC 视频输出和 PA04 GPIO 物理按键，通过串口查看识别、注册和统计结果。

## 系统架构

```
┌──────────────┐
│   摄像头模块   │ SC030IOT 采集图像 (640x480 YUYV422)
└──────────────┘
        │
        │
        ├──────────────▶ USB UVC 预览 (YUY2 640x480@10fps)
        │
        │ (识别通路跳帧发送)
        ▼
┌──────────────┐      ACOMP FD IPC       ┌────────────────────────┐
│  CP 侧 demo   │────────────────────────▶│ 人脸识别算法模块 (AP)     │
│ face-detect  │◀────────────────────────│ 检测/对齐/特征/比对       │
└──────────────┘      (人脸结果)          └────────────────────────┘
        ▲
        │ (单击触发处理最新结果)
        │
┌──────────────┐
│  PA04 GPIO KEY │
└──────────────┘
        │
        ▼
┌──────────────┐
│  PA07串口输出 │ 当前 ID / 次数 / 占比统计
└──────────────┘
```

## 主要功能

#### 1. 实时图像采集
- SC030IOT 摄像头通过 DVP 接口采集 640x480 分辨率图像
- 图像格式为 YUYV422（`CONFIG_IMAGE_FORMAT=2`），VenusA FD 库直接处理该格式
- CP 侧按分辨率自动跳帧，降低 IPC 和算法处理压力
- 摄像头帧通过 `fd_image_stream` 发送到 AP 侧人脸算法模块

#### 2. UVC 视频预览
- 默认开启 `CONFIG_FACE_DETECT_UVC_ENABLE=y`，通过 CherryUSB Device Video 枚举为 USB 摄像头
- UVC 输出格式为 YUY2，分辨率跟随 `CONFIG_IMAGE_WIDTH` / `CONFIG_IMAGE_HEIGHT`，当前为 640x480
- 默认帧率为 `CONFIG_FACE_DETECT_UVC_FPS=10`，UVC 任务会按该帧率节流输出
- UVC 使用摄像头原始 YUV422 帧，提交发生在人脸识别跳帧之前，因此预览帧率不受 `SKIP_FRAME_CNT` 影响
- 主机端打开 UVC 设备后才会持续取帧；未打开时 `uvc_stream_submit_frame()` 会直接返回，不影响人脸识别通路

#### 3. 人脸识别算法
人脸识别算法在 AP 侧运行，对输入图像执行以下流程：
- **人脸检测**：输出人脸数量、最大人脸索引、矩形框、置信度等结果
- **人脸对齐**：输出关键点数量和头部姿态角（yaw/pitch/roll）
- **特征提取**：输出人脸特征向量，用于注册和比对
- **特征比对**：将当前特征与已加载的人脸特征库进行相似度比对

VenusA 当前提供 detect / align / verify 三类资源，未提供 live 模型，因此默认 `CONFIG_FACE_LIVE_DETECT_ENABLE=n`，活体检测资源地址和长度配置为 0。

#### 4. 按键交互与统计
- **PA04 单击**：处理最近一次有效人脸结果
- 若当前人脸与已注册特征匹配，则对应 ID 的成功次数加 1
- 若当前人脸质量有效但未匹配已注册特征，则注册为新的 RAM-only ID，成功次数从 1 开始
- 每次成功识别或注册后，通过串口输出当前 ID、当前分数、当前次数、当前 ID 占比和所有 ID 统计
- 失败原因通过 `FACE_RESULT fail reason=...` 输出，便于自动化测试或日志分析

#### 5. 资源与 Flash 布局
VenusA FD 使用以下算法资源：
- `res/algo/fdetect_venusA.bin`
- `res/algo/falign_venusA104_concat.bin`
- `res/algo/fverify_venusA.bin`

合并固件使用以下 Flash 布局：

| Offset | File |
| --- | --- |
| `0x000000` | `res/ap.bin` |
| `0x100000` | `res/algo/fdetect_venusA.bin` |
| `0x170000` | `res/algo/falign_venusA104_concat.bin` |
| `0x2E0000` | `res/algo/fverify_venusA.bin` |
| `0x800000` | `build/venusa_face_detect.bin` |

## 硬件连接

- **开发板**：`csk7s_yt_evb`
- **摄像头**：SC030IOT 摄像头模块，通过 DVP 接口连接
- **按键**：PA04 GPIO 物理按键，低电平按下
- **调试串口**：UART0，用于查看 CP 日志；开启 `CONFIG_ACOMP_LOGGER=y` 后 AP 侧日志也会转发到 CP 侧输出
- **USB UVC**：通过 USB Device 连接主机，主机端可作为标准 UVC 摄像头打开实时预览
- **显示屏/触摸/ADC 按键**：当前 VenusA demo 未启用

## UVC 配置与使用

### 配置项

| 配置项 | 默认值 | 说明 |
| --- | --- | --- |
| `CONFIG_FACE_DETECT_UVC_ENABLE` | `y` | 开启 CherryUSB UVC 视频输出 |
| `CONFIG_FACE_DETECT_UVC_FPS` | `10` | UVC 声明并节流的输出帧率，取值范围 1~30 |
| `CONFIG_IMAGE_WIDTH` | `640` | UVC 与算法输入共用的图像宽度 |
| `CONFIG_IMAGE_HEIGHT` | `480` | UVC 与算法输入共用的图像高度 |
| `CONFIG_IMAGE_FORMAT` | `2` | 当前要求为 YUV422/YUY2；非 YUV422 时 UVC 会打印 warning |
| `CONFIG_CHERRYUSB_DEVICE_SPEED_HS` | `y` | 使用 CherryUSB HS Device，单包 payload 为 1024 字节 |

如需关闭 UVC，只需在 `prj.conf` 中改为：

```text
CONFIG_FACE_DETECT_UVC_ENABLE=n
```

### 主机端预览

1. 烧录并启动固件后，串口会出现 `UVC ready: YUY2 640x480@10fps` 日志
2. 使用 USB 线连接开发板 USB Device 到主机
3. 在主机端打开名为 `VenusA Face Detect UVC` 的摄像头设备
4. 打开预览后，固件会打印 `UVC open ...`；关闭预览时会打印 `UVC close ...`

### Windows 查看方式

- **系统相机**：打开 Windows 自带“相机”应用，点击切换摄像头按钮，选择 `VenusA Face Detect UVC`
- **设备管理器**：在“设备管理器 -> 照相机”或“图像设备”下确认是否枚举出 `VenusA Face Detect UVC`
- **VLC**：选择“媒体 -> 打开捕获设备”，捕获模式选 `DirectShow`，视频设备名选择 `VenusA Face Detect UVC`，点击“播放”
- **OBS/其它工具**：添加“视频采集设备”，设备选择 `VenusA Face Detect UVC`，分辨率选择 640x480，帧率选择 10fps

### macOS 查看方式

- **QuickTime Player**：选择“文件 -> 新建影片录制”，点击录制按钮旁边的下拉菜单，摄像头选择 `VenusA Face Detect UVC`
- **Photo Booth**：打开 Photo Booth，在“相机”菜单中选择 `VenusA Face Detect UVC`
- **VLC**：选择“文件 -> 打开捕获设备”，视频设备选择 `VenusA Face Detect UVC` 后打开预览
- **权限检查**：若应用看不到画面，检查“系统设置 -> 隐私与安全性 -> 相机”，允许对应应用访问摄像头

### ffplay 查看方式

Windows 使用 DirectShow 输入，先列出摄像头设备名：

```powershell
ffmpeg -list_devices true -f dshow -i dummy
```

确认设备名后打开预览：

```powershell
ffplay -f dshow -video_size 640x480 -framerate 10 -i video="VenusA Face Detect UVC"
```

macOS 使用 AVFoundation 输入，先列出摄像头设备名和索引：

```bash
ffmpeg -f avfoundation -list_devices true -i ""
```

通过设备名打开预览：

```bash
ffplay -f avfoundation -video_size 640x480 -framerate 10 -pixel_format yuyv422 -i "VenusA Face Detect UVC:none"
```

如果 macOS 设备名无法直接打开，可使用上一步列出的索引号，例如视频设备索引为 `0`：

```bash
ffplay -f avfoundation -video_size 640x480 -framerate 10 -pixel_format yuyv422 -i "0:none"
```

> 如果主机端无法枚举 UVC 设备，请优先确认 USB 线支持数据传输，并避免多个应用同时占用同一个摄像头设备。

> UVC 预览只输出原始摄像头画面，不叠加人脸框、ID 或统计信息；识别结果仍通过串口输出。

## 使用说明

1. 烧录合并固件后，系统自动初始化 ACOMP、FD 算法、IPC 图像流、UVC、PA04 按键和摄像头
2. 摄像头持续采集图像：原始帧用于 UVC 预览，跳帧后的帧发送到 AP 侧进行人脸算法处理
3. 可选：在主机端打开 UVC 摄像头查看实时画面，辅助调整人脸位置
4. 将人脸对准摄像头，等待算法产生有效结果
5. 单击 PA04：
   - 若识别为已注册人脸，累计该 ID 成功次数
   - 若满足质量条件且未匹配已注册人脸，注册新 ID 并从 1 次开始统计
   - 若不满足条件，输出 `FACE_RESULT fail reason=...`
6. 查看串口统计块确认当前 ID、次数和百分比

> 注意：本 demo 没有使用持久化存储注册的人脸特征，每次上电后都需要重新注册人脸。

## 人脸注册与比对判定条件

### PA04 注册成功条件（需同时满足）

- 最新结果有效，且至少检测到 1 张人脸（`results_cnt > 0`）
- 使用最大面积人脸结果（`max_area_results_index`）
- 人脸质量满足：
  - `face_score > 0.7`
  - `n_align_point > 0`
  - 姿态角范围：`-30 < yaw/pitch/roll < 30`
  - `feature_cnt > 0`
  - `feature_cnt <= ACOMP_FD_MAX_FEATURE_CNT`
- 当前人脸与已注册特征的最大相似度不高于阈值（否则按已注册人脸统计）：
  - `max_score <= CONFIG_FACE_COMPARE_SCORE_THRESHOLD / 100`
- 未超过最大注册数量：`fd_feature_cnt < ACOMP_FD_MAX_RESULT_CNT`（最多 10 个）
- `acomp_fd_features_load()` 成功加载更新后的人脸特征库

### PA04 注册失败条件（任一触发即失败）

- 尚未收到算法结果（`no_result`）
- 未检测到人脸（`no_face`）
- 结果数量、结果索引或结果长度异常（`too_many_results` / `invalid_result` / `short_payload` / `short_result`）
- 人脸质量条件不满足（`low_quality`）
- 已达到最大可注册数量（`full`）
- 特征库加载失败（`feature_load`）

### 比对成功条件

- PA04 单击后，当前最大面积人脸通过质量检查
- 已注册特征库中存在可比对特征（`compare_cnt > 0`）
- 当前最大相似度满足：
  - `max_score > CONFIG_FACE_COMPARE_SCORE_THRESHOLD / 100`
- 成功后输出当前 ID、当前分数、当前次数、当前 ID 占比和所有 ID 统计

### 比对失败条件

- 不满足质量检查或没有可比对特征
- 或者最大相似度未超过阈值，此时若注册数量未满，会将当前人脸注册为新 ID

## 编译运行

### 1. 编译 AP 侧 `remote-ap` 固件

从仓库根目录执行：

```bash
# 目前不开源, 一般不需要自行编译

cd apps/remote-ap
./build.sh -C -DBOARD=venusa_rd_evb

# 拷贝替换 face_detect/res/ap.bin

```

### 2. 编译 CP 侧 face-detect demo

```bash
cd arcs-sdk
./build.sh -C -S demos/venusa/face_detect -DBOARD=csk7s_yt_evb

```

### 3. 合并固件

```bash
demos/venusa/face_detect/merge_bins.py
```

生成文件：

```text
demos/venusa/face_detect/merged_firmware.bin
```

## 烧录固件

在 `arcs-sdk` 目录下执行：

```bash
cskburn -C venus -s <tty> -b 3000000 \
  0x0 demos/venusa/face_detect/merged_firmware.bin
```

其中 `<tty>` 替换为实际串口设备，例如 `/dev/tty.usbserial-xxxx`。

## 预期输出

**CP 侧串口输出示例:**

```text
VenusA face-detect CP demo, hart:1
I/acomp_fd        [...] acomp fd init enter
I/acomp_fd        [...] acomp fd start exit
I/main            [...] PA04 gpio button ready
I/uvc_stream      [...] UVC ready: YUY2 640x480@10fps, frame:614400 payload:1024
I/video_camera    [...] camera config: w:640, h:480, format:2
I/video_camera    [...] Camera capture started with internal task
```

单击 PA04 后，成功注册或识别时会输出：

```text

I/main            [...] ----------face_detect result-----------------
I/main            [...] face score:0.740460, align_n:68, head_pose:[12.224598,26.405865,-5.455006], live:0, feature_cnt:384, compare_cnt:0
I/main            [...] FACE_QUALITY score:1(0.740460>0.700000), align:1(68), yaw:1(12.224598), pitch:1(26.405865), roll:1(-5.455006), feature:1(384/384)
I/acomp_fd        [...] acomp_fd_features_load enter
I/acomp_fd        [...] acomp_fd_features_load exit
I/main            [...] FACE_RESULT ok action=register id=0 score=0.740460 count=1
Current ID: 0
Current Score: 0.74
Current Times: 1
Current Attration: 100%
ToTal Attration:
ID: 0 Times: 1 Attraction: 100%
I/main            [...] -----------------end-----------------------

```

常见失败输出：

```text
W/main            [...] FACE_RESULT fail reason=no_result
W/main            [...] FACE_RESULT fail reason=no_face
W/main            [...] FACE_RESULT fail reason=low_quality
W/main            [...] FACE_RESULT fail reason=full score=0.123456 registered=10
```
