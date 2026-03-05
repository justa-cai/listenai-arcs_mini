# 网络音频播放示例

## 功能说明

演示如何使用 app_player 播放网络音频资源（HTTP/HTTPS），包括播放控制（播放、暂停、恢复、停止）和事件回调处理。

## 硬件连接

- **PA27**: PA（功放）控制引脚，用于控制音频功放的开关
- **WiFi**: 需要设备支持 WiFi 并连接到互联网

## 前置准备

### 网络配置

本示例需要设备连接到互联网才能播放网络音频。请根据实际情况配置 WiFi 连接参数：

1. 编辑 `src/net/net_connect.c` 或相关网络配置文件
2. 配置 WiFi SSID 和密码
3. 确保网络连接成功后再播放音频

## 示例内容

1. 连接网络（WiFi）
2. 配置 PA 控制 GPIO
3. 初始化 app_player 模块并创建播放器实例
4. 注册播放事件回调函数
5. 播放网络音频（HTTPS URL）
6. 演示播放控制：
   - 播放 1 秒后暂停
   - 暂停 1 秒后恢复播放
   - 恢复 1 秒后停止播放

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 预期输出

```
[I][samples] Connecting to network...
[I][samples] Network connected
[I][samples] PA OFF
[I][samples] Playing URL: https://listenai-firmware-delivery.oss-cn-beijing.aliyuncs.com/weather.mp3
[I][samples] PA ON
[I][samples] Player prepared
[I][samples] Player playing
[I][samples] Player paused
[I][samples] Player playing
[I][samples] Player stopped
[I][samples] PA OFF
```

**音频输出**:
- 播放指定 URL 的网络音频
- 播放过程中可听到暂停和恢复的效果

## 核心 API

| API | 说明 |
|-----|------|
| `app_player_init()` | 初始化 app_player 模块 |
| `app_player_create()` | 创建播放器实例 |
| `app_player_register_callback()` | 注册事件回调函数 |
| `app_player_play()` | 播放指定 URL 的音频 |
| `app_player_pause()` | 暂停播放 |
| `app_player_resume()` | 恢复播放 |
| `app_player_stop_sync()` | 同步停止播放（等待停止完成）|
| `app_player_destroy()` | 销毁播放器实例 |

## 关键代码

### 初始化和配置

```c
/* 连接网络 */
net_connect();

/* 初始化 app_player 模块 */
app_player_config_t app_config = {
    .pa_ctrl_callback = pa_control_callback
};
ret = app_player_init(&app_config);
if (ret != APP_PLAYER_OK) {
    LOGE("App player init failed: %d", ret);
    return -1;
}

/* 创建播放器实例 */
app_player_t *player = app_player_create("tts");
if (player == NULL) {
    LOGE("Failed to create player");
    return -1;
}

/* 注册事件回调 */
app_player_register_callback(player, player_event_callback, NULL);
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

### 播放网络音频

```c
/* 播放网络音频资源 */
const char *url = "https://listenai-firmware-delivery.oss-cn-beijing.aliyuncs.com/weather.mp3";
LOGI("Playing URL: %s", url);

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
    case APP_PLAYER_EVENT_PAUSED:
        LOGI("Player paused");
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

### PA 控制引脚

PA 控制引脚在代码中定义：

```c
#define PA_PIN_NUM 27           // PA 控制引脚号
#define PA_GPIO_DEVICE "gpioa"  // GPIO 设备名
```

根据硬件设计修改对应的引脚和设备。

### 网络音频 URL

示例中使用的音频 URL：

```c
const char *url = "https://listenai-firmware-delivery.oss-cn-beijing.aliyuncs.com/weather.mp3";
```

可以替换为其他支持的音频格式 URL（PCM、MP3、WAV、AAC、M4A）。

## 注意事项

1. **网络连接**: 必须先确保设备成功连接到网络，否则无法播放网络音频
2. **URL 格式**: 支持 HTTP 和 HTTPS 协议的音频 URL
3. **音频格式**: 支持 PCM、MP3、WAV、AAC、M4A 格式，推荐使用 16kHz、16bit 的音频
4. **PA 回调必填**: `app_player_init()` 时必须提供 `pa_ctrl_callback`，用于控制功放开关
5. **返回值检查**: 所有 API 调用后务必检查返回值，确保操作成功
6. **网络稳定性**: 网络不稳定可能导致播放中断或失败，建议添加重试机制
7. **资源清理**: 使用完毕后记得调用 `app_player_destroy()` 释放资源
8. **HTTPS 支持**: 播放 HTTPS 音频需要确保系统支持 TLS/SSL

### WiFi 连接失败

**问题描述**：示例程序无法连接到 WiFi 网络，或者网络连接超时。

**解决方法**：

本示例需要联网才能播放网络音频和在线 TTS，请按以下步骤配置 WiFi：

1. 打开 [src/net/net_connect.c](src/net/net_connect.c#L28-L29) 文件

2. 修改WiFi配置为你的实际网络信息：

```c
#define TARGET_WIFI_SSID "your_wifi_ssid"      // 替换为你的 WiFi 名称
#define TARGET_WIFI_PWD "your_wifi_password"   // 替换为你的 WiFi 密码