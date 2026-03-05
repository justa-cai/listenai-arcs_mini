# USB CDC 录音系统快速开始指南

## 概述

本文档介绍如何使用新的USB CDC串口录音系统，该系统使用自定义二进制协议进行通信，支持：

- 命令控制（开始/停止录音、查询状态）
- 音频数据传输（带序列号防丢包）
- MD5校验（确保数据完整性）
- 无需依赖adb shell命令

## 系统架构

```
┌──────────────┐                    ┌──────────────┐
│   PC端       │                    │   设备端     │
│              │                    │              │
│  Python脚本  │ ◄──USB CDC协议──► │  C代码       │
│              │                    │              │
│ - 发送命令   │                    │ - 接收命令   │
│ - 接收数据   │                    │ - 发送音频   │
│ - 计算MD5    │                    │ - 计算MD5    │
└──────────────┘                    └──────────────┘
```

## 文件说明

### 协议相关文件

| 文件 | 说明 |
|------|------|
| [docs/usb_cdc_protocol.md](usb_cdc_protocol.md) | 完整的协议规范文档 |
| [src/middleware/usb/cdc_protocol.h](../src/middleware/usb/cdc_protocol.h) | 协议头文件（C） |
| [src/middleware/usb/cdc_protocol.c](../src/middleware/usb/cdc_protocol.c) | 协议实现（C） |
| [src/middleware/usb/app_usb_cdc.h](../src/middleware/usb/app_usb_cdc.h) | USB CDC接口头文件 |
| [src/middleware/usb/app_usb_cdc.c](../src/middleware/usb/app_usb_cdc.c) | USB CDC实现文件 |
| [tools/audio/serial_capture.py](../tools/audio/serial_capture.py) | PC端录音脚本（Python） |

## PC端使用说明

### 1. 环境准备

确保已安装Python 3和pyserial库：

```bash
# 安装依赖
pip3 install pyserial

# 或者使用系统包管理器
# Ubuntu/Debian
sudo apt-get install python3-serial

# macOS
brew install python3
pip3 install pyserial
```

### 2. 查找串口设备

**Linux:**
```bash
# 查看所有串口设备
ls /dev/ttyACM* /dev/ttyUSB*

# 通常USB CDC设备显示为 /dev/ttyACM0
```

**Windows:**
```powershell
# 在设备管理器中查看 "端口(COM和LPT)"
# 通常显示为 COM3, COM4 等
```

**macOS:**
```bash
# 查看所有串口设备
ls /dev/tty.*

# USB CDC设备通常显示为 /dev/tty.usbmodemXXXX
```

### 3. 录音操作

**基本用法：**

```bash
# Linux
python3 tools/audio/serial_capture.py -p /dev/ttyACM0 -b 115200 -o output.pcm

# Windows
python tools/audio/serial_capture.py -p COM3 -b 115200 -o output.pcm

# macOS
python3 tools/audio/serial_capture.py -p /dev/tty.usbmodem14201 -b 115200 -o output.pcm
```

**完整示例：**

```bash
# 指定所有参数
python3 tools/audio/serial_capture.py \
    --port /dev/ttyACM0 \
    --baudrate 115200 \
    --output recordings/audio_$(date +%Y%m%d_%H%M%S).pcm \
    --timeout 2
```

### 4. 录音流程

脚本运行后会自动执行以下步骤：

1. **连接串口**
   ```
   [2026-01-07 10:30:15] 串口已打开: /dev/ttyACM0
   波特率: 115200
   输出文件: output.pcm
   --------------------------------------------------
   串口缓存已清空
   ```

2. **发送开始命令**
   ```
   --------------------------------------------------
   [命令] 发送: 开始录音
   [响应] 开始录音: 成功
   开始接收音频数据... (按 Ctrl+C 停止)
   --------------------------------------------------
   ```

3. **接收音频数据**
   ```
   已接收: 102400 字节 (序列号: 12)
   ```

4. **停止录音（按Ctrl+C）**
   ```
   用户中断

   停止录音...
   --------------------------------------------------
   [命令] 发送: 停止录音
   等待设备发送MD5...
   [MD5] 设备端: a1b2c3d4e5f6789012345678abcdef01
   [响应] 停止录音: 成功
   ```

5. **验证MD5**
   ```
   --------------------------------------------------
   录音完成!
   总字节数: 102400
   总帧数: 13
   PC端MD5: a1b2c3d4e5f6789012345678abcdef01
   设备端MD5: a1b2c3d4e5f6789012345678abcdef01
   MD5校验: 通过 ✓
   --------------------------------------------------
   ```

### 5. 播放录音

录音文件为原始PCM格式，可以使用以下工具播放：

**使用ffplay（推荐）：**
```bash
# 16kHz, 16位, 单声道
ffplay -f s16le -ar 16000 -ac 1 output.pcm

# 48kHz, 16位, 双声道
ffplay -f s16le -ar 48000 -ac 2 output.pcm
```

**转换为WAV格式：**
```bash
# 使用ffmpeg转换
ffmpeg -f s16le -ar 16000 -ac 1 -i output.pcm output.wav
```

**使用sox播放：**
```bash
play -t raw -r 16000 -e signed -b 16 -c 1 output.pcm
```

## 设备端使用说明

### 1. 添加源文件到编译系统

确保以下文件被包含在编译中：

```cmake
# CMakeLists.txt
target_sources(your_target PRIVATE
    src/middleware/usb/cdc_protocol.c
    src/middleware/usb/app_usb_cdc.c
)

target_include_directories(your_target PRIVATE
    src/middleware/usb
)
```

### 2. 初始化USB CDC

在应用初始化代码中调用：

```c
#include "app_usb_cdc.h"

// 在main或init函数中
int ret = app_usb_cdc_init();
if (ret != 0) {
    LISA_LOGE("APP", "Failed to initialize USB CDC");
    return ret;
}
```

### 3. 发送音频数据

在音频采集回调中发送数据：

```c
#include "app_usb_cdc.h"

void audio_callback(uint8_t *pcm_data, uint32_t len)
{
    // 只有在录音状态才发送
    if (app_usb_cdc_is_recording()) {
        // 数据会被自动封装成协议帧并发送
        app_usb_cdc_audio_write(pcm_data, len);
    }
}
```

### 4. 配置MD5计算（可选）

如果系统中有mbedtls，可以在编译配置中启用：

```kconfig
# Kconfig或.config
CONFIG_MBEDTLS=y
```

如果没有mbedtls，代码会使用占位函数（返回全0），建议集成一个MD5实现。

### 5. 查看状态

```c
// 检查是否正在录音
bool recording = app_usb_cdc_is_recording();

// 获取已发送的字节数
uint32_t bytes = app_usb_cdc_get_total_bytes();

// 获取当前序列号
uint32_t seq = app_usb_cdc_get_seq_num();

LISA_LOGI("APP", "Recording=%d, Bytes=%lu, Seq=%lu",
          recording, bytes, seq);
```

## 协议详解

### 基本帧格式

```
+--------+--------+--------+--------+--------+--------+
| Magic  | Type   |    Length       |  Data  | Check  |
| 2 Bytes| 1 Byte |    4 Bytes      | N Bytes| 1 Byte |
+--------+--------+--------+--------+--------+--------+
  0xAA55    0xXX    Little Endian      ...     XOR
```

### 数据类型

- `0x01` - 命令请求（PC → 设备）
- `0x02` - 命令响应（设备 → PC）
- `0x03` - 音频数据（设备 → PC）
- `0x04` - MD5数据（设备 → PC）

### 命令码

- `0x01` - 开始录音
- `0x02` - 停止录音
- `0x03` - 查询状态

详细协议说明请参考：[usb_cdc_protocol.md](usb_cdc_protocol.md)

## 常见问题

### Q1: 串口打开失败

**错误：** `串口错误: [Errno 13] Permission denied: '/dev/ttyACM0'`

**解决方法：**
```bash
# 临时解决：添加当前用户到dialout组
sudo usermod -a -G dialout $USER
# 注销并重新登录

# 或者临时使用sudo（不推荐）
sudo python3 tools/audio/serial_capture.py -p /dev/ttyACM0 -b 115200 -o output.pcm
```

### Q2: MD5校验失败

**可能原因：**
1. 设备端没有正确实现MD5计算（使用的是占位函数）
2. 传输过程中有数据丢失
3. 序列号跳变

**解决方法：**
1. 在设备端集成完整的MD5库（mbedtls或其他）
2. 检查是否有序列号跳变的警告
3. 尝试降低波特率或使用更短的传输距离

### Q3: 数据接收不完整

**症状：** 序列号跳变警告

**解决方法：**
1. 检查USB连接是否稳定
2. 降低音频数据发送速率
3. 增大设备端的队列长度（`CDC_AUDIO_QUEUE_LENGTH`）
4. 检查PC端CPU占用率

### Q4: 设备无响应

**检查步骤：**
```bash
# 1. 确认设备已连接
ls -l /dev/ttyACM*

# 2. 检查设备是否可读写
cat /dev/ttyACM0
# 应该能看到一些数据（如果设备在发送）

# 3. 尝试手动发送命令（用于调试）
echo -ne '\xAA\x55\x01\x01\x00\x00\x00\x01\xFA' > /dev/ttyACM0
```

### Q5: 如何调试协议

**PC端调试：**

在脚本中添加调试输出：

```python
# 在parse_byte函数中添加
print(f"State: {self.state}, Byte: 0x{byte:02X}")
```

**设备端调试：**

```c
// 启用协议日志
#define CDC_PROTO_DEBUG 1

// 在关键位置添加日志
LISA_LOGI(TAG, "Frame type: 0x%02X, length: %lu",
          frame->header.type, frame->header.length);
```

## 性能参数

### 典型配置

| 参数 | 值 | 说明 |
|------|-----|------|
| 波特率 | 115200 | 标准USB CDC速度 |
| 音频帧大小 | 1024-4096字节 | 推荐2048字节 |
| 队列长度 | 50 | 可根据内存调整 |
| 最大数据长度 | 4096字节 | 单帧最大负载 |

### 理论吞吐量

```
波特率: 115200 bps
有效数据率: ~11.5 KB/s (考虑协议开销)

单帧开销: 8字节（帧头+校验）+ 4字节（序列号）= 12字节
数据负载: 2048字节
帧效率: 2048 / (2048 + 12) ≈ 99.4%

支持音频格式:
- 16kHz, 16bit, Mono: 32 KB/s (需要更高波特率)
- 8kHz, 16bit, Mono: 16 KB/s (可支持)
```

**建议：** 对于高采样率音频（如16kHz以上），考虑使用更高的波特率或USB bulk传输模式。

## 扩展开发

### 添加新命令

1. **在协议头文件中定义命令码：**
   ```c
   // cdc_protocol.h
   typedef enum {
       CDC_CMD_START_RECORD    = 0x01,
       CDC_CMD_STOP_RECORD     = 0x02,
       CDC_CMD_QUERY_STATUS    = 0x03,
       CDC_CMD_SET_GAIN        = 0x04,  // 新命令：设置增益
   } cdc_cmd_id_t;
   ```

2. **在设备端实现命令处理：**
   ```c
   // app_usb_cdc.c
   case CDC_CMD_SET_GAIN:
       if (frame->header.length >= 2) {
           uint8_t gain = cmd->params[0];
           // 处理增益设置
           set_audio_gain(gain);
           response_len = cdc_proto_build_cmd_response(...);
       }
       break;
   ```

3. **在PC端脚本中添加命令：**
   ```python
   # serial_capture.py
   class CommandID(IntEnum):
       START_RECORD = 0x01
       STOP_RECORD = 0x02
       QUERY_STATUS = 0x03
       SET_GAIN = 0x04  # 新命令

   # 添加发送方法
   def set_gain(self, gain_value):
       params = bytes([gain_value])
       return self.send_command(CommandID.SET_GAIN, params)
   ```

## 参考资料

- [USB CDC协议规范](usb_cdc_protocol.md) - 完整的协议文档
- [TinyUSB文档](https://docs.tinyusb.org/) - USB协议栈参考
- [Python pyserial文档](https://pyserial.readthedocs.io/) - 串口通信库

## 技术支持

如有问题或建议，请：
1. 查阅协议文档：`docs/usb_cdc_protocol.md`
2. 查看代码注释：头文件中有详细的API说明
3. 联系开发团队

---

**版本：** 1.0
**更新日期：** 2026-01-07
**作者：** LISTENAI开发团队
