# 本地提示音使用指南

本地提示音功能允许将音频文件打包到 Flash 中，实现快速离线播放。

## 功能概述

本地提示音系统提供：
- 将多个音频文件打包成单个二进制文件
- 烧录到 Flash 后快速访问
- 无需网络或文件系统，启动即可播放
- 适合系统提示音、语音提示等场景

## 使用流程

### 1. 准备音频文件

音频文件需满足以下格式要求：
- **位深度**: 16 bit（硬件固定）
- **采样率**: 16 kHz（推荐，其他采样率会自动重采样）
- **格式**: 支持 PCM、MP3、WAV、AAC、M4A

### 2. 打包音频文件

使用 `tools/tone_tool` 工具将音频文件打包成二进制文件：

```bash
cd tools/tone_tool

# 将音频文件放入 ring/ 目录，使用数字前缀命名
# 例如：000_greeting.mp3, 001_network_suc.mp3

# 执行打包脚本
./run.sh
```

打包工具会生成：
- `ring/tone.bin` - 音频二进制数据（需烧录到 Flash）
- `ring/tone.h` - C 头文件，包含音频 ID 枚举定义（如 `TONE_ID_0`, `TONE_ID_1`）

详细使用方法请参考：`tools/tone_tool/README.md`

### 3. 烧录到设备

将生成的 `tone.bin` 烧录到设备 Flash 的指定地址（例如 `0x30100000`）。

**注意：** 烧录地址需要根据实际硬件配置确定，确保不与其他数据冲突。

### 4. 代码中使用

```c
#include "app_tone.h"
#include "tone.h"  // 包含工具生成的头文件

// 初始化本地提示音（指定 Flash 地址）
app_tone_init(0x30100000);

// 获取提示音 URL（使用工具生成的 TONE_ID）
const char *url = app_tone_get_url(TONE_ID_0);  // 对应 000_greeting.mp3

// 播放提示音
app_player_play(player, url);
```

## 音频 ID 说明

工具会按音频文件名排序生成对应的枚举 ID：
- `000_greeting.mp3` → `TONE_ID_0`
- `001_network_suc.mp3` → `TONE_ID_1`
- `002_network_fail.mp3` → `TONE_ID_2`
- ...

**命名规则：**
- 文件名必须以三位数字开头（000-999）
- 数字决定生成的 ID 顺序
- 数字后可跟任意描述性文本

## 完整示例

### 准备音频文件

```bash
# 音频文件列表
ring/
├── 000_boot_up.mp3        # 开机提示音
├── 001_wifi_connected.mp3 # WiFi 连接成功
├── 002_wifi_failed.mp3    # WiFi 连接失败
└── 003_shutdown.mp3       # 关机提示音
```

### 打包并烧录

```bash
cd tools/tone_tool
./run.sh

# 生成文件：
# ring/tone.bin - 烧录到 Flash
# ring/tone.h   - 复制到项目中
```

### 代码集成

```c
#include "app_player.h"
#include "app_tone.h"
#include "tone.h"

void tone_example(void) {
    // 初始化提示音系统
    app_tone_init(0x30100000);

    // 创建播放器
    app_player_t *player = app_player_create("tone");

    // 播放开机提示音
    const char *boot_url = app_tone_get_url(TONE_ID_0);
    app_player_play(player, boot_url);

    // 等待播放完成...

    // 播放 WiFi 连接成功提示音
    const char *wifi_ok_url = app_tone_get_url(TONE_ID_1);
    app_player_play(player, wifi_ok_url);
}
```

## 注意事项

1. **Flash 地址规划**：
   - 确保烧录地址不与其他数据（如固件、配置）冲突
   - 建议在分区表中明确预留提示音区域

2. **音频文件大小**：
   - Flash 空间有限，注意控制音频文件总大小
   - 建议使用 MP3 格式压缩音频

3. **初始化时机**：
   - 必须在播放提示音前调用 `app_tone_init()`
   - 通常在系统初始化阶段调用一次即可

4. **URL 格式**：
   - `app_tone_get_url()` 返回的 URL 格式为 `tone://ID`
   - 可以直接传递给 `app_player_play()`

