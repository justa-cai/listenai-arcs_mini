#pragma once

#include <stdbool.h>

/** 注册 INTENT_MUSIC 的入/出栈钩子，由 voice_player_ready 调用 */
int voice_intent_music_register(void);

/** 用户主动暂停标记：设置后 on_resumed/on_enter 不会自动恢复播放 */
void voice_intent_music_set_user_paused(bool paused);

