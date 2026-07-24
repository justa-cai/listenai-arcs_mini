#ifndef VOICE_PLAYER_ALARM_H
#define VOICE_PLAYER_ALARM_H

/**
 * @file voice_player_alarm.h
 * @brief 闹钟播放编排的应用内接口。
 */

/**
 * @brief 初始化闹钟播放编排并注册 alarm_ring 回调。
 * @return 0 初始化成功，其他值表示失败。
 */
int voice_player_alarm_init(void);

#endif /* VOICE_PLAYER_ALARM_H */
