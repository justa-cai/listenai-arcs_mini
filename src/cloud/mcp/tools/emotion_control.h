#ifndef EMOTION_CONTROL_H
#define EMOTION_CONTROL_H

/**
 * @brief 获取当前表情状态
 * 
 * @return const char* 当前表情："生气", "开心", "撒娇", "无表情"
 */
const char* get_current_emotion(void);

/**
 * @brief 获取当前表情的英文名称
 * 
 * @return const char* 当前表情英文名："angry", "happy", "cute", "neutral"
 */
const char* get_current_emotion_en(void);

#endif // EMOTION_CONTROL_H
