# 播放器管理接口文档

## 概述

`player_mgr.h` 提供统一的播放器管理接口，支持多种播放器类型的创建和控制。

## 数据结构

### DEFAULT_PLAY_ID - 默认播放器ID

```c
typedef enum {
    AIP = 0,      // 识别通道（唤醒音等）
    TTS = 1,      // TTS播放
    ALERT = 2,    // 提示音/闹钟
    CONTENT = 3,  // 内容播放（音乐等，支持完整控制）
    LOCAL = 4,    // 本地音效
} DEFAULT_PLAY_ID;
```

### PLAY_ACTION - 播放动作枚举

```c
typedef enum PLAY_ACTION {
    PLAY_ACTION_PLAY = 0,          // 播放
    PLAY_ACTION_PAUSE,             // 暂停（仅 CONTENT 支持）
    PLAY_ACTION_STOP,              // 停止
    PLAY_ACTION_RECOGNIZE,         // 识别开始（AIP专用）
    PLAY_ACTION_RECOGNIZE_END,     // 识别结束（AIP专用）
} PLAY_ACTION;
```

### player_config_t - 播放器配置

```c
typedef struct player_config_s {
    int id;                                  // 播放器ID
    char *name;                              // 播放器名称（用于日志）
    int priority;                            // 优先级（数值越小优先级越高）
    int capture_ids[MAX_CAPTURE_COUNT];      // 可抢占的播放器ID列表
    int capture_count;                       // 抢占列表长度
    PLAY_ACTION fg_action;                   // 前景动作
    PLAY_ACTION bg_action;                   // 背景动作
    PLAY_ACTION none_action;                 // 失焦动作
} player_config_t;
```

**配置说明：**
- `bg_action = PLAY_ACTION_PAUSE`：支持暂停/恢复/上下首，仅支持一个
- `fg_action = PLAY_ACTION_RECOGNIZE`：AIP识别通道，仅支持一个

### 回调类型

```c
// 播放状态回调
typedef void (*player_status_cb_t)(int player_id, uint16_t status, void *arg);

// 焦点状态回调
typedef void (*player_focus_cb_t)(int player_id, focus_state_e state, int by_which, void *arg);
```

**status 取值（来自 lisa_player.h）：**
- `APP_PLAYER_PREPARING` - 准备中
- `PLAYER_EVT_PLAYING` - 播放中
- `PLAYER_EVT_PAUSED` - 已暂停
- `PLAYER_EVT_STOPED` - 已停止
- `PLAYER_EVT_PLAYBACK_COMPLETE` - 播放完成
- `PLAYER_EVT_ERROR` - 播放错误

**focus_state_e 取值：**
- `FOREGROUND` - 前景
- `BACKGROUND` - 背景
- `FOCUS_NONE` - 无焦点

---

## 接口说明

### 初始化

#### player_mgr_init
```c
int player_mgr_init(player_config_t *configs, int config_count);
```
初始化播放管理器，创建所有播放器实例。

| 参数 | 说明 |
|------|------|
| configs | 播放器配置数组 |
| config_count | 配置数量 |
| 返回值 | 0 成功，其他失败 |

**示例：**
```c
static player_config_t s_player_configs[] = {
	{
		.id = AIP,
		.name = "AIP",
		.priority = 30,
		.capture_ids = {AIP, TTS, LOCAL},
		.capture_count = 3,
		.fg_action = PLAY_ACTION_RECOGNIZE,
		.bg_action = PLAY_ACTION_RECOGNIZE_END,
		.none_action = PLAY_ACTION_RECOGNIZE_END,
	},
	{
		.id = TTS,
		.name = "TTS",
		.priority = 10,
		.capture_ids = {AIP, ALERT},
		.capture_count = 2,
		.fg_action = PLAY_ACTION_PLAY,
		.bg_action = PLAY_ACTION_STOP,
		.none_action = PLAY_ACTION_STOP,
	},
	{
		.id = ALERT,
		.name = "ALERT",
		.priority = 40,
		.capture_ids = {AIP, TTS, LOCAL},
		.capture_count = 3,
		.fg_action = PLAY_ACTION_PLAY,
		.bg_action = PLAY_ACTION_STOP,
		.none_action = PLAY_ACTION_STOP,
	},
	{
		.id = CONTENT,
		.name = "CONTENT",
		.priority = 50,
		.capture_ids = {AIP, TTS, ALERT, LOCAL},
		.capture_count = 4,
		.fg_action = PLAY_ACTION_PLAY,
		.bg_action = PLAY_ACTION_PAUSE,
		.none_action = PLAY_ACTION_STOP,
	},
	{
		.id = LOCAL,
		.name = "LOCAL",
		.priority = 10,
		.capture_ids = {AIP},
		.capture_count = 1,
		.fg_action = PLAY_ACTION_PLAY,
		.bg_action = PLAY_ACTION_STOP,
		.none_action = PLAY_ACTION_STOP,
	},	
};

player_mgr_init(s_player_configs, sizeof(s_player_configs) / sizeof(s_player_configs[0]));
```

---

### 回调注册

#### player_mgr_register_status_cb
```c
int player_mgr_register_status_cb(int player_id, player_status_cb_t cb, void *arg);
```
注册播放状态回调。

| 参数 | 说明 |
|------|------|
| player_id | 播放器ID |
| cb | 回调函数 |
| arg | 用户参数 |
| 返回值 | 0 成功，其他失败 |

#### player_mgr_register_focus_cb
```c
int player_mgr_register_focus_cb(int player_id, player_focus_cb_t cb, void *arg);
```
注册焦点状态回调。

| 参数 | 说明 |
|------|------|
| player_id | 播放器ID |
| cb | 回调函数 |
| arg | 用户参数 |
| 返回值 | 0 成功，其他失败 |

---

### 播放控制

#### player_mgr_play
```c
int player_mgr_play(int player_id, const char *url, int throw_time_ms);
```
播放音频（打断模式）。

| 参数 | 说明 |
|------|------|
| player_id | 播放器ID |
| url | 音频URL或路径 |
| throw_time_ms | 丢弃能量较低音频的最大时常 |
| 返回值 | 0 成功，其他失败 |

**示例：**
```c
// 播放TTS,丢弃开头能量较低的音频，最大丢300ms
player_mgr_play(TTS, "/res/hello.mp3", 300);

// 播放音乐，
player_mgr_play(CONTENT, "http://xxx/music.mp3", 0);
```

#### player_mgr_play_item
```c
int player_mgr_play_item(int player_id, audio_out_t *item, bool interrupt);
```
播放单个音频项（支持打断/追加模式）。

| 参数 | 说明 |
|------|------|
| player_id | 播放器ID |
| item | 音频项 |
| interrupt | true=打断当前播放，false=追加到队列 |
| 返回值 | 0 成功，其他失败 |

#### player_mgr_play_array
```c
int player_mgr_play_array(int player_id, const audio_out_t *items, int count);
```
播放音频数组（打断模式，按顺序播放）。

| 参数 | 说明 |
|------|------|
| player_id | 播放器ID |
| items | 音频项数组 |
| count | 数组长度 |
| 返回值 | 0 成功，其他失败 |

#### player_mgr_stop
```c
int player_mgr_stop(int player_id);
```
停止播放。

| 参数 | 说明 |
|------|------|
| player_id | 播放器ID |
| 返回值 | 0 成功，其他失败 |

---

### 完整控制

以下接口仅对 `bg_action = PLAY_ACTION_PAUSE` 的播放器有效。

#### player_mgr_pause
```c
int player_mgr_pause(int player_id);
```
暂停播放。

#### player_mgr_resume
```c
int player_mgr_resume(int player_id);
```
恢复播放。

#### player_mgr_play_next
```c
int player_mgr_play_next(int player_id);
```
播放下一首。

#### player_mgr_play_prev
```c
int player_mgr_play_prev(int player_id);
```
播放上一首。

#### player_mgr_switch_playmode
```c
int player_mgr_switch_playmode(int player_id, PLAY_MODE_E mode);
```
切换播放模式。

| mode 取值 | 说明 |
|-----------|------|
| `PLAY_MODE_ORDER` | 顺序播放 |
| `PLAY_MODE_CYCLE` | 列表循环 |
| `PLAY_MODE_SINGLE` | 单曲循环 |
| `PLAY_MODE_RANDOM` | 随机播放 |

---

### 音量控制

#### player_mgr_set_volume
```c
int player_mgr_set_volume(int volume);
```
设置音量。

| 参数 | 说明 |
|------|------|
| volume | 音量值 (0-100) |
| 返回值 | 0 成功，其他失败 |

#### player_mgr_get_volume
```c
int player_mgr_get_volume(void);
```
获取当前音量。

| 返回值 | 说明 |
|--------|------|
| int | 当前音量值 |

---

### 其他

#### player_mgr_get_config
```c
player_config_t *player_mgr_get_config(int player_id);
```
获取播放器配置。

| 参数 | 说明 |
|------|------|
| player_id | 播放器ID |
| 返回值 | 播放器配置指针，不存在返回NULL |

---

## 焦点管理

播放器通过焦点管理实现互斥和抢占：

1. **抢占规则**：播放器抢占 `capture_ids` 中的播放器，直接使其进入无焦点状态
2. **优先级**：`priority` 值越大优先级越高
3. **前景/背景**：
   - 前景：当前正在播放
   - 背景：被其他播放器抢占，执行 `bg_action`
   - 失焦：完全释放，执行 `none_action`


