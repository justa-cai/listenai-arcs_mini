# 人脸识别算法示例

## 功能说明

演示如何使用 ACOMP 人脸识别组件进行人脸检测、对齐、活体检测和特征提取。本示例展示了人脸识别算法的基本使用流程,包括图像输入、算法处理和结果输出。

## 硬件连接

- **调试串口**: CP核的UART0,PA3引脚，波特率 921600,用于日志输出
- **调试串口**: AP核的UART1,PA21引脚，波特率 921600,用于日志输出

## 示例内容

1. 初始化 ACOMP组件、人脸识别组件
2. 配置活体检测模式和阈值
3. 创建图像输入数据流通道
4. 发送测试图像到人脸识别算法
5. 接收并处理人脸识别结果(人脸框、姿态角、活体得分等)

## 编译运行

```bash
./build.sh -C -DBOARD=arcs_evb
```

## 烧录固件

### 1. 烧录 AP 核固件(Boot Core)

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

## CP预期输出

```
********Arcs SDK 0.1.1 @ v0.0.1-860-gd85bab268324********
Running on hart-id: 1
CP=======! Hard ID: 1
I/elog            [00:00:00.009 1 elog_async] EasyLogger V2.2.99 is initialize success.
I/main            [00:00:00.011 1 main] ic_message_init done!
I/acomp_ipc       [00:00:02.011 1 rpc_client] [0]acomp remote dev index 1 name acomp.fd
I/acomp_fd        [00:00:02.011 1 main] acomp fd init enter
I/acomp_fd        [00:00:02.012 1 main] acomp fd dev index 1,name:acomp.fd
I/acomp_fd        [00:00:02.012 1 main] acomp fd dev index 1,name:acomp.fd
I/acomp_fd        [00:00:02.013 1 main] acomp fd init exit
I/acomp_fd        [00:00:02.013 1 main] acomp fd prepare enter
I/acomp_fd        [00:00:02.013 1 main] acomp fd prepare:0x2880c4c0, size:128
I/acomp_fd        [00:00:02.015 1 main] acomp fd prepare exit
I/acomp_fd        [00:00:02.015 1 main] acomp fd start enter
I/acomp_fd        [00:00:02.215 1 main] acomp fd start exit
I/acomp_fd        [00:00:02.215 1 main] acomp fd add callback enter
I/acomp_fd        [00:00:02.216 1 main] acomp fd add callback exit
I/acomp_fd        [00:00:02.216 1 main] acomp_fd_live_detect_mode_set enter
I/acomp_fd        [00:00:02.217 1 main] acomp_fd_live_detect_mode_set exit
INF:[acomp_stream_channel_create]Acomp stream channel 0x288aa5f4,0x200150d4,'stream.fd_image' created successfully ,vring phy addr=0x288144e0, num_descs=4, buffer_size=153632, direction=M2R, role=Master

I/acomp_fd        [00:00:02.227 1 main] acomp_fd_stream_ch_enable chn(stream.fd_image) index(0),desc(0x2880bb58)
I/app_fd          [00:00:03.420 1 rpc_client] fd result:2732,
I/app_fd          [00:00:03.420 1 rpc_client] detect face cnt: 1

max area result:
index: 0
face_rect: [x:124, y:96, w:84, h:106]
face_score: 0.909907
face_align_cnt:68
head_pose:[yaw:-1.790493, pitch:-4.476233, roll:-2.685740]
face_live_result:[status:1, scores[0]:0.000066, scores[1]:0.999934]
feature_cnt:384
compare_cnt:0
compare_scores:[0.000000, 0.000000]

...

```

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

## 关键代码

### 初始化人脸识别组件

```c
/* 初始化算法组件 */
acomp_init();

/* 初始化人脸识别 */
acomp_fd_init();
acomp_fd_prepare();
acomp_fd_start();

/* 注册结果回调 */
acomp_fd_add_callback(FD_CB_EVENT_ENGINE_RLT, fd_event_handler, NULL);

/* 配置活体检测 */
const acomp_fd_live_detect_mode_t mode = {
    .enable = true,
    .score_threshold = {
        0.51,   // 非活体阈值
        0.1,    // 活体得分阈值（如果活体得分大于等于这个阈值， 就认为是活体，如果小于这个阈值，就不会进行人脸特征提取和人脸识别）
    },
};
acomp_fd_live_detect_mode_set(&mode);
```

### 创建图像输入流

```c
acomp_stream_chn_create_desc_t desc = {
    .cname = "stream.fd_image",
    .direction = ACOMP_STREAM_DIRECTION_M2R,
    .index = 0,
    .buffer_size = 320 * 240 * 2 + sizeof(acomp_fd_input_frame_t),
    .num_descs = 4,
    .kick_policy = 1,
};

acomp_fd_stream_ch_enable(0, &desc);
```

### 发送图像数据

```c
/* 分配发送缓冲区 */
uint8_t *buffer = acomp_fd_stream_tx_buffer_alloc(0, &buf_size, &desc_idx);

/* 填充图像数据 */
acomp_fd_input_frame_t *fd_frame = (acomp_fd_input_frame_t *)buffer;
fd_frame->format = PIX_FMT_YUV422_YUYV_PACKED;
fd_frame->width = 320;
fd_frame->height = 240;
fd_frame->length = 320 * 240 * 2;
memcpy(fd_frame->data, image_data, fd_frame->length);

/* 提交到算法 */
acomp_fd_stream_tx_buffer_submit(0, buffer, buf_size, desc_idx);
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

### PSRAM 堆大小配置

```
CONFIG_PSRAM_HEAP_SIZE=0x100000
```

## 算法内存占用

双麦算法内存占用如下(粗略统计)， 详细内存分布见`memap.h`文件

|  MCU核心 |  内存类型 | 大小 | 备注 |
|---------|----------|----|-----|
|  AP |  FLASH | 6546KB   | 4个算法资源 + ap固件大小 + cp固件大小 |
|  AP |  PSRAM |  7900KB   | 算法实例用的PSRAM + 4个算法资源拷贝到PSRAM上 + 动态内存 |
|  AP |  SRAM  |  8KB | IPC共享内存 |
|  AP |  SRAM  |  60KB | AP的SRAM |
|  AP |  SRAM  |  384KB | 算法实例大小 |
|  AP |  SRAM  |  28KB | LUNA的*(.sharedmem.*) |
|  AP |  LUNASRAM |  64KB  | LUNA专用的SRAM大小 |

## 注意事项

1. **算法资源烧录**: 必须先烧录 AP 核固件和算法模型资源到 Flash,否则人脸识别算法无法正常运行
2. **内存配置**: 人脸识别算法需要较大的 PSRAM 空间,建议配置 `CONFIG_PSRAM_HEAP_SIZE` 不小于 0x100000
3. **图像格式**: 当前示例使用 YUYV422 格式的图像输入,分辨率为 320x240
4. **活体检测阈值**: 活体得分阈值需要根据实际应用场景调整,阈值越高误识率越低但拒识率越高
5. **双核协作**: 本示例运行在 CP 核(HARTID=1),需要 AP 核(HARTID=0)提供算法支持
6. **回调函数**: 人脸识别结果通过回调函数异步返回,不要在回调中执行耗时操作
