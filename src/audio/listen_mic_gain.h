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
 * @brief 音量设置初始化
 */
void listen_mic_gain_init(void);

/**
 * @brief  设置音量
 * @param  gain			[in]0-100
 */
void listen_mic_gain_set(int gain);
int listen_mic_gain_get(void);


void listen_mic_mute(bool is_mute);
int listen_mic_mute_get();

#ifdef __cplusplus
}
#endif

#endif
