#ifndef __VOICE_CONTROL_H__
#define __VOICE_CONTROL_H__

#include <stdint.h>

/**
 * @brief 获取当前TTS音色
 *
 * @return 当前音色ID字符串
 */
const char* voice_control_get_current_voice(void);

#endif
