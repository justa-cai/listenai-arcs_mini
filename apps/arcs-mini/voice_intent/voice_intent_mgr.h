#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * voice_intent_mgr — 交互意图栈（有序栈，优先级从低到高，栈顶最高）
 *
 * 设计文档：voice-intent-stack.md
 *
 * 栈始终按优先级排序（MUSIC < VOICE_SESSION < ALARM < PHOTO_FLOW < PROMPT_TONE）。
 * push 按优先级插入，只有真正抢占栈顶时才触发 on_preempted/on_enter。
 * pop 只在栈顶变化时才触发 on_resumed。栈空时 voice_intent_top() 返回 INTENT_NONE。
 */

typedef enum {
    INTENT_NONE = 0,
    INTENT_MUSIC,            /* 长期播放，可被任何上层抢占暂停 */
    INTENT_VOICE_SESSION,    /* wakeup → ASR → TTS 一轮会话 */
    INTENT_ALARM,            /* 闹钟响铃 */
    INTENT_PHOTO_FLOW,       /* take_photo: preview → capture → upload → result TTS */
    INTENT_PROMPT_TONE,      /* 短促本地提示音，最高优先级，播完后恢复被抢占意图 */
    INTENT_COUNT
} voice_intent_t;

/* ---- 栈操作 ---- */

/** push 一个意图到栈顶。若栈满返回 -1。 */
int  voice_intent_push(voice_intent_t type);

/** pop 指定意图。若 type 与栈顶不匹配，打 WARN 但仍执行 pop。返回 0 成功，-1 失败。 */
int  voice_intent_pop(voice_intent_t type);

/** 返回栈顶意图类型，栈空返回 INTENT_NONE */
voice_intent_t voice_intent_top(void);

/** 查询栈中是否包含指定意图 */
bool voice_intent_contains(voice_intent_t type);

/** 返回栈中元素个数 */
int  voice_intent_depth(void);

/* ---- 生命周期 ---- */

/** 初始化：订阅 ebus 消息，开始自动维护栈 */
int  voice_intent_mgr_init(void);

/* ---- Phase 1 双写校验 ---- */

/**
 * 对比意图栈派生状态与旧标志，不一致时打 LOGW。
 *
 * 调用时机：voice_player 中每次修改 s_content_hold_for_tts / _photo /
 *           s_alarm_session_active 等标志之后。
 *
 * @param hold_tts     s_content_hold_for_tts 的当前值
 * @param hold_photo   s_content_hold_for_photo 的当前值
 * @param alarm_active s_alarm_session_active 的当前值
 * @param resume_music s_resume_music_after_voice 的当前值
 */
void voice_intent_validate_with(bool hold_tts, bool hold_photo, bool alarm_active, bool resume_music);

/* ---- 意图钩子：intent 变化时驱动播放器操作 ---- */

typedef void (*voice_intent_hook_t)(void);

typedef struct {
    voice_intent_t   type;
    voice_intent_hook_t on_enter;       /* 成为栈顶时调用 */
    voice_intent_hook_t on_preempted;   /* 更高优先级意图压入时调用 */
    voice_intent_hook_t on_resumed;     /* 上层意图弹出、本意图重新成为栈顶时调用 */
    voice_intent_hook_t on_exit;        /* 从栈中移除时调用 */
} voice_intent_ops_t;

/** 注册意图钩子。每个 type 最多注册一组，重复注册会覆盖。 */
int  voice_intent_register_ops(const voice_intent_ops_t *ops);

/* ---- 调试 ---- */

/** dump 当前栈内容到日志 */
void voice_intent_dump(void);
