/**
 * @file listen_volume.h
 * @brief
 * @author wqliu (wqliu@listenai.com)
 * @version 1.0
 * @date 2022-09-29
 *
 * @copyright Copyright (c) 2022 安徽聆思智能科技有限公司
 *
 */
#ifndef __LISTENAI_VOLUME_H__
#define __LISTENAI_VOLUME_H__

typedef enum {
	VOLUME_LEV1 = 0,
	VOLUME_LEV2,
	VOLUME_LEV3,
	VOLUME_LEV4,
	VOLUME_LEV5,
} volume_lev;

/**
 * @brief 音量设置初始化
 */
void listen_volume_init(void);

/**
 * @brief  设置音量
 * @param  vol			[in]0-100
 */
void listen_set_volume(int vol);

int listen_get_volume(void);

/**
 * @brief  音量调整, 正负值
 * @param  adj			音量调整值
 */
void listen_vol_adjust(int adj);

void listen_vol_mute(void);

void listen_vol_cancel_mute();

#endif
