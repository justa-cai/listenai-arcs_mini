# Tone 本地音频打包工具

本工具用于生成 app_player 的本地音频二进制文件，将多个音频文件打包成 `tone.bin` 和 `tone.h` 文件，供嵌入式设备使用。

## 功能说明

该工具可以：
1. 处理音频文件（裁剪静音、格式转换）
2. 将多个音频文件打包成二进制文件 `tone.bin`
3. 生成头文件 `tone.h`，包含音频 ID 枚举定义

## 音频文件要求

原始音频文件需满足以下格式要求：
- **采样率**: 16 kHz
- **位深度**: 16 bit
- **格式**: MP3 或 PCM
- **存放位置**: [ring/](ring/) 目录下

## 使用方法

### 快速使用

将符合要求的音频文件放入 [ring/](ring/) 目录，然后执行：

```shell
./run.sh
```

执行后会在 [ring/](ring/) 目录下生成：
- `tone.bin` - 音频二进制数据文件
- `tone.h` - C 头文件，包含音频 ID 定义

### 高级用法

使用 [proc.sh](proc.sh) 脚本可以进行更精细的控制：

```shell
# 1. 仅打包路径下的音频文件（不处理静音）
./proc.sh -p ./ring

# 2. 处理并打包路径下的音频文件（裁剪静音 + 打包）
./proc.sh -d ./ring

# 3. 处理指定的单个音频文件
./proc.sh -f ./ring/test.mp3
```

参数说明：
- `-p, --pack <目录>` - 仅打包目录下的音频文件
- `-d, --dir <目录>` - 处理并打包目录下的所有音频文件
- `-f, --file <文件>` - 处理指定的单个音频文件
- `-h, --help` - 显示帮助信息

## 音频命名规范

音频文件名应使用数字前缀进行排序，例如：
```
000_greeting.mp3
001_network_suc.mp3
002_network_fail.mp3
003_no_intention.mp3
...
```

工具会按文件名排序后生成对应的 `TONE_ID_0`, `TONE_ID_1` 等枚举值。

## 生成文件说明

### tone.h

头文件包含音频 ID 枚举定义，示例：

```c
#ifndef __LISTENAI_TONE_PACKAGE_H__
#define __LISTENAI_TONE_PACKAGE_H__

enum {
    TONE_ID_0 = 0,  // 000_greeting.mp3
    TONE_ID_1 = 1,  // 001_network_suc.mp3
    TONE_ID_2 = 2,  // 002_network_fail.mp3
    ...
};

#endif
```

### tone.bin

二进制打包文件，包含所有音频数据，需要烧录到设备的 Flash 中。

## 代码使用示例

在代码中通过 `app_tone_get_url()` 函数获取本地音频 URL：

```c
#include "app_tone.h"
#include "ring/tone.h"

// 初始化音频系统
app_tone_default_init();

// 获取音频 URL
char *url = app_tone_get_url(TONE_ID_0);

// 使用 URL 播放音频
// ...
```

更多 API 说明请参考 [components/app_player/tone/app_tone.h](../../components/app_player/tone/app_tone.h)。

## 配置说明

工具的配置文件为 [config.sh](config.sh)，可以配置：

### 工具路径配置
- `FFMPEG_EXE` - FFmpeg 工具路径（默认：`cmd/ffmpeg-64`）
- `TONETOOL_EXE` - 音频打包工具路径（默认：`cmd/tone_tool`）
- `ID3TAG_EXE` - ID3 标签移除工具（默认：`id3v2`，需安装）

### 静音裁剪配置
- `MUTE_THRESHOLD` - 静音检测阈值（默认：`-50dB`）
- `MUTE_SAVE_PREFIX_TIME` - 音频开头保留的静音时长（默认：`0.01` 秒）
- `MUTE_SAVE_SUFFIX_TIME` - 音频结尾保留的静音时长（默认：`0.01` 秒）
- `MUTE_AUDIO_RATE` - 裁剪后的音频码率（默认：`24k`）

### 输出路径配置
- `PACK_TONE_OUT_BINARY` - 二进制文件输出路径（默认为空，生成在 ring 目录）
- `PACK_TONE_OUT_HEADER` - 头文件输出路径（默认为空，生成在 ring 目录）

## 环境依赖

- Linux 系统（x86-64 架构）
- `id3v2` 工具（用于移除 MP3 的 ID3 标签）

安装依赖：
```shell
sudo apt-get install id3v2
```

## 工作流程

1. 将音频文件复制到处理
2. 使用 FFmpeg 裁剪音频首尾静音
3. 转换音频格式为 16kHz, 单声道, MP3
4. 移除 MP3 的 ID3 标签
5. 使用 tone_tool 打包所有音频为 tone.bin
6. 生成包含音频 ID 枚举的 tone.h 头文件

## 注意事项

- 音频文件名中的数字前缀决定了音频 ID 的顺序
- 如需支持其他平台，请从 [FFmpeg官网](https://ffbinaries.com/downloads) 下载对应平台的 FFmpeg 工具，并修改 [config.sh](config.sh) 中的 `FFMPEG_EXE` 配置
- 生成的 `tone.bin` 需要烧录到设备 Flash 的指定位置，具体地址由应用代码中的 `app_tone_init()` 参数决定
