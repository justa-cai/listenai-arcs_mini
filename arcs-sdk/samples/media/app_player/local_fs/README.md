# app_player 本地文件播放示例

## 功能说明

演示如何使用 `app_player` 模块从本地文件系统（FAT32 SD 卡）播放音频文件，包括文件系统挂载、播放控制和事件回调处理。

## 硬件连接

- **PA27**: PA（功放）控制引脚，用于控制音频功放的开关
- **SDMMC0 接口**: 连接 SD 卡/eMMC，用于存储音频文件

使用 `tools/fatfs_package` 工具将音频文件打包为 FAT32 镜像：

```bash
# 进入工具目录
cd tools/fatfs_package

# 生成镜像文件（示例：32MB 大小）
./tools/fatfs_package/mkfatfs.py -o fatfs.img -s 32M -d samples/media/app_player/local_fs/res/audio_file -l SD
```

## 示例步骤

1. 初始化 SD 卡并挂载 FAT32 文件系统到 `/SD:` 挂载点
2. 初始化 `app_player` 模块并配置 PA 控制回调
3. 创建播放器实例并注册事件回调
4. 播放本地 MP3 音频文件 `/SD:/001_network_suc.mp3`
5. 演示播放控制：播放、暂停、恢复、停止
6. 销毁播放器实例

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 烧录

```{eval-rst}
.. include:: /sample_flash.rst
```

## 预期输出

```
[app_player_sample] App Player Local Filesystem Sample
[app_player_sample] Starting playback demo...
[app_player_sample] Player prepared
[app_player_sample] Player playing
[app_player_sample] PA ON
[app_player_sample] Player stopped
[app_player_sample] PA OFF
[app_player_sample] Playback demo completed
```

## 核心 API

| API | 说明 |
|-----|------|
| `app_player_init()` | 初始化 app_player 模块 |
| `app_player_create()` | 创建播放器实例 |
| `app_player_register_callback()` | 注册播放事件回调函数 |
| `app_player_play()` | 播放指定路径的音频文件 |
| `app_player_pause()` | 暂停播放 |
| `app_player_resume()` | 恢复播放 |
| `app_player_stop()` | 停止播放（同步接口） |
| `app_player_destroy()` | 销毁播放器实例 |

## 关键代码

```c
/* 初始化播放器 */
app_player_config_t config = {
    .pa_ctrl_callback = pa_control_callback
};
app_player_init(&config);

/* 创建播放器实例 */
app_player_t *player = app_player_create("tone");
app_player_register_callback(player, player_event_callback, NULL);

/* 播放本地文件 */
app_player_play(player, "/SD:/001_network_suc.mp3");

/* 播放控制 */
app_player_pause(player);
app_player_resume(player);
app_player_stop(player);
```

## 注意事项

1. **文件系统镜像**: 运行前需使用 `mkfatfs.py` 工具将音频文件打包为 FAT32 镜像并烧录到 eMMC
2. **烧录顺序**: 先烧录文件系统镜像到 eMMC（使用 `--emmc` 参数），再烧录应用固件到 Flash
3. **文件路径格式**: 播放路径必须包含挂载点前缀，如 `/SD:/filename.mp3`
4. **PA 回调必填**: `app_player_init()` 时必须提供 `pa_ctrl_callback`，用于控制功放开关
5. **音频格式**: 支持 MP3、WAV 等格式，推荐 16kHz、16bit 的音频文件
