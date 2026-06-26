#pragma once

/**
 * 播放需要抢占音乐的本地提示音。
 *
 * 提示音开始时通过 INTENT_PROMPT_TONE 抢占当前意图；
 * 提示音结束或被停止后释放 intent，由下层意图自行恢复。
 */
void voice_player_play_prompt_tone_url(const char *url);
