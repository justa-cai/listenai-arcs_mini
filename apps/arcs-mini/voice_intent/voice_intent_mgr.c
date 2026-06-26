#define TAG "intent_mgr"

#include "voice_intent_mgr.h"

#include <string.h>
#include <stdio.h>

#include "lisa_log.h"
#include "shell.h"
#include "voice_intent_music.h"
#include "voice_intent_session.h"
#include "voice_intent_photo_flow.h"
#include "voice_intent_alarm.h"
#include "voice_intent_prompt_tone.h"
/* ---- 栈定义 ---- */

#define INTENT_STACK_CAPACITY 8

static voice_intent_t s_stack[INTENT_STACK_CAPACITY];
static int s_depth = 0;

/* ---- 钩子注册表 ---- */

static voice_intent_ops_t s_ops[INTENT_COUNT];

static const char *intent_name(voice_intent_t t)
{
    switch (t) {
    case INTENT_NONE:           return "NONE";
    case INTENT_MUSIC:          return "MUSIC";
    case INTENT_VOICE_SESSION:  return "VOICE_SESSION";
    case INTENT_PHOTO_FLOW:     return "PHOTO_FLOW";
    case INTENT_ALARM:          return "ALARM";
    case INTENT_PROMPT_TONE:    return "PROMPT_TONE";
    default:                    return "?";
    }
}

static void call_hook(voice_intent_t type, voice_intent_hook_t hook, const char *hook_name)
{
    if (hook) {
        LISA_LOGI(TAG, "hook: %s -> %s", intent_name(type), hook_name);
        hook();
    }
}

int voice_intent_register_ops(const voice_intent_ops_t *ops)
{
    if (!ops || ops->type <= INTENT_NONE || ops->type >= INTENT_COUNT) {
        LISA_LOGW(TAG, "register_ops: invalid ops or type");
        return -1;
    }

    s_ops[ops->type] = *ops;
    LISA_LOGI(TAG, "registered ops for %s", intent_name(ops->type));
    return 0;
}

/* ---- 栈操作 ---- */

int voice_intent_push(voice_intent_t type)
{
    if (type == INTENT_NONE || type >= INTENT_COUNT) {
        LISA_LOGW(TAG, "push: invalid type=%d", type);
        return -1;
    }

    if (voice_intent_contains(type)) {
        LISA_LOGI(TAG, "push: %s already in stack, skip", intent_name(type));
        return 0;
    }

    if (s_depth >= INTENT_STACK_CAPACITY) {
        LISA_LOGE(TAG, "push: stack full, cannot push %s", intent_name(type));
        return -1;
    }

    voice_intent_t old_top = (s_depth > 0) ? s_stack[s_depth - 1] : INTENT_NONE;

    /* 按优先级找到插入位置：栈从低到高有序，栈顶优先级最高 */
    int insert_at = s_depth;
    for (int i = 0; i < s_depth; i++) {
        if (s_stack[i] > type) {
            insert_at = i;
            break;
        }
    }

    for (int i = s_depth; i > insert_at; i--) {
        s_stack[i] = s_stack[i - 1];
    }
    s_stack[insert_at] = type;
    s_depth++;

    voice_intent_t new_top = s_stack[s_depth - 1];
    LISA_LOGI(TAG, "push: %s (depth=%d)", intent_name(type), s_depth);
    voice_intent_dump();

    if (new_top != old_top) {
        call_hook(old_top, s_ops[old_top].on_preempted, "on_preempted");
        call_hook(new_top, s_ops[new_top].on_enter, "on_enter");
    }
    return 0;
}

int voice_intent_pop(voice_intent_t type)
{
    if (type == INTENT_NONE || type >= INTENT_COUNT) {
        LISA_LOGW(TAG, "pop: invalid type=%d", type);
        return -1;
    }

    if (s_depth == 0) {
        LISA_LOGW(TAG, "pop: stack empty, cannot pop %s", intent_name(type));
        return -1;
    }

    int found = -1;
    for (int i = s_depth - 1; i >= 0; i--) {
        if (s_stack[i] == type) {
            found = i;
            break;
        }
    }

    if (found < 0) {
        LISA_LOGW(TAG, "pop: %s not found in stack", intent_name(type));
        voice_intent_dump();
        return -1;
    }

    if (found != s_depth - 1) {
        LISA_LOGW(TAG, "pop: %s at index %d (not top=%s), removing from middle",
                  intent_name(type), found, intent_name(s_stack[s_depth - 1]));
    }

    bool top_changed = (found == s_depth - 1);

    call_hook(type, s_ops[type].on_exit, "on_exit");

    for (int i = found; i < s_depth - 1; i++) {
        s_stack[i] = s_stack[i + 1];
    }
    s_depth--;

    if (top_changed && s_depth > 0) {
        voice_intent_t new_top = s_stack[s_depth - 1];
        call_hook(new_top, s_ops[new_top].on_resumed, "on_resumed");
    }

    LISA_LOGI(TAG, "pop: %s (depth=%d)", intent_name(type), s_depth);
    voice_intent_dump();
    return 0;
}

voice_intent_t voice_intent_top(void)
{
    if (s_depth == 0) {
        return INTENT_NONE;
    }
    return s_stack[s_depth - 1];
}

bool voice_intent_contains(voice_intent_t type)
{
    for (int i = 0; i < s_depth; i++) {
        if (s_stack[i] == type) {
            return true;
        }
    }
    return false;
}

int voice_intent_depth(void)
{
    return s_depth;
}

/* ---- Phase 1 双写校验 ---- */

void voice_intent_validate_with(bool hold_tts, bool hold_photo, bool alarm_active, bool resume_music)
{
    bool stack_has_voice  = voice_intent_contains(INTENT_VOICE_SESSION);
    bool stack_has_photo  = voice_intent_contains(INTENT_PHOTO_FLOW);
    bool stack_has_alarm  = voice_intent_contains(INTENT_ALARM);
    bool stack_has_music  = voice_intent_contains(INTENT_MUSIC);
    bool stack_music_held = stack_has_music && voice_intent_top() != INTENT_MUSIC;

    if (stack_has_voice != hold_tts) {
        LISA_LOGW(TAG, "VALIDATE MISMATCH: VOICE_SESSION in stack=%d but hold_tts=%d",
                  stack_has_voice, hold_tts);
    }
    if (stack_has_photo != hold_photo) {
        LISA_LOGW(TAG, "VALIDATE MISMATCH: PHOTO_FLOW in stack=%d but hold_photo=%d",
                  stack_has_photo, hold_photo);
    }
    if (stack_has_alarm != alarm_active) {
        LISA_LOGW(TAG, "VALIDATE MISMATCH: ALARM in stack=%d but alarm_active=%d",
                  stack_has_alarm, alarm_active);
    }
    if (stack_music_held != resume_music) {
        LISA_LOGW(TAG, "VALIDATE MISMATCH: music_held=%d but resume_music=%d",
                  stack_music_held, resume_music);
    }
}

/* ---- 调试 ---- */

void voice_intent_dump(void)
{
    if (s_depth == 0) {
        LISA_LOGI(TAG, "dump: [empty]");
        return;
    }

    char buf[128];
    int pos = 0;

    for (int i = 0; i < s_depth; i++) {
        const char *name = intent_name(s_stack[i]);
        int n = snprintf(buf + pos, sizeof(buf) - pos,
                         "%s%s", (i > 0 ? " -> " : ""), name);
        if (n > 0) {
            pos += n;
        }
        if (pos >= (int)sizeof(buf) - 1) {
            break;
        }
    }
    LISA_LOGI(TAG, "dump: [%s]", buf);
}

/* ---- 初始化 ---- */

int voice_intent_mgr_init(void)
{
    /* 注册各 intent 的钩子（含各模块内部的 ebus 订阅） */
    voice_intent_music_register();
    voice_intent_session_register();
    voice_intent_photo_flow_register();
    voice_intent_alarm_register();
    voice_intent_prompt_tone_register();

    LISA_LOGI(TAG, "voice intent mgr init: done");
    return 0;
}

/* ---- shell 命令 ---- */

static int intent_cmd_handler(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (s_depth == 0) {
        shellPrint(shellGetCurrent(), "intent stack: [empty]\n");
        return 0;
    }

    shellPrint(shellGetCurrent(), "intent stack (depth=%d):\n", s_depth);
    for (int i = 0; i < s_depth; i++) {
        shellPrint(shellGetCurrent(), "  [%d] %s\n", i, intent_name(s_stack[i]));
    }
    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN,
                 intent, intent_cmd_handler, show voice intent stack);
