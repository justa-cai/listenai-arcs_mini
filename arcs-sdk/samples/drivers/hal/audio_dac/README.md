# Audio DAC 示例

本示例演示 CP 核生成音频数据并通过核间通信传输给 AP 核的功能。CP 核负责生成 1kHz 正弦波音频数据并发送到 AP 核。

## 📖 示例说明

### 功能演示

本示例演示以下功能：

- **双核音频数据传输**：CP 核生成音频数据并发送到 AP 核
- **核间通信**：使用 ICStream、ICMessage、ICProxy 进行核间数据传输
- **共享内存机制**：通过共享内存实现 CP 核和 AP 核的数据交互
- **音频生成**：CP 核生成 1kHz 正弦波测试音频
- **Producer-Consumer 模式**：CP 核作为生产者，AP 核作为消费者

### 硬件要求

- 支持双核（AP/CP）的 CSK 系列开发板
- USB 转串口工具（用于查看日志输出和烧录）

## 🚀 快速开始

### 1. 构建项目

在示例目录下执行构建脚本：

```bash
cd samples/drivers/audio_dac
./build.sh
```

或者在 SDK 根目录执行：

```bash
./build.sh -S samples/drivers/audio_dac -C
```

构建成功后，会在 `build` 目录下生成 CP 核固件 `arcs.bin`。

```shell
cskburn -s /dev/ttyUSB0 -b 3000000 0x0 ap.bin -C arcs
```

#### 烧录 CP 核固件

```shell
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
CONFIG_LOG=n                          # 禁用日志
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
| `PLAYSTREAM_FRAME_SAMPLES` | 768 | 每帧采样点数 |
| 采样率 | 16000 Hz | 音频采样率 |
| 频率 | 1000 Hz | 生成的测试音频频率 |
| 幅度 | 512 | 16位PCM幅度值 |
| 采样精度 | 16位 | 采样精度 |

## 📋 代码解析

### 关键代码段

#### 1. 核间通信初始化

```c
int main(int argc, char **argv)
{
    printf("Hello, world! Audio DAC\n\n");
    
    // 延时等待系统稳定
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // 初始化核间通信消息系统
    ic_message_init();
    
    vTaskDelay(pdMS_TO_TICKS(20));
    
    // 启动音频播放服务
    audio_play_service_start();
}
```

#### 2. 远程流同步

```c
static void PlayService_remote_sync(void)
{
    // 挂接到已经创建好的核间数据流通道上
    // 此时server端的ICStream对象已就绪
    s_play_stream = IC_Proxy_getRemoteICStream(&play_Stream_0, 
                                                ap2cp_play_stream_id);
    
    // 对象级的核间同步，以确认对端已经就绪
    ICStream_Producer_syncWithConsumer(s_play_stream);
}

static void audio_play_service_start()
{
    PlayService_remote_sync();
}
```

#### 3. 音频数据生成

```c
static void generate_1khz_audio(int16_t *buffer, size_t length) {
    const double sample_rate = 16000.0;  // 采样率
    const double frequency = 1000.0;      // 频率
    const double amplitude = 512.0;       // 16位PCM的最大值
    
    for (size_t i = 0; i < length; i++) {
        double t = i / sample_rate;
        // 生成正弦波
        double value = amplitude * sin(2 * M_PI * frequency * t);
        buffer[i] = (int16_t)value;
    }
}
```

#### 4. 音频帧获取

```c
static void PlayStream_Producer_acquireInterCoreFrame(void)
{
    int ret;
    
    // 获取最新的共享状态
    ICStream_Producer_fetchRemote(s_play_stream);
    
    // 先检查是否有空帧
    if (ICStream_Producer_isFull(s_play_stream)) {
        // 等待空帧出现，如果已经有，则立即返回，否则阻塞等待让出调度
        ICStream_Producer_waitFrame(s_play_stream);
        
        // 再获取最新的共享状态
        ICStream_Producer_fetchRemote(s_play_stream);
    }
    else {
        // 就算内存先看见指针更新，还是要等对端通知，以保持流程同步
        ICStream_Producer_waitFrame(s_play_stream);
    }
    
    // 失败则返回出错码，无关远程
    ret = ICStream_Producer_acquireFrame(s_play_stream, &p_stream_frame_0);
    assert(IC_OK == ret);
}
```

#### 5. 音频帧释放

```c
static void PlayStream_Producer_releaseInterCoreFrame(void)
{
    int ret;
    
    // 失败则返回出错码，无关远程
    ret = ICStream_Producer_releaseFrame(s_play_stream, p_stream_frame_0);
    assert(IC_OK == ret); 
    
    // 发布最新状态至共享，通信机制出错码
    ret = ICStream_Producer_commitRemote(s_play_stream);
    assert(IC_OK == ret);
}
```

#### 6. 音频数据发送

```c
static void audio_play_send_pcm(char *data, int size)
{
    // 获取空闲帧
    PlayStream_Producer_acquireInterCoreFrame();
    
    // 复制音频数据到共享内存
    memcpy(p_stream_frame_0, data, size);
    
    // 释放帧，通知AP核
    PlayStream_Producer_releaseInterCoreFrame();
}
```

#### 7. 主循环

```c
int main(int argc, char **argv)
{
    // ... 初始化代码 ...
    
    short buffer[PLAYSTREAM_FRAME_SAMPLES*2];
    
    while (1) {
        // 清空缓冲区
        memset(buffer, 0, sizeof(buffer));
        
        // 生成1kHz音频数据
        generate_1khz_audio(buffer, PLAYSTREAM_FRAME_SAMPLES);
        
        // 发送音频数据到AP核
        audio_play_send_pcm((char *)buffer, PLAYSTREAM_FRAME_SAMPLES);
    }
}
```

## 🔍 预期输出

程序运行后，串口将输出以下日志：

```
********Arcs SDK@V0.0.10-8-g52552ce3-dirty-@v0.0.10********
Running on hart-id: 1
Hello, world! Audio DAC
```

输出说明：
- `Arcs SDK@...`：SDK 版本信息
- `Running on hart-id: 1`：程序运行在 HART 1（CP 核）
- `Hello, world! Audio DAC`：程序启动信息

## ⚠️ 注意事项

1. **双核固件烧录**：
   - 必须先烧录 AP 核固件（地址 0x0）
   - 再烧录 CP 核固件（地址 0xa00000）
   - 两个固件缺一不可

2. **核间通信机制**：
   - CP 核作为生产者（Producer）生成音频数据
   - AP 核作为消费者（Consumer）接收音频数据
   - 使用 ICStream 进行核间数据传输

3. **内存配置**：
   - 使用自定义内存配置文件 `memap.h`（`CONFIG_MEM_CONFIG_USE_CUSTOM_FILE=y`）
   - PSRAM 堆大小配置为 3.7MB（`CONFIG_PSRAM_HEAP_SIZE=0x3b0000`）

4. **音频数据格式**：
   - 每帧包含 768 个采样点（`PLAYSTREAM_FRAME_SAMPLES`）
   - 16 位有符号整数（`int16_t`）格式
   - 每次传输 768 字节数据

5. **同步机制**：
   - 启动时延时等待系统稳定（`vTaskDelay`）
   - 使用 `ICStream_Producer_waitFrame()` 等待空闲帧
   - 当缓冲区满时会阻塞等待（`ICStream_Producer_isFull` 检查）

6. **音频生成**：
   - 使用标准 C 数学库生成正弦波（`sin()`）
   - 采样率（16000 Hz）和频率（1000 Hz）在 `generate_1khz_audio()` 函数中定义

7. **缓冲区管理**：
   - 定义了 `PLAYSTREAM_FRAME_SAMPLES*2` 大小的缓冲区
   - 每次循环清空（`memset`）并重新生成数据（`generate_1khz_audio`）
   - 通过 `memcpy` 复制到共享内存帧
