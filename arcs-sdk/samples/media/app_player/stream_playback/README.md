# app_player 流式播放示例

## 功能说明

演示如何使用 `app_player` 组件的流式播放接口来播放 PCM 音频数据，适用于实时音频合成、网络音频流播放等场景。

## 硬件连接

- **PA27**: PA（功放）控制引脚，用于控制音频功放的开关

## 示例内容

1. 初始化 `app_player` 模块并创建播放器实例
2. 注册播放事件回调函数
3. 调用 `app_player_play_stream()` 开启流式播放，指定音频格式参数
4. 循环调用 `app_player_write_stream()` 逐块写入 PCM 数据
5. 调用 `app_player_finish_stream()` 通知数据写入完毕
6. 等待播放完成事件回调

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
[I][samples] PA OFF
[I][samples] Playing stream: 16000 Hz, 1 ch, 16 bit
[I][samples] PA ON
[I][samples] Player prepared
[I][samples] Player playing
[I][samples] Player completed
[I][samples] PA OFF
```

## 核心 API

| API | 说明 |
|-----|------|
| `app_player_init()` | 初始化 app_player 模块 |
| `app_player_create()` | 创建播放器实例 |
| `app_player_register_callback()` | 注册播放事件回调函数 |
| `app_player_play_stream()` | 开始流式播放（指定音频格式） |
| `app_player_write_stream()` | 写入 PCM 数据（可多次调用） |
| `app_player_finish_stream()` | 结束流式播放（通知数据写入完毕） |
| `app_player_destroy()` | 销毁播放器实例 |

## 关键代码

```c
// 初始化
app_player_config_t config = { .pa_ctrl_callback = pa_ctrl_callback };
app_player_init(&config);

// 创建播放器
app_player_t *player = app_player_create("stream_demo");
app_player_register_callback(player, event_callback, NULL);

// 流式播放
app_player_play_stream(player, 16000, 1, 16);   // 开始：16kHz, 单声道, 16位
app_player_write_stream(player, pcm_data, size, 1000);  // 写入数据
app_player_finish_stream(player);               // 结束

// 清理
app_player_destroy(player);
```

## 注意事项

1. **播放顺序**: 必须先调用 `app_player_play_stream()` 开启流式播放模式
2. **超时设置**: 数据写入时需注意超时设置，避免阻塞过长
3. **缓冲区处理**: 调用 `app_player_finish_stream()` 后会自动播放完剩余缓冲区数据
4. **完成通知**: 可通过事件回调监听 `APP_PLAYER_EVENT_COMPLETED` 获知播放完成
5. **PA 回调必填**: `app_player_init()` 时必须提供 `pa_ctrl_callback`，用于控制功放开关
