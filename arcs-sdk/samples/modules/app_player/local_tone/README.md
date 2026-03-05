# 本地提示音播放示例

## 功能说明

演示如何使用 app_player 播放本地 Flash 提示音，包括播放控制（播放、暂停、恢复、停止）和事件回调处理。

## 硬件连接

- **PA27**: PA（功放）控制引脚，用于控制音频功放的开关

## 前置准备

在运行本示例前，需要先将音频文件打包并烧录到设备 Flash：

### 1. 打包音频文件

使用 `tools/tone_tool` 工具将 MP3 文件打包成 tone.bin：

```bash
cd tools/tone_tool

# 将音频文件放入 ring/ 目录（使用数字前缀命名）
# 例如：000_greeting.mp3, 001_network_suc.mp3

# 执行打包脚本
./run.sh

# 生成 ring/tone.bin 和 ring/tone.h
```

详细使用方法请参考：[tools/tone_tool/README.md](../../../../tools/tone_tool/README.md)

### 2. 烧录 tone.bin

将生成的 `tone.bin` 烧录到设备 Flash 的 `0x30100000` 地址。

### 3. 复制 tone.h

将生成的 `tone.h` 复制到示例的 `src/` 目录，用于引用音频 ID。

## 示例内容

1. 初始化本地提示音系统（指定 Flash 地址 `0x30100000`）
2. 配置 PA 控制 GPIO
3. 初始化 app_player 模块并创建播放器实例
4. 注册播放事件回调函数
5. 播放本地提示音（`TONE_ID_2`）
6. 演示播放控制：
   - 播放 1 秒后暂停
   - 暂停 1 秒后恢复播放
   - 恢复 1 秒后停止播放

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 烧录

使用以下命令烧录固件和提示音：

```bash
./tools/burn/cskburn -s /dev/ttyUSB0 -b 3000000 0x0 build/arcs.bin -C arcs
./tools/burn/cskburn -s /dev/ttyUSB0 -b 3000000 0x10000 samples/modules/app_player/local_tone/src/tone.bin -C arcs

```

## 预期输出

```
I/APP_PLAYER      [1034:42:46.206 1 main] Resume: tone
I/samples         [1034:42:46.206 1 main] PA ON
I/PA_MGR          [1034:42:46.206 1 main] PA ON (immediate)
I/APP_PLAYER      [1034:42:46.208 1 player_td] Player[0] tone evt=2
I/samples         [1034:42:46.209 1 player_td] PA ON
I/PA_MGR          [1034:42:46.209 1 player_td] PA ON (immediate)
I/samples         [1034:42:46.215 1 cb_tone] Player playing
```

**音频输出**:
- 播放指定的本地提示音（对应 `TONE_ID_2`）
- 播放过程中可听到暂停和恢复的效果

## 核心 API

| API | 说明 |
|-----|------|
| `app_tone_init()` | 初始化本地提示音系统，指定 Flash 地址 |
| `app_tone_get_url()` | 根据音频 ID 获取播放 URL |
| `app_player_init()` | 初始化 app_player 模块 |
| `app_player_create()` | 创建播放器实例 |
| `app_player_register_callback()` | 注册事件回调函数 |
| `app_player_play()` | 播放指定 URL 的音频 |
| `app_player_pause()` | 暂停播放 |
| `app_player_resume()` | 恢复播放 |
| `app_player_stop_sync()` | 同步停止播放（等待停止完成）|
| `app_player_destroy()` | 销毁播放器实例 |

## 关键代码

### 初始化本地提示音

```c
/* 初始化本地提示音文件（指定 Flash 地址）*/
app_tone_init(0x30100000);
```

### PA 控制回调

```c
/**
 * @brief PA控制回调函数
 */
static int pa_control_callback(int onoff)
{
    LOGI("PA %s", onoff ? "ON" : "OFF");
    return lisa_gpio_write_pin(lisa_device_get(PA_GPIO_DEVICE),
                               PA_PIN_NUM,
                               onoff ? LISA_GPIO_HIGH : LISA_GPIO_LOW);
}
```

### 播放本地提示音

```c
/* 获取提示音 URL */
const char *url = app_tone_get_url(TONE_ID_2);
LOGI("Playing URL: %s", url);

/* 播放提示音 */
ret = app_player_play(player, url);
if (ret != APP_PLAYER_OK) {
    LOGE("Failed to play: %d", ret);
    app_player_destroy(player);
    return -1;
}
```

### 播放控制

```c
/* 暂停播放 */
app_player_pause(player);

/* 恢复播放 */
app_player_resume(player);

/* 同步停止（等待停止完成）*/
app_player_stop_sync(player);
```

### 事件回调

```c
static void player_event_callback(app_player_t *player,
                                  app_player_event_t event,
                                  void *user_data)
{
    switch (event) {
    case APP_PLAYER_EVENT_PREPARED:
        LOGI("Player prepared");
        break;
    case APP_PLAYER_EVENT_PLAYING:
        LOGI("Player playing");
        break;
    case APP_PLAYER_EVENT_COMPLETED:
        LOGI("Player completed");
        break;
    case APP_PLAYER_EVENT_ERROR:
        LOGE("Player error");
        break;
    case APP_PLAYER_EVENT_STOPPED:
        LOGI("Player stopped");
        break;
    }
}
```

## 配置说明

### Flash 地址配置

本地提示音的 Flash 地址在代码中配置：

```c
app_tone_init(0x30100000);  // Flash 地址
```

确保 `tone.bin` 烧录到相同的地址。

### PA 控制引脚

PA 控制引脚在代码中定义：

```c
#define PA_PIN_NUM 27        // PA 控制引脚号
#define PA_GPIO_DEVICE "gpioa"  // GPIO 设备名
```

根据硬件设计修改对应的引脚和设备。

## 注意事项

1. **Flash 地址一致性**: `app_tone_init()` 中的 Flash 地址必须与 `tone.bin` 的烧录地址一致
2. **tone.h 文件**: 需要将打包工具生成的 `tone.h` 复制到 `src/` 目录，否则编译时会找不到 `TONE_ID_*` 定义
3. **PA 回调必填**: `app_player_init()` 时必须提供 `pa_ctrl_callback`，用于控制功放开关
4. **返回值检查**: 所有 API 调用后务必检查返回值，确保操作成功
5. **资源清理**: 使用完毕后记得调用 `app_player_destroy()` 释放资源
6. **音频格式**: 本地提示音建议使用 16kHz、16bit 的 MP3 格式，以获得最佳性能
