# 音频焦点管理（Audio Focus Management）

音频焦点管理是 app_player 的一个**可裁剪功能**，用于协调多个播放器实例之间的音频播放，避免音频冲突。

## 功能概述

**特性：**
- 默认**禁用**，启用需配置 `CONFIG_APP_PLAYER_AUDIO_FOCUS=y`
- 禁用后焦点相关 API 不可用，减少代码体积
- 适用于多播放器场景（如音乐、提示音、TTS 需要协调播放）

当应用中存在多个播放器时，焦点管理可以根据预定义的优先级和抢占规则自动处理播放状态切换。

## 核心概念

### 1. 焦点状态（Focus State）

每个播放器可以处于以下三种焦点状态之一：

- **FOREGROUND（前景焦点）**: 播放器拥有最高优先级，应该正常播放
- **BACKGROUND（背景焦点）**: 播放器被更高优先级的播放器抢占，应该暂停或降低音量
- **NONE（无焦点）**: 播放器没有焦点，应该停止播放

### 2. 优先级（Priority）

- 每个播放器通道都有一个优先级值（整数）
- **数值越小，优先级越高**（0 = 最高优先级）
- 优先级用于决定多个播放器申请焦点时的仲裁顺序

**重要说明：**
- 优先级和抢占列表是**两个独立的维度**
- 优先级主要用于：
  1. 当抢占列表**不匹配**时，通过优先级决定谁是前景、谁是背景
  2. 决定多个播放器同时竞争时的顺序

### 3. 焦点丢失策略（Focus Loss Policy）

当播放器失去焦点或降为背景时，可配置不同的处理策略：

| 策略 | 说明 | 使用场景 |
|------|------|---------|
| `APP_PLAYER_FOCUS_LOSS_IGNORE` | 忽略焦点变化，继续播放 | 特殊场景，需始终播放 |
| `APP_PLAYER_FOCUS_LOSS_PAUSE` | 暂停播放（可自动恢复） | 音乐播放被提示音打断 |
| `APP_PLAYER_FOCUS_LOSS_STOP` | 停止播放（不可自动恢复） | 提示音播放结束 |
| `APP_PLAYER_FOCUS_LOSS_DUCK` | 降低音量（暂不支持） | 保留，未来实现 |

**焦点行为配置：**
```c
app_player_focus_behavior_t behavior = {
    .on_background = APP_PLAYER_FOCUS_LOSS_PAUSE,  // 变为后景时暂停
    .on_focus_lost = APP_PLAYER_FOCUS_LOSS_STOP,   // 完全失焦时停止
    .duck_volume_percent = 30  // 暂不支持
};
```

### 4. 抢占规则（Capture Rules）

抢占规则是焦点管理的核心机制，通过 `capture_names` 数组定义。

**核心原理：**

1. **白名单机制**
   - 每个播放器的 `capture_names` 定义了"我可以抢占哪些播放器"
   - 这是一个**显式白名单**，只有在列表中的播放器才会被抢占
   - **不在列表中的播放器不会被抢占**，即使优先级更低

2. **抢占与优先级的工作机制**

   焦点管理器按以下顺序判断（**三个独立判断**）：

   **判断 1：检查是否能够强制抢占（最高优先级）**
   ```
   if (B 在 A 的 capture_names 中) {
       // A 可以强制抢占 B（不考虑优先级数值）
       // B → NONE，A 重新申请焦点
       return;
   }
   ```

   **判断 2：检查优先级是否相等**
   ```
   if (A 的优先级 == B 的优先级) {
       // 优先级相同，新申请者优先
       // B → NONE，A → 前景
       return;
   }
   ```

   **判断 3：通过优先级数值仲裁**
   ```
   if (A 的优先级 < B 的优先级) {  // 数值小 = 优先级高
       // A 优先级更高
       // B → 背景，A → 前景
   } else {
       // A 优先级更低
       // A → 背景（如果背景为空）
   }
   ```

   **关键点：**
   - **三个判断是独立的**：抢占列表、优先级相等、优先级比较
   - `capture_names` 提供**强制抢占**能力，优先级最高，无视优先级数值
   - 不在抢占列表时，完全通过**优先级数值**仲裁
   - 优先级相同时，新申请者优先（取代当前前景）

3. **典型配置示例**

   **示例 1：三级优先级（提示音 > TTS > 音乐）**
   ```c
   static const char *tone_captures[] = {"tts", "music"};
   static const char *tts_captures[] = {"music"};

   static const app_player_focus_channel_config_t focus_configs[] = {
       {
           .name = "tone",
           .priority = 0,
           .capture_names = tone_captures,
           .capture_count = 2,
           .behavior = {
               .on_background = APP_PLAYER_FOCUS_LOSS_PAUSE,
               .on_focus_lost = APP_PLAYER_FOCUS_LOSS_STOP,
           }
       },
       {
           .name = "tts",
           .priority = 1,
           .capture_names = tts_captures,
           .capture_count = 1,
           .behavior = {
               .on_background = APP_PLAYER_FOCUS_LOSS_PAUSE,
               .on_focus_lost = APP_PLAYER_FOCUS_LOSS_STOP,
           }
       },
       {
           .name = "music",
           .priority = 2,
           .capture_names = NULL,
           .capture_count = 0,
           .behavior = {
               .on_background = APP_PLAYER_FOCUS_LOSS_PAUSE,
               .on_focus_lost = APP_PLAYER_FOCUS_LOSS_STOP,
           }
       }
   };

   // 抢占关系：
   // - tone (优先级0) 可以抢占 tts 和 music
   // - tts  (优先级1) 可以抢占 music
   // - music(优先级2) 不抢占任何播放器
   ```

   **示例 2：选择性抢占（理解抢占 vs 优先级仲裁）**
   ```c
   static const char *call_captures[] = {"music"};
   static const char *notif_captures[] = {"music"};

   static const app_player_focus_channel_config_t focus_configs[] = {
       {
           .name = "call",
           .priority = 0,
           .capture_names = call_captures,
           .capture_count = 1,
           .behavior = {
               .on_background = APP_PLAYER_FOCUS_LOSS_PAUSE,
               .on_focus_lost = APP_PLAYER_FOCUS_LOSS_STOP,
           }
       },
       {
           .name = "notification",
           .priority = 1,
           .capture_names = notif_captures,
           .capture_count = 1,
           .behavior = {
               .on_background = APP_PLAYER_FOCUS_LOSS_PAUSE,
               .on_focus_lost = APP_PLAYER_FOCUS_LOSS_STOP,
           }
       },
       {
           .name = "music",
           .priority = 2,
           .capture_names = NULL,
           .capture_count = 0,
           .behavior = {
               .on_background = APP_PLAYER_FOCUS_LOSS_PAUSE,
               .on_focus_lost = APP_PLAYER_FOCUS_LOSS_STOP,
           }
       }
   };

   // 行为分析：
   // 场景 1：music 播放中，call 申请焦点
   //   - call 能抢占 music（music 在 call 的 capture_names 中）
   //   - 结果：music → NONE，call → 前景

   // 场景 2：call 播放中，notification 申请焦点
   //   - notification 不能抢占 call（call 不在 notification 的 capture_names 中）
   //   - 进入优先级仲裁：call(0) vs notification(1)
   //   - call 优先级更高（数值小）
   //   - 结果：call → 前景，notification → 背景

   // 场景 3：notification 播放中，call 申请焦点
   //   - call 不能抢占 notification（notification 不在 call 的 capture_names 中）
   //   - 进入优先级仲裁：call(0) vs notification(1)
   //   - call 优先级更高
   //   - 结果：notification → 背景，call → 前景
   ```

4. **设计要点**
   - **高优先级播放器**：应在 `capture_names` 中列出需要打断的播放器
   - **低优先级播放器**：通常设置 `capture_names = NULL`，不主动抢占
   - **避免循环抢占**：不要配置 A 抢占 B，同时 B 也抢占 A
   - **选择性抢占**：利用白名单机制实现特殊场景（如通话中允许通知音）

## 使用流程

### 第一步：定义焦点通道配置

在初始化 app_player 时，定义各个播放器的焦点配置：

```c
#include "app_player.h"

// 定义可抢占的通道列表
static const char *tone_captures[] = {"music", "tts"};
static const char *tts_captures[] = {"music"};

// 定义焦点通道配置数组
static const app_player_focus_channel_config_t focus_configs[] = {
    {
        .name = "tone",                  // 通道名称（需与 create 时的 name 匹配）
        .priority = 0,                   // 最高优先级
        .capture_names = tone_captures,
        .capture_count = 2,              // 可抢占 music 和 tts
        .behavior = {
            .on_background = APP_PLAYER_FOCUS_LOSS_PAUSE,
            .on_focus_lost = APP_PLAYER_FOCUS_LOSS_STOP,
        }
    },
    {
        .name = "tts",
        .priority = 1,                   // 中等优先级
        .capture_names = tts_captures,
        .capture_count = 1,              // 可抢占 music
        .behavior = {
            .on_background = APP_PLAYER_FOCUS_LOSS_PAUSE,
            .on_focus_lost = APP_PLAYER_FOCUS_LOSS_STOP,
        }
    },
    {
        .name = "music",
        .priority = 2,                   // 最低优先级
        .capture_names = NULL,
        .capture_count = 0,              // 不抢占任何播放器
        .behavior = {
            .on_background = APP_PLAYER_FOCUS_LOSS_PAUSE,
            .on_focus_lost = APP_PLAYER_FOCUS_LOSS_STOP,
        }
    }
};

// 初始化 app_player，传入焦点配置
app_player_config_t config = {
    .pa_ctrl_callback = pa_control_callback,
    .focus_configs = focus_configs,
    .focus_config_count = 3
};
app_player_init(&config);
```

### 第二步：创建播放器实例

```c
// 创建播放器（名称必须与焦点配置中的 name 匹配）
app_player_t *tone_player = app_player_create("tone");
app_player_t *tts_player = app_player_create("tts");
app_player_t *music_player = app_player_create("music");

// 如果创建的播放器名称在焦点配置中，会自动注册到焦点管理器
// 如果名称不在配置中，则该播放器不参与焦点管理
```

### 第三步：注册焦点变化回调（可选）

```c
// 定义焦点变化回调函数
// 返回值：true=完全接管处理，false=执行默认策略
bool music_focus_callback(app_player_t *player,
                          app_player_focus_state_t state,
                          app_player_t *by_which,
                          void *user_data) {
    const char *by_name = by_which ? by_which->name : "unknown";

    switch (state) {
        case APP_PLAYER_FOCUS_FOREGROUND:
            printf("Music got foreground (by %s)\n", by_name);
            break;

        case APP_PLAYER_FOCUS_BACKGROUND:
            printf("Music moved to background (by %s)\n", by_name);
            break;

        case APP_PLAYER_FOCUS_NONE:
            printf("Music lost focus (by %s)\n", by_name);
            break;
    }

    // 返回 false，让 app_player 根据 behavior 配置自动执行策略
    return false;
}

// 注册回调（如果不注册，仍会执行默认策略，只是没有通知）
app_player_register_focus_cb(music_player, music_focus_callback, NULL);
```

**回调函数说明：**
- **返回 false**：app_player 根据 `behavior` 配置自动执行策略（pause/stop/ignore）
- **返回 true**：app_player 完全跳过处理，由应用层自行处理
- **不注册回调**：仍会执行 `behavior` 配置的默认策略，只是没有事件通知

### 第四步：播放时自动申请焦点

```c
// 播放时会自动申请焦点，无需手动调用
app_player_play(tone_player, "tone://0");
// → tone 优先级 0，会抢占 tts 和 music
// → tts 和 music 会收到焦点变化回调

app_player_play(tts_player, "https://example.com/tts.mp3");
// → tts 优先级 1，会抢占 music
// → music 会收到焦点变化回调

app_player_play(music_player, "https://example.com/music.mp3");
// → music 优先级 2，不会抢占任何播放器
```

## 焦点管理内部判断流程

焦点管理器维护两个角色：**前景（FOREGROUND）** 和 **背景（BACKGROUND）**，同一时刻最多只有一个前景和一个背景。

### 焦点申请流程（acquire）

当播放器调用 `app_player_play()` 时，会自动申请焦点，焦点管理器按以下流程处理：

```
┌─────────────────────────────────────────────────────────────────┐
│ 播放器申请焦点：listen_audiomgr_acquire_channel(requester_id)  │
└─────────────────────────────────┬───────────────────────────────┘
                                  │
                    ┌─────────────▼─────────────┐
                    │  是否存在前景播放器？      │
                    └────┬─────────────────┬────┘
                         │是               │否
            ┌────────────▼───────┐    ┌───▼────────────────────┐
            │ 判断与前景的关系    │    │  是否存在背景播放器？  │
            └────┬───────────────┘    └───┬────────────────┬───┘
                 │                        │是              │否
     ┌───────────▼───────────────┐ ┌──────▼─────────┐  ┌──▼──────────┐
     │ 能抢占前景？              │ │ 比较优先级     │  │ 直接成为前景 │
     │ (前景在capture_names中)   │ │ (数值小=高)    │  └─────────────┘
     └──┬────────────────────┬───┘ └──┬─────────────┘
        │是                  │否       │
   ┌────▼────────┐  ┌────────▼──────────────────┐
   │ 前景→NONE   │  │  比较 requester 与前景优先级│
   │ 递归申请    │  └──┬──────────┬───────────┬──┘
   └─────────────┘     │          │           │
                  ┌────▼─────┐ ┌──▼──────┐ ┌─▼──────────┐
                  │ 优先级   │ │ 优先级  │ │ 优先级     │
                  │ 更高     │ │ 相同    │ │ 更低       │
                  │ (数值小) │ │         │ │ (数值大)   │
                  └────┬─────┘ └──┬──────┘ └─┬──────────┘
                       │          │           │
            ┌──────────▼────┐ ┌──▼──────┐ ┌──▼─────────────┐
            │ 前景→背景     │ │前景→NONE│ │ requester→背景 │
            │(背景→NONE)    │ │req→前景 │ │(如背景为空)    │
            │ req→前景      │ └─────────┘ └────────────────┘
            └───────────────┘
```

### 焦点释放流程（release）

当播放器停止或播放完成时，自动释放焦点：

```
┌──────────────────────────────────────────────────────┐
│ 播放器释放焦点：listen_audiomgr_release_channel(id) │
└───────────────────────┬──────────────────────────────┘
                        │
              ┌─────────▼──────────┐
              │ 设置该通道为 NONE  │
              └─────────┬──────────┘
                        │
          ┌─────────────▼──────────────┐
          │ 该通道是否是前景播放器？   │
          └────┬─────────────────┬─────┘
               │是               │否
    ┌──────────▼────────┐   ┌───▼────────────┐
    │ 是否存在背景？    │   │ 无需处理       │
    └────┬─────────┬────┘   └────────────────┘
         │是       │否
    ┌────▼───┐  ┌─▼────────┐
    │ 背景→  │  │ 无前景   │
    │  前景  │  │ 无背景   │
    └────────┘  └──────────┘
```

## 焦点状态转换示例

假设有三个播放器：tone（优先级0）、tts（优先级1）、music（优先级2）

**场景 1：音乐播放时，播放提示音**

```
初始状态：
  tone: NONE    tts: NONE    music: FOREGROUND (正在播放)

执行：app_player_play(tone_player, ...)

结果：
  tone: FOREGROUND (正在播放)
  tts: NONE
  music: BACKGROUND (被暂停，收到回调 by_which=tone播放器)
```

**场景 2：提示音播放完成**

```
初始状态：
  tone: FOREGROUND (正在播放)
  music: BACKGROUND (被暂停)

事件：tone 播放完成，自动释放焦点

结果：
  tone: NONE
  music: FOREGROUND (恢复播放，收到回调 by_which=tone播放器)
```

**场景 3：音乐播放时，播放 TTS，然后播放提示音**

```
初始：music: FOREGROUND

执行：app_player_play(tts_player, ...)
结果：tts: FOREGROUND, music: BACKGROUND

执行：app_player_play(tone_player, ...)
结果：tone: FOREGROUND, tts: BACKGROUND, music: NONE

tone 播放完成：
结果：tone: NONE, tts: FOREGROUND, music: NONE

tts 播放完成：
结果：tts: NONE, music: NONE (music 已被完全停止，不会自动恢复)
```

## API 参考

### 焦点相关数据结构

```c
// 焦点状态枚举
typedef enum {
    APP_PLAYER_FOCUS_FOREGROUND = 0,  // 前景焦点
    APP_PLAYER_FOCUS_BACKGROUND,      // 背景焦点
    APP_PLAYER_FOCUS_NONE             // 无焦点
} app_player_focus_state_t;

// 焦点丢失策略
typedef enum {
    APP_PLAYER_FOCUS_LOSS_IGNORE = 0,  // 忽略焦点变化
    APP_PLAYER_FOCUS_LOSS_PAUSE,       // 暂停播放（可恢复）
    APP_PLAYER_FOCUS_LOSS_STOP,        // 停止播放（不可恢复）
    APP_PLAYER_FOCUS_LOSS_DUCK,        // 降低音量（暂不支持）
} app_player_focus_loss_policy_t;

// 焦点行为配置
typedef struct {
    app_player_focus_loss_policy_t on_background;  // 变为后景时的策略
    app_player_focus_loss_policy_t on_focus_lost;  // 完全失焦时的策略
    uint8_t duck_volume_percent;                    // 降低音量百分比（暂不支持）
} app_player_focus_behavior_t;

// 焦点变化回调函数类型
// 返回值：true=完全接管处理，false=执行默认策略
typedef bool (*app_player_focus_change_cb_t)(
    app_player_t *player,                // 播放器实例
    app_player_focus_state_t new_state,  // 新的焦点状态
    app_player_t *by_which,              // 触发焦点变化的播放器句柄
    void *user_data                      // 用户自定义数据
);

// 焦点通道配置（在 app_player_init 时使用）
typedef struct {
    const char *name;                            // 通道名称（需与 create 时的 name 匹配）
    int priority;                                // 优先级（0 = 最高）
    const char **capture_names;                  // 可抢占的通道名称列表
    int capture_count;                           // 抢占列表长度
    app_player_focus_behavior_t behavior;        // 焦点丢失时的行为配置
} app_player_focus_channel_config_t;
```

### 焦点相关 API

| 函数 | 说明 |
|------|------|
| `app_player_register_focus_cb()` | 注册焦点变化回调（可选） |
| `app_player_set_focus_behavior()` | 运行时动态修改焦点行为策略 |
| `app_player_get_focus_behavior()` | 获取当前的焦点行为策略 |

**注意：** 焦点的申请和释放是自动的：
- 调用 `app_player_play()` 时自动申请焦点
- 播放完成或停止时自动释放焦点

**焦点功能编译控制：**
- 仅在 `CONFIG_APP_PLAYER_AUDIO_FOCUS=y` 时，焦点相关 API 和数据结构才可用
- 禁用后可减少代码体积，适合单播放器场景

## 设计原则与注意事项

1. **通道名称必须匹配**
   - 焦点配置中的 `name` 必须与 `app_player_create(name)` 的参数完全一致
   - 不在焦点配置中的播放器不参与焦点管理

2. **优先级设计建议**
   - 提示音（系统音效）：优先级 0（最高）
   - TTS/语音播报：优先级 1
   - 音乐/背景音：优先级 2（最低）

3. **抢占规则设计**
   - 只配置必要的抢占关系，避免循环依赖
   - 高优先级播放器应该可以抢占低优先级播放器
   - 低优先级播放器通常不抢占任何其他播放器

4. **焦点行为配置**
   - 在 `app_player_init()` 时通过 `focus_configs` 配置每个通道的默认行为
   - 运行时可通过 `app_player_set_focus_behavior()` 动态修改
   - 不同播放器可配置不同的焦点丢失策略（pause/stop/ignore）

5. **回调函数注意事项**
   - 回调函数应快速返回，避免阻塞焦点管理器
   - 耗时操作（如网络请求）应提交到任务队列异步执行
   - 回调返回 `false` 时，app_player 自动根据 `behavior` 配置执行策略
   - 回调返回 `true` 时，app_player 当前不执行注册的策略，由应用层自行处理

6. **默认行为**
   - 如果不注册焦点回调，app_player 仍会根据 `behavior` 配置自动执行策略
   - 大多数场景下，配置好 `behavior` 即可，无需注册回调

7. **禁用焦点管理**
   - 如果应用只使用单个播放器，建议禁用 `CONFIG_APP_PLAYER_AUDIO_FOCUS` 减少代码体积
   - 禁用后，焦点相关的 API 和数据结构将不可用
   - 默认情况下焦点功能是禁用的

## 完整示例

```c
#include "app_player.h"

// 1. 定义焦点配置
static const char *tone_captures[] = {"music", "tts"};
static const char *tts_captures[] = {"music"};

static const app_player_focus_channel_config_t focus_configs[] = {
    {
        .name = "tone",
        .priority = 0,
        .capture_names = tone_captures,
        .capture_count = 2,
        .behavior = {
            .on_background = APP_PLAYER_FOCUS_LOSS_PAUSE,
            .on_focus_lost = APP_PLAYER_FOCUS_LOSS_STOP,
        }
    },
    {
        .name = "tts",
        .priority = 1,
        .capture_names = tts_captures,
        .capture_count = 1,
        .behavior = {
            .on_background = APP_PLAYER_FOCUS_LOSS_PAUSE,
            .on_focus_lost = APP_PLAYER_FOCUS_LOSS_STOP,
        }
    },
    {
        .name = "music",
        .priority = 2,
        .capture_names = NULL,
        .capture_count = 0,
        .behavior = {
            .on_background = APP_PLAYER_FOCUS_LOSS_PAUSE,
            .on_focus_lost = APP_PLAYER_FOCUS_LOSS_STOP,
        }
    }
};

// 2. 初始化 app_player
void audio_init(void) {
    app_player_config_t config = {
        .pa_ctrl_callback = pa_ctrl,
        .focus_configs = focus_configs,
        .focus_config_count = 3
    };
    app_player_init(&config);
}

// 3. 创建播放器并注册回调（可选）
app_player_t *tone, *tts, *music;

// 焦点变化通知回调（可选，仅用于监听焦点变化）
bool music_focus_cb(app_player_t *p, app_player_focus_state_t state,
                    app_player_t *by, void *ud) {
    const char *by_name = by ? by->name : "unknown";
    printf("Music focus: %d (by %s)\n", state, by_name);
    return false;  // 让 app_player 执行默认策略
}

void create_players(void) {
    tone = app_player_create("tone");
    tts = app_player_create("tts");
    music = app_player_create("music");

    // 注册回调（可选，仅用于监听焦点变化）
    app_player_register_focus_cb(music, music_focus_cb, NULL);
}

// 4. 使用（焦点自动管理）
void play_samples(void) {
    app_player_play(music, "https://example.com/music.mp3");
    // music 获得焦点开始播放

    app_player_play(tone, "tone://beep");
    // tone 抢占 music，music 根据 behavior 配置自动暂停

    // tone 播放完成后，music 自动恢复
}
```
