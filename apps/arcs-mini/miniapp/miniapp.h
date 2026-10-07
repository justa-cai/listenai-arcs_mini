#ifndef ARCS_MINI_MINIAPP_H
#define ARCS_MINI_MINIAPP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MINIAPP_API_VERSION 4
#define MINIAPP_SCREEN_WIDTH CONFIG_MINIAPP_SCREEN_WIDTH
#define MINIAPP_SCREEN_HEIGHT CONFIG_MINIAPP_SCREEN_HEIGHT
#define MINIAPP_TEXT_FONT_PX 16
#define MINIAPP_SOURCE_MAX CONFIG_MINIAPP_SOURCE_MAX_BYTES
#define MINIAPP_MAX_RECTS 128
#define MINIAPP_MAX_TEXTS 8
#define MINIAPP_TEXT_MAX 63
#define MINIAPP_LUA_HEAP_LIMIT CONFIG_MINIAPP_HEAP_MAX_BYTES
#define MINIAPP_TICK_MS 20u
#define MINIAPP_CHUNK_INSTRUCTION_LIMIT 500000u
/* 源码块的时间预算。解析(词法/语法/代码生成)也落在这个窗口里，而且解析期间
 * 指令钩子不触发 —— 预算实际上只在"执行到第 1000 条指令"的检查点被核算，
 * 所以余量必须按解析耗时给：设备实测约 130 字节/ms，满额 128 KiB
 * (见 CONFIG_MINIAPP_SOURCE_MAX_BYTES 的应用级覆盖) 的解析约 1 s。
 * 取 2 s 对该上限留约 2 倍余量，同时把安装/启动时可能占住 Lua 任务的最长时间
 * 限制在 2 秒内。执行侧的计算量仍由上面的指令上限独立约束。 */
#define MINIAPP_CHUNK_DEADLINE_MS 2000u
#define MINIAPP_CALLBACK_INSTRUCTION_LIMIT 250000u
/* Allow occasional response parsing to exceed the tick interval. Runtime
 * statistics discount task preemption; the instruction limit stays bounded. */
#define MINIAPP_CALLBACK_DEADLINE_MS 500u
#define MINIAPP_BUTTON_EXIT_HOLD_MS 3000u
/* 传给 on_button_click 的 button_id 取值。
 *
 * 设备按键驱动是"聚合多击"语义 (FlexibleButton: 静默 300ms 后才发一个事件),
 * 所以快速双击会合成 DOUBLE_CLICK —— 设备把它作为 function_double 转发, 让脚本
 * 能区分"单击"与"双击"; 三击及以上和长按仍由系统屏蔽。
 *
 * 方向/确认/返回/设置这几个取值来自手柄 (src/middleware/gamepad): 手柄天然是
 * 离散按键, 所以直接翻译成点击事件; 方向键按住会由 gamepad 侧做自动重复,
 * 对应桌面上的"连续移动焦点"。没有手柄时设备功能键仍只产生
 * function / function_double 两种。
 *
 * 新增取值时必须同步: 手柄映射表 (gamepad_input.c)、能力上报的 events 与
 * docs/miniapp.md; 只能使用 [a-z_] 字符, 长度不超过 MINIAPP_BUTTON_ID_MAX。 */
#define MINIAPP_BUTTON_ID_FUNCTION        "function"
#define MINIAPP_BUTTON_ID_FUNCTION_DOUBLE "function_double"
#define MINIAPP_BUTTON_ID_UP              "up"
#define MINIAPP_BUTTON_ID_DOWN            "down"
#define MINIAPP_BUTTON_ID_LEFT            "left"
#define MINIAPP_BUTTON_ID_RIGHT           "right"
#define MINIAPP_BUTTON_ID_BACK            "back"
#define MINIAPP_BUTTON_ID_SETTINGS        "settings"
#define MINIAPP_BUTTON_ID_MAX             15u
#define MINIAPP_BUZZER_MIN_HZ 100u
#define MINIAPP_BUZZER_MAX_HZ 5000u
#define MINIAPP_BUZZER_MIN_MS 20u
#define MINIAPP_BUZZER_MAX_MS 3000u
/* buzzer.play_seq(text) —— 整曲播放。文本是 "freq:ms,freq:ms,..."，freq = 0 表示
 * 休止；空串表示停止。
 *
 * MAX_MS 比单音的 MINIAPP_BUZZER_MAX_MS 大得多: 单音的 3000ms 上限来自"一个音
 * 一个 WAV 缓冲区"的容量, 而整曲是流式推 PCM(没有缓冲区限制), 时长的唯一约束
 * 只是别让校验形同虚设 —— 半速播放时一个全音符(120BPM 下 2s)会变成 4s。
 *
 * MAX_BYTES 与 MAX_NOTES 互相印证: 最长的单个记号约 "1047:10000"(11 字节),
 * 192 * 11 = 2112 < 3072。
 */
#define MINIAPP_BUZZER_SEQ_MAX_NOTES 192u
#define MINIAPP_BUZZER_SEQ_MAX_MS 10000u
#define MINIAPP_BUZZER_SEQ_MAX_BYTES 3072u
/* 流式写入的块长(毫秒)。它决定两件事: 合成的粒度, 以及"中止"被察觉的最长
 * 延迟 —— 每写完一块才检查一次是否该停。 */
#define MINIAPP_BUZZER_SEQ_BLOCK_MS 100u
#define MINIAPP_TTS_TEXT_MAX 512u
#define MINIAPP_TTS_MAX_PENDING 1u
#define MINIAPP_TTS_TIMEOUT_MS 120000u
#define MINIAPP_HTTP_MAX_URL_BYTES 511u
#define MINIAPP_HTTP_MAX_REQUESTS 4u
#define MINIAPP_HTTP_DEFAULT_TIMEOUT_MS 10000u
#define MINIAPP_HTTP_MAX_TIMEOUT_MS 60000u
#define MINIAPP_HTTP_DEFAULT_RESPONSE_BYTES 8192u
#define MINIAPP_HTTP_MAX_RESPONSE_BYTES 32768u
#define MINIAPP_HTTP_MAX_HEADERS 16u
#define MINIAPP_HTTP_MAX_HEADER_BYTES 1024u
#define MINIAPP_HTTP_MAX_BODY_BYTES 8192u
#ifdef CONFIG_MINIAPP_SCREEN
#define MINIAPP_HAS_SCREEN 1
#else
#define MINIAPP_HAS_SCREEN 0
#endif
#ifdef CONFIG_MINIAPP_BUTTON
#define MINIAPP_HAS_BUTTONS 1
#else
#define MINIAPP_HAS_BUTTONS 0
#endif
#ifdef CONFIG_MINIAPP_LED
#define MINIAPP_HAS_LEDS 1
#else
#define MINIAPP_HAS_LEDS 0
#endif
#ifdef CONFIG_MINIAPP_BUZZER
#define MINIAPP_HAS_BUZZER 1
#else
#define MINIAPP_HAS_BUZZER 0
#endif

typedef struct {
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
    uint32_t color;
} miniapp_rect_t;

typedef struct {
    int16_t x;
    int16_t y;
    uint32_t color;
    char text[MINIAPP_TEXT_MAX + 1];
} miniapp_text_t;

typedef struct {
    uint32_t background;
    uint16_t rect_count;
    uint16_t text_count;
    miniapp_rect_t rects[MINIAPP_MAX_RECTS];
    miniapp_text_t texts[MINIAPP_MAX_TEXTS];
} miniapp_scene_t;

typedef struct {
    bool installed;
    bool active;
    uint32_t source_size;
    uint32_t source_hash;
    uint32_t frames;
    uint32_t dropped_frames;
    uint32_t last_frame_ms;
    uint32_t max_frame_ms;
    char last_error[128];
} miniapp_status_t;

/* Strings are bounded UTF-8 bytes; identifiers are opaque, never paths. */
#define MINIAPP_ID_MAX 127
#define MINIAPP_VERSION_MAX 127

typedef struct {
    char id[MINIAPP_ID_MAX + 1];
    char version[MINIAPP_VERSION_MAX + 1];
    char hash[33];
    uint32_t size;
} miniapp_package_t;

#ifdef CONFIG_MINIAPP
#ifdef CONFIG_MINIAPP_ADB_DEBUG
/* Immutable source snapshot; acquire/release may span replacement or exit. */
typedef struct miniapp_source miniapp_source_t;
miniapp_source_t *miniapp_source_acquire(void);
void miniapp_source_release(miniapp_source_t *source);
const char *miniapp_source_data(const miniapp_source_t *source, size_t *size);
#endif
int miniapp_init(void);
bool miniapp_is_active(void);
int miniapp_button_click(const char *button_id);
int miniapp_exit(void);
/* One install at a time, including its download. Never queue a second install. */
bool miniapp_install_begin(void);
void miniapp_install_end(void);
bool miniapp_matches(const miniapp_package_t *package);
int miniapp_install(const miniapp_package_t *package, const char *source,
                    char *error, size_t error_size);
#else
/* 运行时不启用时保留这些入口是为了让"按键来源"(如 src/middleware/gamepad)
 * 无条件编译/链接 —— 没有小应用时按键事件自然无人消费。 */
static inline bool miniapp_is_active(void) { return false; }
static inline int miniapp_button_click(const char *button_id) { (void)button_id; return -1; }
static inline int miniapp_exit(void) { return -1; }
#endif

int miniapp_ui_init(void);
int miniapp_ui_open(void);
int miniapp_ui_close(void);
int miniapp_ui_present(const miniapp_scene_t *scene);
int miniapp_ui_wait_frame(uint32_t timeout_ms);
void miniapp_runtime_ui_closed(void);

#endif
