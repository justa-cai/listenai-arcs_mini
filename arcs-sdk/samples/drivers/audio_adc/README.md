# Audio ADC 示例

本示例演示 AP 核和 CP 核之间通过共享内存进行音频数据传输的功能。AP 核负责录音，CP 核通过核间通信机制获取并处理音频数据。

## 📖 示例说明

### 功能演示

本示例演示以下功能：

- **双核音频采集**：AP 核负责音频采集，CP 核负责数据处理
- **核间通信**：使用 ICStream、ICMessage、ICProxy 进行核间数据传输
- **共享内存机制**：通过共享内存实现 AP 核和 CP 核的数据交互
- **录音控制**：支持启动、停止、暂停、恢复等录音控制命令
- **音频参数**：双麦 16kHz 采样率，16 位采样精度

### 硬件要求

- 支持双核（AP/CP）的 CSK 系列开发板
- 支持的麦克风接口（双麦克风）
- USB 转串口工具（用于查看日志输出和烧录）

## 🚀 快速开始

### 1. 构建项目

在示例目录下执行构建脚本：

```bash
cd samples/drivers/audio_adc
./build.sh
```

或者在 SDK 根目录执行：

```bash
./build.sh -S samples/drivers/audio_adc -C
```

构建成功后，会在 `build` 目录下生成 CP 核固件 `arcs.bin`。

### 2. 烧录固件

本示例需要烧录 AP 核和 CP 核两个固件。

#### 烧录 AP 核固件

烧录示例目录下的 `ap.bin` 文件：

```bash
cskburn -s /dev/ttyUSB0 -b 3000000 0x0 ap.bin -C arcs
```

#### 烧录 CP 核固件

烧录构建生成的 CP 核固件：

```bash
cskburn -s /dev/ttyUSB0 -b 3000000 0xa00000 build/arcs.bin -C arcs
```

参数说明：
- `-s /dev/ttyUSB0`：串口设备路径（根据实际情况修改）
- `-b 3000000`：烧录波特率
- `0x0`：AP 核固件烧录地址
- `0xa00000`：CP 核固件烧录地址

> 💡 详细烧录步骤请参考 {ref}`快速开始 - 烧录运行 <flashing>`。

### 3. 查看输出

烧录完成后，复位开发板，通过串口工具（如 minicom）查看 CP 核的日志输出。

## 🔧 配置说明

### 项目配置

本示例在 `prj.conf` 中配置：

```conf
CONFIG_LOG=y                          # 使能日志
CONFIG_BACK_TRACE=y                   # 使能堆栈回溯
CONFIG_MODULE_HEAP=y                  # 使能模块堆内存
CONFIG_PSRAM_HEAP_SIZE=0x3b0000      # PSRAM堆大小
CONFIG_BOOT=n                         # 禁用启动配置

CONFIG_MEM_CONFIG=n                   # 禁用默认内存配置
CONFIG_MEM_CONFIG_USE_CUSTOM_FILE=y  # 使用自定义内存配置

CONFIG_BOOT_HART=n                    # 禁用HART启动

CONFIG_ARCS_HAL_LSF=y                # 使能LSF硬件抽象层
CONFIG_IPC_LSF=y                     # 使能LSF核间通信
CONFIG_LSF_CLIENT=y                  # 使能LSF客户端
```

### 音频参数配置

| 参数 | 值 | 说明 |
|------|-----|------|
| `REC_CELL_SIZE` | 2 | 每个采样点字节数（16位） |
| `REC_CHANNELS` | 2 | 通道数（双麦克风） |
| `REC_FRM_SAMPS` | 256 | 每帧采样点数 |
| `REC_FRM_SIZE` | 1024 | 每帧数据大小（字节） |
| 采样率 | 16kHz | 音频采样率 |
| 采样精度 | 16位 | 采样精度 |

## 📋 代码解析

### 关键代码段

#### 1. 核间通信初始化

```c
int main(int argc, char **argv)
{
    printf("Hello, world! Audio ADC\n");
    
    // 初始化核间通信消息系统
    ic_message_init();
    
    // 检查AP核的消息系统是否就绪（带重试机制）
    const int max_retries = 10;
    int retries = 0;
    do {
        int32_t data = 0;
        int ret = ic_message_msg_send_by_id(IC_MESSAGE_ID_MIC, 
                                            IC_MESSAGE_MSG_TYPE_CMD, 
                                            &data, sizeof(data));
        if (ret == 0) {
            break;  // 通信正常
        } else {
            if (++retries >= max_retries) {
                printf("Error: Failed after %d attempts\n", max_retries);
                return -1;
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    } while (1);
    
    // 启动音频录制任务
    audio_recorder_task(NULL);
}
```

#### 2. 远程流同步

```c
void aadcservice_remote_sync(void)
{
    // 从AP核获取远程ICStream
    s_record_stream = IC_Proxy_getRemoteICStream(&aadc_Stream_0, 
                                                  ap2cp_record_stream_id);
    if (!s_record_stream) {
        printf("Error getting remote stream\n");
        return;
    }
    
    // 与AP核的生产者同步
    ICStream_Consumer_syncWithProducer(s_record_stream);
}
```

#### 3. 录音控制

```c
int record_ctrl(mic_cmd_msg_e cmd)
{
    int ret = 0;
    mic_cmd_t data = {0};
    
    data.func = cmd;  // 命令类型：START, STOP, PAUSE, RESUME
    data.arg = 0;
    
    // 向AP核发送录音控制命令
    ret = ic_message_msg_send_by_id(IC_MESSAGE_ID_MIC, 
                                    IC_MESSAGE_MSG_TYPE_CMD, 
                                    &data, sizeof(data));
    if (ret != 0) {
        printf("Error sending message: %d\n", ret);
    }
    
    return ret;
}
```

#### 4. 音频帧获取

```c
static void aadcstream_consumer_acquire_intercore_frame(void)
{
    int ret;
    
    // 获取并同步共享状态
    ICStream_Consumer_fetchRemote(s_record_stream);
    
    // 等待AP核产生新的数据帧
    ICStream_Consumer_waitFrame(s_record_stream);
    
    // 获取音频数据帧
    ret = ICStream_Consumer_acquireFrame(s_record_stream, &p_stream_frame_0);
    if (ret != IC_OK) {
        printf("Error acquiring frame: %d\n", ret);
    }
}
```

#### 5. 音频帧释放

```c
static void aadcstream_consumer_release_intercore_frame(void)
{
    int ret;
    
    // 释放已处理的音频帧
    ret = ICStream_Consumer_releaseFrame(s_record_stream, p_stream_frame_0);
    if (ret != IC_OK) {
        printf("Error releasing frame: %d\n", ret);
    }
    
    // 发布最新状态至共享内存
    ret = ICStream_Consumer_commitRemote(s_record_stream);
    if (ret != IC_OK) {
        printf("Error committing frame: %d\n", ret);
    }
}
```

#### 6. 音频录制任务

```c
static void audio_recorder_task(void *arg)
{
    // 延时等待系统稳定
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // 与AP核同步流
    aadcservice_remote_sync();
    
    // 启动录音
    record_ctrl(MIC_CMD_START);
    
    // 循环处理音频数据
    while (1) {
        // 获取音频帧
        aadcstream_consumer_acquire_intercore_frame();
        printf("Stream frame: %p\n", p_stream_frame_0);
        
        // 在此处理音频数据
        // Process the audio data here
        
        // 释放音频帧
        aadcstream_consumer_release_intercore_frame();
    }
}
```

## 🔍 预期输出

程序运行后，串口将输出以下日志（具体地址值与实际情况有关）：

```
Running on hart-id: 1
I/elog            [00:00:00.000 1 elog_async] EasyLogger V2.2.99 is initialize success.
Hello, world! Audio ADC
Stream frame: 0x28007b40
Stream frame: 0x28007f40
Stream frame: 0x28008340
Stream frame: 0x28008740
Stream frame: 0x28008b40
Stream frame: 0x28008f40
Stream frame: 0x28009340
Stream frame: 0x28009740
Stream frame: 0x28009b40
Stream frame: 0x28009f40
Stream frame: 0x2800a340
Stream frame: 0x2800a740
Stream frame: 0x2800ab40
Stream frame: 0x2800af40
Stream frame: 0x2800b340
```

输出说明：
- `Running on hart-id: 1`：程序运行在 HART 1（CP 核）
- `EasyLogger...`：日志系统初始化成功
- `Hello, world! Audio ADC`：程序启动信息
- `Stream frame: 0x...`：持续输出接收到的音频帧地址，表示正在接收音频数据

## ⚠️ 注意事项

1. **双核固件烧录**：
   - 必须先烧录 AP 核固件（地址 0x0）
   - 再烧录 CP 核固件（地址 0xa00000）
   - 两个固件缺一不可

2. **核间通信机制**：
   - AP 核作为生产者（Producer）采集音频数据
   - CP 核作为消费者（Consumer）读取音频数据
   - 使用共享内存和信号量机制保证数据同步

3. **内存配置**：
   - 使用自定义内存配置文件 `memap.h`
   - PSRAM 堆大小配置为 3.7MB（0x3b0000）
   - 确保共享内存区域正确配置

4. **音频数据格式**：
   - 每帧包含 256 个采样点（`REC_FRM_SAMPS`）
   - 双通道（`REC_CHANNELS = 2`）
   - 每个采样点 2 字节（`REC_CELL_SIZE = 2`）
   - 每帧数据大小：256 × 2 × 2 = 1024 字节（`REC_FRM_SIZE`）

5. **同步机制**：
   - 启动时需要等待 AP 核消息系统就绪
   - 使用重试机制确保核间通信建立成功
   - 每次获取帧前需要等待新数据到达

6. **数据处理**：
   - 代码注释标记了音频处理位置：`// Process the audio data here`
   - 处理完成后及时释放帧，避免阻塞 AP 核

7. **错误处理**：
   - 核间通信失败时会有错误日志输出（`Error sending message`、`Error acquiring frame` 等）
