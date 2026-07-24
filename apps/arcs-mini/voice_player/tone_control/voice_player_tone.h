#ifndef VOICE_PLAYER_TONE_H
#define VOICE_PLAYER_TONE_H

/**
 * @file voice_player_tone.h
 * @brief 提示音队列执行器的应用内生命周期接口。
 */

typedef enum {
    VOICE_PLAYER_TONE_SOURCE_DEFAULT = 0,
    VOICE_PLAYER_TONE_SOURCE_WAKEUP,
    VOICE_PLAYER_TONE_SOURCE_ALARM,
} voice_player_tone_source_t;

/** tone 自然播放完成事件携带的来源信息。 */
typedef struct {
    voice_player_tone_source_t source;
} voice_player_tone_completed_t;

/**
 * @brief 初始化提示音执行器并注册 tone_player 事件回调。
 * @return 0 初始化成功，其他值表示失败。
 */
int voice_player_tone_init(void);

/** @brief 停止当前提示音并清空等待队列。 */
void voice_player_tone_stop(void);

/**
 * @brief 提交唤醒提示音。
 *
 * 清空尚未播放的提示音并打断当前提示音，立即播放本次唤醒反馈；唤醒
 * 提示音播放期间收到的重复唤醒请求会被忽略。普通提示音仍可在其后排队。
 *
 * @param url 提示音资源 URL。
 */
void voice_player_play_wakeup_tone_url(const char *url);

/** @brief 提交闹钟提示音，用于关联闹钟播放阶段完成事件。 */
void voice_player_play_alarm_tone_url(const char *url);

#endif /* VOICE_PLAYER_TONE_H */
