#ifndef __LISTENAI_APP_TONE_H__
#define __LISTENAI_APP_TONE_H__

#include <stdint.h>
#include <stdbool.h>

#define MAX_URL_LEN (32)

#define TONE_MAGIC_NONE         (0x0)
#define TONE_MAGIC_DEF          (0x30100000)
#define TONE_MAGIC_TWO          (0x30100000)
#define TONE_SKIP_COUNT_NONE    (0)
#define TONE_SKIP_COUNT_TWO     (1000)

typedef struct {
	uint16_t tone_id;
	char *url;
} tone_dsc_t;

typedef struct {
	uint16_t total_cnt;
	tone_dsc_t *item;
} tone_hdr_t;

/**
 * @brief	使用默认地址初始化
 * @return 	0: 初始化成功
 * @return  -1: 初始化失败
 */
int app_tone_default_init();

/**
 * @brief 	音频初始化
 * @param	flash_addr	Flash起始地址
 * @return 	0: 加载成功
 * @return  -1: 加载失败
 */
int app_tone_init(uint32_t flash_offser);

/**
 * @brief 	根据离线音频 ID 获取离线音频 URL
 * @param  	tone_id		音频 ID
 * @return 	离线音频 URL 
 */
char *app_tone_get_url(uint16_t tone_id);

/**
 * @brief 	重新加载离线音频
 * @param  	type		加载类型
 * @param	custom_addr	自定义地址
 * @return 	0: 重载成功
 * @return  -1: 重载失败
 */
int app_tone_reload(uint32_t custom_addr);

/**
 * @brief 	标记跳过多少音频
 * @param	skip_count	跳过数量
 */
void app_tone_skip_count(int skip_count);

#endif
