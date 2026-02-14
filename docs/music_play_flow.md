# 音乐播放流程分析

## 目录
- [1. 播放器架构](#1-播放器架构)
- [2. 音频焦点管理](#2-音频焦点管理)
- [3. 播放模式](#3-播放模式)
- [4. 音乐播放流程](#4-音乐播放流程)
- [5. 事件状态机](#5-事件状态机)
- [6. API 接口](#6-api-接口)

---

## 1. 播放器架构

### 1.1 双播放器设计

```
app_player (应用层播放器管理)
├── tone_player (提示音播放器)
│   ├── PLAYER_T_TONE
│   └── 用于: 提示音、系统音效
└── audio_player (音乐播放器)
    ├── PLAYER_T_CLOUD
    └── 用于: 云端音乐、音频内容
```

**关键代码**: `src/audio/app_player.c:43-48`

```c
typedef struct app_player_s {
    app_player_item_t tone_player;   // 提示音播放器
    app_player_item_t audio_player;  // 音乐播放器
} app_player_t;
```

### 1.2 音频数据结构

```c
// src/player/audio_out.h
typedef struct audio_out_s {
    char m_url[512];        // 音频URL
    char mid[128];         // 资源ID
    char m_name[32];       // 歌曲名称
    char m_artist[32];     // 歌手名称
    char m_all_rate[64];   // 评分/评价
    int throw_time;        // 抛出时间
} audio_out_t;
```

### 1.3 播放器项目结构

```c
// src/audio/app_player.c:24-41
typedef struct app_player_item_s {
    PLAYER_HANDLE hld;           // 播放器句柄 (LisaPlayer)
    uint32_t playid;            // 播放ID
    status_cb_map_t cb_map;      // 回调映射
    bool is_preparing;           // 准备中标志
    bool wait_prepare_intercepted; // 等待准备拦截标志
    bool pause_preparing;        // 准备时暂停标志
    lisa_semaphore_t *preparing_sem; // 准备信号量
    PlayerEvt evt;              // 播放器事件
} app_player_item_t;
```

---

## 2. 音频焦点管理

### 2.1 音频通道类型

```c
// src/player/listen_audiomgr.h:6
typedef enum {
    AIP = 0,      // AI语音通道 (优先级: 30)
    TTS,          // TTS语音通道 (优先级: 10)
    ALERT,        // 警报通道 (优先级: 40)
    CONTENT,      // 内容音乐通道 (优先级: 50)
    LOCAL,        // 本地音频通道 (优先级: 10)
    EXTRA         // 扩展通道 (优先级: 50)
} channel_type_e;
```

### 2.2 焦点状态

```c
// src/player/listen_audiomgr.h:7
typedef enum {
    FOREGROUND = 0,  // 前景 (获得音频输出)
    BACKGROUND,      // 背景 (被其他通道抢占)
    NONE            // 无焦点 (停止播放)
} focus_state_e;
```

### 2.3 焦点抢占规则

| 请求通道 | 可抢占的通道 | 原因 |
|---------|-------------|------|
| AIP (30) | TTS, LOCAL, ALERT | AI语音优先级高 |
| TTS (10) | ALERT | 仅可抢占警报 |
| ALERT (40) | TTS, LOCAL, AIP | 警报优先级高 |
| CONTENT (50) | LOCAL, ALERT, AIP, TTS, EXTRA | 内容音乐最高 |
| LOCAL (10) | 无 | 最低优先级 |
| EXTRA (50) | LOCAL, ALERT, AIP, TTS, CONTENT | 扩展通道最高 |

### 2.4 音频管理器结构

```c
// src/player/listen_audiomgr.h:20-31
typedef struct listen_audiomgr_s {
    channel_callback_cb *m_callbacks[6];  // 通道回调数组
    focus_state_t *m_aip;     // AI语音焦点
    focus_state_t *m_tts;     // TTS焦点
    focus_state_t *m_alert;   // 警报焦点
    focus_state_t *m_content; // 内容焦点
    focus_state_t *m_local;   // 本地焦点
    focus_state_t *m_extra;   // 扩展焦点
    focus_state_t *m_foreground_channel; // 前景通道
    focus_state_t *m_background_channel; // 背景通道
} listen_audiomgr_t;
```

---

## 3. 播放模式

### 3.1 播放模式枚举

```c
// src/player/play_mode.h:6-11
typedef enum {
    MODE_ORDER,   // 顺序播放 - 到末尾停止
    MODE_CYCLE,   // 列表循环 - 到末尾从头开始
    MODE_SINGLE,  // 单曲循环 - 重复当前歌曲
    MODE_RANDOM   // 随机播放 - 随机选择歌曲
} PLAY_MODE_E;
```

### 3.2 播放模式数据结构

```c
// src/player/play_mode.h:13-18
typedef struct play_mode_s {
    PLAY_MODE_E m_play_mode;   // 当前播放模式
    int m_curr_index;          // 当前播放索引
    audio_out_t *m_music_list; // 歌曲列表
    int m_music_size;         // 歌曲数量
} play_mode_t;
```

### 3.3 下一首/上一首逻辑

#### 下一首 (listen_next_audio)

```
MODE_ORDER (顺序)
├── curr_index >= size - 1 → 返回最后一首 (code=1)
└── 否则 → curr_index++ (code=0)

MODE_CYCLE (循环)
└── curr_index++
    └── 超过末尾 → 回到 0

MODE_SINGLE (单曲)
└── 保持当前索引不变

MODE_RANDOM (随机)
├── 生成随机索引
├── 与当前相同则重试 (最多3次)
└── 3次都相同 → curr_index++
```

#### 上一首 (listen_pre_audio)

```
MODE_ORDER (顺序)
├── curr_index <= 0 → 返回第一首 (code=1)
└── 否则 → curr_index-- (code=0)

MODE_CYCLE (循环)
└── curr_index--
    └── 小于0 → 跳到末尾

MODE_SINGLE (单曲)
└── 保持当前索引不变

MODE_RANDOM (随机)
├── 生成随机索引
└── 逻辑与下一首相同
```

---

## 4. 音乐播放流程

### 4.1 初始化流程

```
app_player_init() [src/audio/app_player.c:231]
├── 创建 tone_player
│   ├── lisa_player_create("toneplayer")
│   ├── 初始化回调映射 (mutex)
│   ├── 创建信号量
│   └── 设置回调 _tone_player_callback
└── 创建 audio_player
    ├── lisa_player_create("audioplayer")
    ├── 初始化回调映射 (mutex)
    ├── 创建信号量
    └── 设置回调 _audio_player_callback
```

### 4.2 播放流程

```
用户请求播放音乐
        │
        ▼
audio_player.on_directive(AUDIO_PLAY)
        │
        ├── 停止当前播放 (如果正在播放)
        ├── 初始化播放列表 (listen_playlist_init)
        │
        ▼
检查焦点状态
        │
        ├── NONE → 获取焦点并播放
        └── FOREGROUND → 直接播放
        │
        ▼
_audio_play_next()
        │
        ├── listen_next_audio() - 获取下一首
        │
        ├── 如果URL为空 → ls_req_url() 请求URL
        │
        ├── 设置状态为 APP_PLAYER_PREPARING
        │
        ├── 获取音频焦点 (如果需要)
        │
        └── app_player_play(PLAYER_T_CLOUD, url, callback)
                │
                ▼
        app_player_play() [src/audio/app_player.c:314]
                │
                ├── pa_manager_refresh(PA_MGR_ON) - 打开功放
                ├── 检查Preparing状态 - 如需要则等待
                ├── 增加 playid
                ├── 缓存回调 (__player_cache_callback)
                ├── 触发 APP_PLAYER_PREPARING 事件
                └── lisa_player_seturl(url)
                        │
                        ▼
                LisaPlayer (底层播放器)
                        │
                        ├── 准备 (PREPARED)
                        │   └── lisa_player_play()
                        ├── 播放中 (PLAYING)
                        ├── 暂停 (PAUSED)
                        ├── 完成 (COMPLETE)
                        └── 错误 (ERROR)
                                │
                                ▼
                        _audio_player_callback() [事件回调]
                                │
                                ├── 触发用户回调
                                ├── 状态更新
                                └── 完成时自动播放下一首
```

### 4.3 播放流程图

```
┌─────────────────────────────────────────────────────────────────────┐
│                        音乐播放流程                             │
└─────────────────────────────────────────────────────────────────────┘

    ┌──────────┐
    │  用户请求  │
    │  播放音乐  │
    └─────┬────┘
          │
          ▼
    ┌─────────────────────────────────────────────────────────┐
    │  on_directive(AUDIO_PLAY, audio_out_t[], count)         │
    │  ┌─────────────────────────────────────────────────┐    │
    │  │  1. 停止当前播放 stop(current, true)          │    │
    │  │  2. 初始化播放列表 playlist_init(items, count) │    │
    │  └─────────────────────────────────────────────────┘    │
    └─────────────────────────────────────────────────────────┘
          │
          ▼
    ┌─────────────────────────────────────────────────────────┐
    │  检查焦点状态 m_focus_state                          │
    │  ┌─────────────────────────────────────────────────┐    │
    │  │  NONE:        获取焦点 → 播放                  │    │
    │  │  FOREGROUND:  直接播放                          │    │
    │  │  BACKGROUND:  恢复前景                         │    │
    │  └─────────────────────────────────────────────────┘    │
    └─────────────────────────────────────────────────────────┘
          │
          ▼
    ┌─────────────────────────────────────────────────────────┐
    │  _audio_play_next(handle, force)                     │
    │  ┌─────────────────────────────────────────────────┐    │
    │  │  1. listen_next_audio(mode, &code)            │    │
    │  │     - 根据模式获取下一首                       │    │
    │  │     - code: 0=正常, 1=末首, -1=空            │    │
    │  │  2. ls_req_url(mid, url) [如URL为空]         │    │
    │  │  3. 设置 m_player_state = PREPARING           │    │
    │  │  4. acquire_channel(CONTENT) [如需要]        │    │
    │  │  5. app_player_play(CLOUD, url, callback)     │    │
    │  └─────────────────────────────────────────────────┘    │
    └─────────────────────────────────────────────────────────┘
          │
          ▼
    ┌─────────────────────────────────────────────────────────┐
    │  app_player_play(type, url, callback)                 │
    │  ┌─────────────────────────────────────────────────┐    │
    │  │  1. pa_manager_refresh(ON)                   │    │
    │  │  2. player_prepare_check() - 等待准备完成      │    │
    │  │  3. playid++                                  │    │
    │  │  4. callback_cache(cb, playid)                │    │
    │  │  5. callback(PREPARING)                        │    │
    │  │  6. is_preparing = true                       │    │
    │  │  7. lisa_player_seturl(url)                   │    │
    │  └─────────────────────────────────────────────────┘    │
    └─────────────────────────────────────────────────────────┘
          │
          ▼
    ┌─────────────────────────────────────────────────────────┐
    │  LisaPlayer 底层播放                                │
    │  ┌─────────────────────────────────────────────────┐    │
    │  │  PREPARED → PLAYING → COMPLETE               │    │
    │  │       ↓                                      │    │
    │  │  暂停: PAUSED → RESUME → PLAYING           │    │
    │  │       ↓                                      │    │
    │  │  停止: STOPPED                               │    │
    │  │       ↓                                      │    │
    │  │  错误: ERROR → reset()                      │    │
    │  └─────────────────────────────────────────────────┘    │
    └─────────────────────────────────────────────────────────┘
          │
          ▼
    ┌─────────────────────────────────────────────────────────┐
    │  _audio_player_callback(evt, arg1, arg2, id)         │
    │  ┌─────────────────────────────────────────────────┐    │
    │  │  PREPARED:                                    │    │
    │  │    - ignore_play = check_wait_intercepted()     │    │
    │  │    - lisa_player_play() [如未暂停]            │    │
    │  │  PLAYING:                                     │    │
    │  │    - broadcast_status(PLAYING)                 │    │
    │  │  PAUSED/STOPPED/COMPLETE/ERROR:              │    │
    │  │    - pa_manager_refresh(OFF)                  │    │
    │  │    - broadcast_status(evt)                    │    │
    │  │    - ERROR → reset()                         │    │
    │  └─────────────────────────────────────────────────┘    │
    └─────────────────────────────────────────────────────────┘
          │
          ▼
    ┌─────────────────────────────────────────────────────────┐
    │  _play_callback(status) [应用层回调]                   │
    │  ┌─────────────────────────────────────────────────┐    │
    │  │  1. 更新 m_player_state                       │    │
    │  │  2. COMPLETE/ERROR → post(_audio_on_play_end) │    │
    │  │  3. PAUSED → give(pause_sema)                │    │
    │  └─────────────────────────────────────────────────┘    │
    └─────────────────────────────────────────────────────────┘
          │
          ▼
    ┌─────────────────────────────────────────────────────────┐
    │  _audio_on_play_end() - 播放结束处理                 │
    │  ┌─────────────────────────────────────────────────┐    │
    │  │  → _audio_play_next(handle, false)             │    │
    │  │    - 自动播放下一首                            │    │
    │  │    - 列表为空时释放焦点并触发停止事件         │    │
    │  └─────────────────────────────────────────────────┘    │
    └─────────────────────────────────────────────────────────┘
```

### 4.4 焦点切换流程

```
┌─────────────────────────────────────────────────────────────────────┐
│                    焦点切换处理流程                             │
└─────────────────────────────────────────────────────────────────────┘

    _audio_on_focus_state(focus_state, by_which)
            │
            ├── 检测焦点状态变化
            │
            ├── NONE (失去焦点)
            │   ├── is_pause_called = true
            │   └── stop_sync() [如正在播放]
            │
            ├── FOREGROUND (获得前景焦点)
            │   ├── by_which == AIP → 不处理
            │   ├── is_need_play_after_op == true → 清除标志
            │   └── 否则 → _audio_play_foreground()
            │       ├── 状态 == PAUSED → resume_sync()
            │       └── 否则 → _audio_play_next()
            │
            └── BACKGROUND (被抢占为背景)
                └── _audio_play_background()
                    ├── 状态 == PLAYING → pause()
                    ├── 状态 == PREPARING → pause()
                    └── 状态 == PREPARED → pause()
```

---

## 5. 事件状态机

### 5.1 播放器事件 (PlayerEvt)

```c
// 来自 lisa_player.h
typedef enum {
    PLAYER_EVT_PREPARED,           // 准备完成
    PLAYER_EVT_PLAYING,           // 播放中
    PLAYER_EVT_PAUSED,            // 已暂停
    PLAYER_EVT_STOPED,            // 已停止
    PLAYER_EVT_PLAYBACK_COMPLETE,  // 播放完成
    PLAYER_EVT_ERROR              // 错误
} PlayerEvt;
```

### 5.2 应用层事件

```c
#define APP_PLAYER_PREPARING  (0xEF)  // 准备中
```

### 5.3 播放器状态转换

```
        ┌──────────────────────────────────────────────────────────────┐
        │                   播放器状态转换图                          │
        └──────────────────────────────────────────────────────────────┘

                        ┌─────────────┐
                        │   PREPARING  │  app_player_play()
                        │   (0xEF)     │  被调用
                        └──────┬──────┘
                               │
                    lisa_player_seturl()
                               │
                               ▼
                        ┌─────────────┐
                        │  PREPARED   │  准备完成
                        └──────┬──────┘
                               │
                        lisa_player_play()
                               │
                               ▼
                        ┌─────────────┐   ┌─────────────┐
                        │  PLAYING    │◄──┤  PAUSED     │  resume()
                        └──────┬──────┘   └──────┬──────┘
                               │                  │
                         pause()              resume()
                               │                  │
                               ▼                  │
                        ┌─────────────┐          │
                        │  PAUSED     │───────────┘
                        └──────┬──────┘
                               │
                         stop()
                               │
                               ▼
                        ┌─────────────┐   ┌─────────────┐
                        │  STOPPED    │   │  COMPLETE   │  播放完成
                        └─────────────┘   └──────┬──────┘
                                               │
                                               ▼
                                        下一曲自动播放
                            _audio_on_play_end()
                            └─► _audio_play_next()
```

### 5.4 控制命令 (audio_play_cmd)

```c
// src/player/audio_player.h:13-18
typedef enum {
    AUDIO_PLAY,     // 播放
    AUDIO_PAUSE,    // 暂停
    AUDIO_RESUME,   // 恢复
    AUDIO_STOP      // 停止
} audio_play_cmd;
```

---

## 6. API 接口

### 6.1 应用层播放器 API (app_player.h)

| 函数 | 描述 | 位置 |
|-----|------|-----|
| `app_player_init()` | 初始化播放器 | src/audio/app_player.c:231 |
| `app_player_play(type, url, cb)` | 播放音频 | src/audio/app_player.c:314 |
| `app_player_play_by_throw(type, url, time, cb)` | 延迟播放 | src/audio/app_player.c:319 |
| `app_player_pause(type)` | 暂停播放 | src/audio/app_player.c:379 |
| `app_player_resume(type)` | 恢复播放 | src/audio/app_player.c:403 |
| `app_player_resume_sync(type)` | 同步恢复 | src/audio/app_player.c:428 |
| `app_player_stop(type)` | 停止播放 | src/audio/app_player.c:453 |
| `app_player_stop_sync(type)` | 同步停止 | src/audio/app_player.c:478 |
| `app_player_seek(type, ms)` | 跳转播放位置 | src/audio/app_player.c:514 |
| `app_player_position(type)` | 获取当前位置 | src/audio/app_player.c:526 |
| `app_player_duration(type)` | 获取总时长 | src/audio/app_player.c:539 |
| `app_player_volume(type, vol)` | 设置音量 | src/audio/app_player.c:552 |
| `app_player_reset(type)` | 重置播放器 | src/audio/app_player.c:576 |

### 6.2 播放器类型

```c
#define PLAYER_T_CLOUD  (1)  // 云端音乐/内容
#define PLAYER_T_TONE   (2)  // 提示音
```

### 6.3 播放器功能 (audio_player_t)

| 函数指针 | 描述 | 位置 |
|---------|------|-----|
| `on_directive` | 处理播放指令 | src/player/audio_player.c:181 |
| `pause` | 暂停播放 | src/player/audio_player.c:207 |
| `resumeByVoice` | 语音恢复 | src/player/audio_player.c:275 |
| `next` | 下一首 | src/player/audio_player.c:237 |
| `prev` | 上一首 | src/player/audio_player.c:243 |
| `stop` | 停止播放 | src/player/audio_player.c:222 |
| `replay` | 重新播放 | src/player/audio_player.c:249 |

### 6.4 音频焦点管理 API (listen_audiomgr.h)

| 函数 | 描述 | 位置 |
|-----|------|-----|
| `listen_audiomgr_create()` | 创建音频管理器 | src/player/listen_audiomgr.c:18 |
| `listen_audiomgr_destory()` | 销毁音频管理器 | src/player/listen_audiomgr.c:32 |
| `listen_audiomgr_acquire_channel()` | 获取焦点 | src/player/listen_audiomgr.c:136 |
| `listen_audiomgr_release_channel()` | 释放焦点 | src/player/listen_audiomgr.c:211 |
| `listen_audiomgr_add_channel_callback()` | 添加回调 | src/player/listen_audiomgr.c:223 |

### 6.5 播放模式 API (play_mode.h)

| 函数 | 描述 | 位置 |
|-----|------|-----|
| `listen_play_mode_create()` | 创建播放模式 | src/player/play_mode.c:11 |
| `listen_playlist_init(handle, items, size)` | 初始化播放列表 | src/player/play_mode.c:24 |
| `listen_switch_playmode(handle, mode)` | 切换播放模式 | src/player/play_mode.c:50 |
| `listen_next_audio(handle, code)` | 获取下一首 | src/player/play_mode.c:60 |
| `listen_pre_audio(handle, code)` | 获取上一首 | src/player/play_mode.c:120 |
| `listen_get_curr_audio(handle)` | 获取当前歌曲 | src/player/play_mode.c:178 |
| `listen_playlist_deinit(handle)` | 清空播放列表 | src/player/play_mode.c:194 |

---

## 7. 关键文件索引

| 文件 | 描述 |
|-----|------|
| `src/audio/app_player.c` | 应用层播放器管理 |
| `src/audio/app_player.h` | 应用层播放器接口 |
| `src/player/audio_player.c` | 内容音乐播放器 |
| `src/player/audio_player.h` | 内容音乐播放器接口 |
| `src/player/listen_audiomgr.c` | 音频焦点管理 |
| `src/player/listen_audiomgr.h` | 音频焦点管理接口 |
| `src/player/play_mode.c` | 播放模式管理 |
| `src/player/play_mode.h` | 播放模式接口 |
| `src/player/audio_out.h` | 音频数据结构 |
| `src/player/tts_player.c` | TTS播放器 |
| `src/player/tts_player.h` | TTS播放器接口 |

---

## 8. 音频参数配置

### 8.1 LisaPlayer 采样率配置

```c
// src/audio/app_player.c:53-56
#define APPLICATION_PLAY_RB_LEFT_SHIFT_BIT  (12)
#define APPLICATION_PLAY_ONE_FRAME_SIZE    (512)
#define APPLICATION_PLAY_SAMPLERATE        (16000)
#define APPLICATION_PLAY_SAMPLERATE_ENUM  (1)
```

- 采样率: 16000 Hz
- 单帧大小: 512 字节
- 环形缓冲区左移位: 12

### 8.2 暂停超时配置

```c
// src/player/audio_player.c:120
#define AUDIOPLAYER_PAUSE_WAIT_TIMEOUT  (1000)  // 1秒
```

---

## 9. 关键流程时序

### 9.1 播放一首歌的完整时序

```
时间轴
  │
  ├─ T0: 用户请求播放
  │     └─ on_directive(AUDIO_PLAY)
  │
  ├─ T1: 准备播放
  │     └─ app_player_play() → PA开启 → is_preparing=true
  │
  ├─ T2: 设置URL
  │     └─ lisa_player_seturl(url)
  │
  ├─ T3: 准备完成 (PREPARED)
  │     └─ _audio_player_callback(PREPARED) → lisa_player_play()
  │
  ├─ T4: 开始播放 (PLAYING)
  │     └─ _audio_player_callback(PLAYING) → 用户回调
  │
  ├─ T5: 播放中...
  │     └─ 持续输出音频
  │
  ├─ T6: 播放完成 (COMPLETE)
  │     └─ _audio_player_callback(COMPLETE) → _audio_on_play_end()
  │
  └─ T7: 自动播放下一首
        └─ _audio_play_next()
```

### 9.2 焦点抢占时序

```
场景: 音乐播放中，用户唤醒语音助手

  │
  ├─ 音乐正在播放 (CONTENT通道, FOREGROUND)
  │
  ├─ 用户按下唤醒键
  │
  ├─ AIP通道请求焦点
  │     └─ listen_audiomgr_acquire_channel(AIP)
  │
  ├─ CONTENT被设为BACKGROUND
  │     └─ _audio_on_focus_state(BACKGROUND)
  │           └─ app_player_pause()
  │
  ├─ AIP获得FOREGROUND
  │
  ├─ TTS播报 "我在"
  │
  ├─ 用户说话完成
  │
  ├─ AIP释放焦点
  │     └─ listen_audiomgr_release_channel(AIP)
  │
  ├─ CONTENT恢复FOREGROUND
  │     └─ _audio_on_focus_state(FOREGROUND)
  │           └─ app_player_resume_sync()
  │
  └─ 音乐继续播放
```

---

## 10. 错误处理

### 10.1 准备状态等待

```c
// src/audio/app_player.c:285-312
static bool __player_prepare_check(app_player_item_t *player_item, const char *tips)
{
    if (player_item->is_preparing) {
        lisa_player_pre_close(player_item->hld);
        if (player_item->is_preparing) {
            player_item->wait_prepare_intercepted = true;
            lisa_semaphore_take(player_item->preparing_sem, LISA_OS_WAIT_FOREVER);
            player_item->wait_prepare_intercepted = false;
        }
        // 同步停止并重置
        lisa_player_stop_sync(player_item->hld);
        lisa_player_reset(player_item->hld);
        return false;
    }
    return true;
}
```

### 10.2 播放错误处理

```c
// src/audio/app_player.c:201-228
static int _audio_player_callback(PlayerEvt evt, ...)
{
    switch (evt) {
        case PLAYER_EVT_ERROR:
            pa_manager_refresh(PA_MGR_OFF, LS_PA_BASE_TIME, "audio_player_end");
            __player_broadcast_status(player, PLAYER_EVT_ERROR);
            lisa_player_reset(player->hld);  // 重置播放器
            break;
    }
}
```

---

## 11. 回调机制

### 11.1 回调缓存映射

```c
// src/audio/app_player.c:16-22
typedef struct status_cb_map {
    lisa_mutex_t *lock;
    player_status_cb pre_cb;    // 前一个回调
    uint32_t pre_id;           // 前一个播放ID
    player_status_cb cur_cb;   // 当前回调
    uint32_t cur_id;          // 当前播放ID
} status_cb_map_t;
```

### 11.2 回调生命周期

```
播放请求
  │
  └─ __player_cache_callback()
        └─ 存储 cur_cb 和 cur_id
              │
              ▼
        播放完成/停止/错误
              │
              └─ __player_broadcast_status()
                    └─ 调用 pre_cb
                          │
                          └─ 如果是停止事件
                                └─ cur → pre 迁移
                                      └─ cur 设为 NULL
```

---

## 12. 其他播放器类型

### 12.1 TTS播放器 (tts_player)

- 通道类型: `TTS`
- 优先级: 10
- 功能: TTS语音输出
- 支持打断和回复询问

### 12.2 警报播放器

- 通道类型: `ALERT`
- 优先级: 40
- 功能: 系统警报、闹钟

### 12.3 本地音频播放器

- 通道类型: `LOCAL`
- 优先级: 10
- 功能: 本地音频文件播放

---

## 13. 用户操作接口

### 13.1 播放控制

| 操作 | 函数 | 效果 |
|-----|------|-----|
| 播放 | `on_directive(AUDIO_PLAY, list, count)` | 初始化列表并播放 |
| 暂停 | `pause()` | 暂停当前播放 |
| 恢复 | `resumeByVoice()` | 恢复播放 |
| 下一首 | `next()` | 停止当前，播放下一首 |
| 上一首 | `prev()` | 停止当前，播放上一首 |
| 重播 | `replay()` | 重新播放当前歌曲 |
| 停止 | `stop()` | 停止播放 |
| 停止(用户) | `audio_player_stop_by_user()` | 停止并释放焦点 |

### 13.2 播放模式切换

```c
listen_switch_playmode(play_mode, MODE_ORDER);   // 顺序
listen_switch_playmode(play_mode, MODE_CYCLE);   // 循环
listen_switch_playmode(play_mode, MODE_SINGLE);  // 单曲循环
listen_switch_playmode(play_mode, MODE_RANDOM);  // 随机
```
