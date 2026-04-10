# 测试音频资源文件

本目录包含用于app_player音频焦点管理测试的音频文件。

## 文件列表

| 文件名 | 大小 | 用途 | 烧录地址 |
|--------|------|------|----------|
| 001_network_suc.mp3 | 4716 字节 | TONE 播放器测试音频 | 0x30200000 |
| 000_geeting.mp3 | 2772 字节 | 备用测试音频 | - |

## 测试前准备

### 烧录音频文件到 Flash

测试代码使用 `mem://` 协议从 Flash 读取音频数据，因此运行测试前必须先烧录音频文件。

**烧录步骤：**
1. 使用 CSK Burner 或其他烧录工具
2. 选择文件：`001_network_suc.mp3`
3. 设置地址：`0x30200000`
4. 执行烧录

### 配置信息

测试代码配置（`test_common.c`）：

```c
#define TONE_AUDIO_FLASH_ADDR   0x30200000  /* Flash烧录地址 */
#define TONE_AUDIO_SIZE         4716        /* 文件实际大小 */
```

### 验证文件大小

```bash
# Linux/macOS
stat -c%s 001_network_suc.mp3
# 输出应为：4716

# macOS
stat -f%z 001_network_suc.mp3
```

## 更换测试音频

如果需要使用其他音频文件进行测试：

1. 将新音频文件放到本目录
2. 获取文件大小（字节数）
3. 烧录到 Flash
4. 修改 `test_common.c` 中的配置：
   ```c
   #define TONE_AUDIO_FLASH_ADDR   <新地址>
   #define TONE_AUDIO_SIZE         <新大小>
   ```

## 音频格式要求

- **格式**：MP3、WAV、AAC、M4A、PCM
- **采样率**：16 kHz（推荐）
- **位深度**：16 bit
- **声道**：单声道（推荐）

## 注意事项

- 文件大小必须与 `test_common.c` 中的 `TONE_AUDIO_SIZE` 一致
- 烧录地址必须与 `test_common.c` 中的 `TONE_AUDIO_FLASH_ADDR` 一致
- 确保烧录地址不与固件或其他数据冲突
- 测试失败时，首先检查音频文件是否正确烧录
