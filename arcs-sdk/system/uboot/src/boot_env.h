#ifndef __BOOT_ENV_H__
#define __BOOT_ENV_H__

#include <stdint.h>
#include <stdbool.h>
#include "boot_partab.h"

/*
 * EasyFlash环境变量键名
 */
#define BOOT_ENV_KEY_MODE       "boot.mode"     /* 启动模式 */
#define BOOT_ENV_KEY_ACTIVE     "boot.active"   /* 当前活动槽位 */

/*
 * 启动模式值
 */
#define BOOT_MODE_VAL_NORMAL    "normal"
#define BOOT_MODE_VAL_UPDATE    "update"

/*
 * 槽位值
 */
#define BOOT_SLOT_VAL_A         "A"
#define BOOT_SLOT_VAL_B         "B"
#define BOOT_SLOT_VAL_NONE      "none"

/*
 * 初始化启动环境
 * @param store_base control store 起始地址 (从boot_config读取)
 * @return 0成功，其他失败
 */
int boot_env_init(uint32_t store_base);

/*
 * 获取启动模式
 */
boot_mode_t boot_env_get_mode(void);

/*
 * 是否处于升级模式
 */
bool boot_env_is_update_mode(void);

/*
 * 设置启动模式
 */
int boot_env_set_mode(boot_mode_t mode);

/*
 * 进入升级/正常模式
 */
int boot_env_enter_update_mode(void);
int boot_env_enter_normal_mode(void);

/*
 * 获取当前活动槽位
 */
boot_slot_t boot_env_get_active_slot(void);

/*
 * 设置当前活动槽位
 */
int boot_env_set_active_slot(boot_slot_t slot);


#endif /* __BOOT_ENV_H__ */
