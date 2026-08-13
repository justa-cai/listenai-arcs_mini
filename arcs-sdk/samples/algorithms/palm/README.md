# 掌静脉算法示例

## 功能说明

演示如何使用 ACOMP 掌静脉组件进行掌静脉检测、特征提取和结果输出。本示例展示了掌静脉算法的基本使用流程，包括图像输入、算法处理和结果回调。

## 硬件连接

- **调试串口**: CP核的UART0,PA3引脚，波特率 921600,用于日志输出
- **调试串口**: AP核的UART1,PA21引脚，波特率 921600,用于日志输出

## 示例内容

1. 初始化 ACOMP 组件、掌静脉组件
2. 创建图像输入数据流通道
3. 发送内置 BGR888 测试图像到掌静脉算法
4. 接收并处理掌静脉识别结果(掌静脉框、关键点数量、特征数量、比对结果等)

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 烧录

### 1. 烧录 AP 核固件(Boot Core)

```bash
cskburn -s /dev/ttyACM0 -b 3000000 -C arcs 0x00 ./res/ap.bin
```

### 2. 烧录算法资源到 Flash

```bash
# 掌静脉检测模型
cskburn -s /dev/ttyACM0 -b 3000000 -C arcs 0x90000 ./res/algo/palm_detect_thinker.bin

# 掌静脉特征提取模型
cskburn -s /dev/ttyACM0 -b 3000000 -C arcs 0xB0000 ./res/algo/palm_feature_thinker.bin
```

### 3. 烧录 CP 核固件

```bash
cskburn -s /dev/ttyACM0 -b 3000000 -C arcs 0x800000 ./build/arcs.bin
```

## 预期输出

```
********SDK 0.1.7 @ v0.1.0.pre-963-ge026a5dc7d23********
Running on cpu-id: 1
Reset reason: 0x00000000
I/elog            [67:08:35.276 1 elog_async] EasyLogger V2.2.99 is initialize success.
I/main            [67:08:35.277 1 main] CP=======! Hard ID: 1
I/main            [67:08:35.277 1 main] ic_message_init done!
I/acomp_ipc       [67:08:37.278 1 rpc_client] [0]acomp remote dev index 2 name acomp.palm
I/acomp_ipc       [67:08:37.278 1 rpc_client] [1]acomp remote dev index 1 name acomp.logger
I/acomp_palm      [67:08:37.278 1 main] acomp_palm_init enter
I/acomp_palm      [67:08:37.281 1 main] acomp_palm_init exit
INF:[acomp_stream_channel_create]Acomp stream channel 0x288f5674,0x20014f34,'stream.palm_image' created successfully ,vring phy addr=0x28814560, num_descs=4, buffer_size=230432, direction=M2R, role=Master

I/app_palm        [67:08:38.447 1 rpc_client] detect palm cnt:1
max area result:
index:0
palm_rect:[x:97, y:5, w:148, h:234]
palm_score:0.899121
align_cnt:5
feature_cnt:128
compare_cnt:0
compare_scores:[0.000000, 0.000000]

I/app_palm        [67:08:39.457 1 rpc_client] detect palm cnt:1
max area result:
index:0
palm_rect:[x:97, y:5, w:148, h:234]
palm_score:0.899121
align_cnt:5
feature_cnt:128
compare_cnt:0
compare_scores:[0.000000, 0.000000]

...
```

如果没有检测到掌静脉，会周期打印 `no palm detected`。如果 AP 固件或模型资源烧录不匹配，通常会在 `acomp_palm_prepare()`、`acomp_palm_start()` 或图像 stream 提交阶段看到错误日志。

## 核心 API

| API | 说明 |
|-----|------|
| `acomp_palm_init()` | 初始化掌静脉组件 |
| `acomp_palm_prepare()` | 准备掌静脉算法资源 |
| `acomp_palm_start()` | 启动掌静脉算法 |
| `acomp_palm_add_callback()` | 注册掌静脉结果回调函数 |
| `acomp_palm_stream_ch_enable()` | 使能图像输入数据流通道 |
| `acomp_palm_stream_tx_buffer_alloc()` | 分配发送缓冲区 |
| `acomp_palm_stream_tx_buffer_submit()` | 提交图像数据到算法 |

## 关键代码

### 初始化掌静脉组件

```c
/* 初始化算法组件 */
acomp_init();

/* 初始化掌静脉识别 */
acomp_palm_init();
acomp_palm_prepare();
acomp_palm_start();

/* 注册结果回调 */
acomp_palm_add_callback(PALM_CB_EVENT_ENGINE_RLT, palm_event_handler, NULL);
```

### 创建图像输入流

```c
acomp_stream_chn_create_desc_t desc = {
    .cname = "stream.palm_image",
    .direction = ACOMP_STREAM_DIRECTION_M2R,
    .index = 0,
    .buffer_size = 320 * 240 * 3 + sizeof(acomp_palm_input_frame_t),
    .num_descs = 4,
    .kick_policy = 1,
};

acomp_palm_stream_ch_enable(0, &desc);
```

### 发送图像数据

```c
/* 分配发送缓冲区 */
uint8_t *buffer = acomp_palm_stream_tx_buffer_alloc(0, &buf_size, &desc_idx);

/* 填充图像数据 */
acomp_palm_input_frame_t *palm_frame = (acomp_palm_input_frame_t *)buffer;
palm_frame->format = ACOMP_PALM_PIX_FMT_BGR888;
palm_frame->width = 320;
palm_frame->height = 240;
palm_frame->length = 320 * 240 * 3;
memcpy(palm_frame->data, image_data, palm_frame->length);

/* 提交到算法 */
acomp_palm_stream_tx_buffer_submit(0,
                                   buffer,
                                   sizeof(*palm_frame) + palm_frame->length,
                                   desc_idx);
```

## 配置说明

### 算法资源地址配置

在 `prj.conf` 中配置算法模型在 Flash 中的地址:

```kconfig
CONFIG_ACOMP_PALM_RES_DETECT_ADDRESS=0x30090000
CONFIG_ACOMP_PALM_RES_DETECT_LENGTH=83288
CONFIG_ACOMP_PALM_RES_VERIFY_ADDRESS=0x300B0000
CONFIG_ACOMP_PALM_RES_VERIFY_LENGTH=92680
```

对应烧录关系:

| 模型 | 烧录地址 | 配置地址 | 文件 |
|------|----------|----------|------|
| 掌静脉检测 | `0x90000` | `0x30090000` | `palm_detect_thinker.bin` |
| 掌静脉特征提取 | `0xB0000` | `0x300B0000` | `palm_feature_thinker.bin` |

### PSRAM 堆大小配置

```kconfig
CONFIG_PSRAM_HEAP_SIZE=0x200000
```

## 算法内存占用

算法内存占用如下(粗略统计)，详细内存分布见 `memap.h` 文件。

| MCU核心 | 内存类型 | 大小 | 备注 |
|---------|----------|------|------|
| AP | FLASH | ~480KB | AP 固件(~306KB) + 2 个 palm 算法资源(~172KB) |
| CP | FLASH | ~352KB | CP 固件，实际大小以编译输出为准 |
| CP | PSRAM | 2MB | `CONFIG_PSRAM_HEAP_SIZE=0x200000` |
| AP | PSRAM | ~480KB | 算法资源拷贝到 PSRAM(~176KB) + 实例 PSRAM(~60KB) + BGR888 图像缓冲(225KB) + 动态内存 |
| AP | SRAM | 8KB | IPC 共享内存 |
| AP | SRAM | 64KB | AP 普通 SRAM |
| AP | SRAM | 384KB | 算法实例 RAM 窗口，palm 实测实例约 60KB |
| AP | LUNASRAM | 28KB | LUNA code/data 共享区 |

算法资源各模型大小：

| 模型 | 文件 | 大小 |
|------|------|------|
| 掌静脉检测 | `palm_detect_thinker.bin` | 83288 B |
| 掌静脉特征提取 | `palm_feature_thinker.bin` | 92680 B |

## 注意事项

1. **算法资源烧录**: 必须先烧录 AP 核固件和算法模型资源到 Flash,否则掌静脉算法无法正常运行
2. **内存配置**: 本示例使用 320x240 BGR888 图像输入,stream buffer 大小为 `320 * 240 * 3 + sizeof(acomp_palm_input_frame_t)`
3. **图像格式**: 当前示例使用 BGR888 格式的图像输入,分辨率为 320x240
4. **测试图替换**: 替换 `src/test_image.h` 时需要保持符号名 `palm_320_240_bin` 和 `palm_320_240_bin_len`,或同步修改 `src/main.c`
5. **AP 最大输入尺寸**: 当前配套 AP palm 固件按最大 320x240 编译；如果要验证更大尺寸，需要同步重编 AP 固件并调整 `CONFIG_PALM_MAX_FRAME_WIDTH/HEIGHT`
6. **双核协作**: 本示例运行在 CP 核(HARTID=1),需要 AP 核(HARTID=0)提供 `acomp.palm` 算法支持
7. **回调函数**: 掌静脉结果通过回调函数异步返回,不要在回调中执行耗时操作
