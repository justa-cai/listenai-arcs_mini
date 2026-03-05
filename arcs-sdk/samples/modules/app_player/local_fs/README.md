# 本地文件系统音频播放示例

本示例演示如何使用 `app_player` 模块从本地文件系统（FAT32 SD 卡）播放音频文件。

## 功能说明

示例展示了以下功能：

1. 初始化 SD 卡并挂载 FAT32 文件系统
2. 创建播放器实例并注册事件回调
3. 播放本地音频文件（MP3 格式）
4. 控制播放流程：播放 → 暂停 → 恢复 → 停止

## 文件系统说明

- **文件系统类型**：FAT32
- **挂载位置**：SD 卡（通过 SDIO 接口）
- **挂载点**：`/SD:`

## 准备工作

### 1. 准备音频文件

音频文件位于 `res/audio_file/` 目录下：

```
res/audio_file/
├── 000_geeting.mp3
└── 001_network_suc.mp3
```

### 2. 生成文件系统镜像

使用 `tools/fatfs_package` 工具将音频文件打包为 FAT32 镜像：

```bash
# 进入工具目录
cd tools/fatfs_package

# 生成镜像文件（示例：32MB 大小）
./tools/fatfs_package/mkfatfs.py -o fatfs.img -s 32M -d samples/modules/app_player/local_fs/res/audio_file -l SD
```

参数说明：
- `-o fatfs.img`：输出镜像文件名
- `-s 32M`：镜像大小（根据音频文件总大小调整）
- `-d <目录>`：源文件目录路径
- `-l SD`：卷标名称

详细使用方法请参考：tools/fatfs_package/README.md

### 3. 编译工程

```{eval-rst}
.. include:: /sample_build.rst
```

### 4. 烧录固件和文件系统

#### 步骤 1：烧录应用程序固件

```bash
# 烧录应用固件到 Flash
./tools/burn/cskburn -s /dev/ttyACM0 -b 3000000 0x0 build/arcs.bin -C arcs
```

#### 步骤 2：烧录文件系统镜像到 SD 卡（T卡）

**重要**：文件系统镜像需要烧录到 eMMC/T卡，而不是 Flash。

```bash
# 烧录 FAT32 镜像到 T卡（通过 SDIO 接口）
./tools/burn/cskburn -s /dev/ttyACM0 -b 3000000 --emmc 0x0 fatfs.img -C arcs
```

**注意事项**：
- 必须使用 `--emmc` 参数指定烧录到 T卡
- 确保 T卡已正确插入开发板的卡槽
- SDIO 接口使用引脚 PA4~PA9

## 运行示例

烧录完成后，重启开发板，示例程序将自动运行：

1. 初始化并挂载 SD 卡文件系统
2. 创建播放器实例
3. 播放 `/SD:/001_network_suc.mp3` 文件
4. 演示暂停、恢复、停止等控制操作

## 日志输出

示例运行时会输出以下日志：

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

## API 使用示例

### 初始化播放器

```c
app_player_config_t config = {
    .pa_ctrl_callback = pa_control_callback
};
app_player_init(&config);
```

### 创建播放器实例

```c
app_player_t *player = app_player_create("tone");
```

### 注册事件回调

```c
app_player_register_callback(player, player_event_callback, NULL);
```

### 播放控制

```c
// 播放文件
app_player_play(player, "/SD:/001_network_suc.mp3");

// 暂停
app_player_pause(player);

// 恢复播放
app_player_resume(player);

// 停止（同步）
app_player_stop_sync(player);
```

## 常见问题

### 1. 挂载文件系统失败

**原因**：
- T卡未正确插入或接触不良
- 文件系统镜像未烧录或烧录地址错误
- T卡格式不是 FAT32

**解决方案**：
- 检查 T卡是否正确插入卡槽
- 确认使用 `--emmc` 参数烧录镜像到 T卡
- 重新生成并烧录文件系统镜像

### 2. 播放失败

**原因**：
- 音频文件路径错误
- 文件不存在或损坏
- 音频格式不支持

**解决方案**：
- 确认音频文件路径正确（`/SD:/文件名`）
- 检查镜像中是否包含该音频文件
- 使用支持的音频格式（MP3、WAV 等）

### 3. PA 控制无效

**原因**：
- GPIO 配置错误
- PA 硬件连接问题

**解决方案**：
- 检查 `PA_PIN_NUM` 和 `PA_GPIO_DEVICE` 配置
- 确认硬件 PA 电路连接正确

