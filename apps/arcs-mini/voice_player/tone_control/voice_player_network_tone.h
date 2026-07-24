#ifndef VOICE_PLAYER_NETWORK_TONE_H
#define VOICE_PLAYER_NETWORK_TONE_H

/**
 * @file voice_player_network_tone.h
 * @brief 网络、云端和配网状态对应的运行时提示音策略。
 */

/**
 * @brief 初始化网络提示音策略、定时器和消息订阅。
 * @return 0 初始化成功，其他值表示失败。
 */
int voice_player_network_tone_init(void);

#endif /* VOICE_PLAYER_NETWORK_TONE_H */
