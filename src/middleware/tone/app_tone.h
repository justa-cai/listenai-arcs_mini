#ifndef __LISTENAI_APP_TONE_H__
#define __LISTENAI_APP_TONE_H__

#include <stdint.h>
#include <stdbool.h>

#define MAX_URL_LEN (32)

typedef struct {
    uint16_t tone_id;
    char *url;
} tone_dsc_t;

typedef struct {
    uint16_t total_cnt;
    tone_dsc_t *item;
} tone_hdr_t;

/**
 * @brief 	音频初始化
 * @param	flash_addr	Flash起始地址
 * @param	flash_size	Flash大小
 * @return 	0: 加载成功
 * @return  -1: 加载失败
 */
int app_tone_init(uint32_t flash_addr, uint32_t flash_size);

/**
 * @brief 	根据离线音频 ID 获取离线音频 URL
 * @param  	tone_id		音频 ID
 * @return 	离线音频 URL
 */
char *app_tone_get_url(uint16_t tone_id);

#endif
