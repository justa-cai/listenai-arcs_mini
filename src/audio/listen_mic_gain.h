/**
 *
 */
#ifndef __LISTENAI_MIC_GAIN_H__
#define __LISTENAI_MIC_GAIN_H__

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 麦克风增益初始化
 */
void listen_mic_gain_init(void);

/**
 * @brief  设置麦克风增益
 * @param  gain			[in]0-100
 */
void listen_mic_gain_set(int gain);
int listen_mic_gain_get(void);

// 根据音量动态调整 AEC 增益
void listen_update_aec_by_volume(int volume);

void listen_mic_mute(bool is_mute);
int listen_mic_mute_get(void);

#ifdef __cplusplus
}
#endif

#endif
