#pragma once

#include <stdint.h>

struct wakeup_algo_res {
    uint8_t *addr;
    uint32_t size;
};

struct wakeup_algo_resources {
    struct wakeup_algo_res mlp;
    struct wakeup_algo_res wrap;
};

typedef enum{
    APP_WAKEUP_SENSITIVITY_LEVEL_0 = 0, /*low*/
    APP_WAKEUP_SENSITIVITY_LEVEL_1,     /*medium*/
    APP_WAKEUP_SENSITIVITY_LEVEL_2      /*high*/
}app_wakeup_sensitivity_level_e;

int app_wakeup_init(struct wakeup_algo_resources *res);

#ifdef CONFIG_BOARD_ARCS_MINI
int app_wakeup_stop(void);
#endif

/**
 * @brief 设置唤醒灵敏度级别
 * @param level 灵敏度级别: LEVEL_0(低), LEVEL_1(中), LEVEL_2(高)
 * @return 0: 成功, 其他值: 失败
 */
int app_wakeup_sensitivity_level_set(app_wakeup_sensitivity_level_e level);

/**
 * @brief 获取当前唤醒灵敏度级别
 * @return 当前灵敏度级别
 */
app_wakeup_sensitivity_level_e app_wakeup_sensitivity_level_get(void);