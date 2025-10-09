#ifndef LED_CONTROL_H
#define LED_CONTROL_H

/**
 * @brief 获取LED当前状态
 * 
 * @return int LED状态：1=开启，0=关闭
 */
int get_led_state(void);

/**
 * @brief 获取LED当前闪烁模式
 * 
 * @return const char* 闪烁模式："off", "normal", "fast", "slow"
 */
const char* get_led_blink_mode(void);

#endif // LED_CONTROL_H
