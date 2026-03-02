#include "cJSON.h"
#include "stdbool.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "evs_utils.h"
#include "app_algo.h"
#include "app_client.h"
#include <string.h>

#define TAG "algo"

typedef struct wakeup_play_id
{
    char *keyword;
    int16_t audio_id;
} wakeup_play_id_t;

/** 是否为数字 */
static bool is_digit(char str) { return (str >= '0' && str <= '9'); }

/** 是否为特殊字符 */
static bool is_special(char str) { return (str == '\r'); }

const wakeup_play_id_t play_id_table[] = {
	{"xiao xian xiao xian", 0},
	{"da kai kong tiao", 1},
	{"guan bi kong tiao", 2},
	{"guan bi ping xian", 3},
	{"zhi leng mo shi", 4},
	{"zhi re mo shi", 5},
	{"song feng mo shi", 6},
	{"chu shi mo shi", 7},
	{"da kai jing hua", 8},
	{"guan bi jing hua", 9},
	{"da kai sha jun", 10},
	{"guan bi sha jun", 11},
	{"di su feng", 12},
	{"zhong su feng", 13},
	{"zui xiao feng", 14},
	{"gao su feng", 15},
	{"zui da feng", 16},
	{"da kai deng guang", 17},
	{"guan bi deng guang", 18},
	{"da kai shui mian", 19},
	{"guan bi shui mian", 20},
	{"zhao ming mo shi", 21},
	{"hui jia mo shi", 22},
	{"li jia mo shi", 23},
	{"quan kai mo shi", 24},
	{"quan guan mo shi", 25},
	{"yue du mo shi", 26},
	{"ying yin mo shi", 27},
	{"shui mian mo shi", 28},
	{"qi chuang mo shi", 29},
	{"jiu can mo shi", 30},
	{"er shi du", 31},
	{"er shi yi du", 32},
	{"er shi er du", 33},
	{"er shi san du", 34},
	{"er shi si du", 35},
	{"er shi wu du", 36},
	{"da kai chuang lian", 37},
	{"guan bi chuang lian", 38},
	{"da kai yang tai deng", 39},
	{"guan bi yang tai deng", 40},
	{"da kai yao tou", 41},
	{"guan bi yao tou", 42},
	{"tiao gao wen du", 43}, {"sheng gao wen du", 43},
	{"tiao di wen du", 44}, {"jiang di wen du", 44},
	{"da kai shang xia bai feng", 45},
	{"ting zhi shang xia bai feng", 46},
	{"da kai zuo you bai feng", 47},
	{"ting zhi zuo you bai feng", 48},
	{"kai qi bai feng", 49},
	{"ting zhi bai feng", 50},
	{"zui da yin liang", 51},
	{"zui xiao yin liang", 52},
	{"zeng da yin liang", 53},
	{"jian xiao yin liang", 54},
	{"da kai fu re", 55},
	{"guan bi fu re", 56},
	{"da kai ping xian", 57},
	{"NULL", -1}, // 结尾标记
};

/**
 * 移除字符串中的数字
 * @param input     待处理字符串
 * @return          处理后的字符串
 */
static char* __remove_digits_and_special(char* input)
{
    char* dest = input;
    char* src = input;

    while(*src) {
        if(is_digit(*src) || is_special(*src)) { src++; continue; }
        *dest++ = *src++;
    }
    *dest = '\0';
    return input;
}

int app_algo_keyword_and_kid_extract(const uint8_t *const in, uint8_t *out, int max_len)
{
	int ret = -1;
	if (in && out) {
		cJSON *root = cJSON_Parse(in);
		if (root) {
			cJSON *rlt_json = cJSON_GetObjectItem(root, "rlt");
			if (rlt_json) {
				if (cJSON_GetArraySize(rlt_json) > 0) {
					cJSON *rlt0_json = cJSON_GetArrayItem(rlt_json, 0);
					if (rlt0_json) {
						cJSON *key_json = cJSON_GetObjectItem(rlt0_json, "keyword");
						cJSON *kid_json = cJSON_GetObjectItem(rlt0_json, "iresid");
						cJSON *threshlod_json = cJSON_GetObjectItem(rlt0_json, "ncm");
						if (key_json && threshlod_json) {
							char *proc_keyword = __remove_digits_and_special(key_json->valuestring);
							if (proc_keyword) {
								ret = 0;
								const int kw_len = strlen(proc_keyword);
								if (kw_len < max_len) {
									memcpy(out, proc_keyword, kw_len + 1);
								} else {
									strncpy(out, proc_keyword, max_len);
								}
							}
						}
					}
				}
			}
			cJSON_Delete(root);
		}
	}

	return ret;
}

int local_tone_get(const uint8_t *keyword)
{
	for (int i = 0; play_id_table[i].audio_id != -1; i++) {
		if (strcmp(keyword, play_id_table[i].keyword) == 0) {
			LISA_LOGD(TAG, "keyword: %s", keyword);
			return play_id_table[i].audio_id;
		}
	}
	return -1;
}

static int __handle_algo_prewakeup_runnable(void *arg)
{
	char *pre_wakeup_info = (char *)arg;
	LISA_LOGD(TAG, "Pre Wakeup: %s", pre_wakeup_info);

	lisa_mem_free(pre_wakeup_info);
	return 0;
}

/**
 * @brief 	复写算法预唤醒处理事件
 * @param	info	算法预唤醒信息
 */
void handle_algo_prewake(char *info)
{
	char *pre_info = lisa_mem_calloc(1, strlen(info) + 1);
	strcpy(pre_info, info);
	if (evs_handler_post_runnable(__handle_algo_prewakeup_runnable, pre_info) != 0)
	{
		lisa_mem_free(pre_info);
	}
}

static int __handle_algo_doa_runnable(void *arg)
{
	uint16_t angle = *((uint16_t *)arg);
	LISA_LOG(TAG, "******** CSK DOA %d ********", angle);

	lisa_mem_free(arg);
	return 0;
}

/**
 * @brief 	复写算法 DOA 事件
 * @param	angle	角度值
 */
void handle_algo_doa(uint16_t angle)
{
	uint16_t *doa = lisa_mem_alloc(sizeof(uint16_t));
	*doa = angle;
	if (evs_handler_post_runnable(__handle_algo_doa_runnable, doa) != 0)
	{
		lisa_mem_free(doa);
	}
}

/**
 * @brief 	复写算法 ESR 结果处理逻辑
 * @param	info	ESR结果字符串
 */
void handle_algo_esr(char *info, uint32_t len)
{
	app_client_wakeup(info, len);
}

/**
 * @brief 	复写算法 ESR 超时逻辑
 */
void handle_algo_esr_timeout()
{
	app_client_algo_timeout();
}

/**
 * @brief 	复写算法录音处理逻辑
*/
void handle_algo_record(const char *audio, int len)
{
	static uint32_t algo_frame_count = 0;
	if (++algo_frame_count % 100 == 1) {
		LISA_LOGI("algo", "handle_algo_record: frame=%lu", algo_frame_count);
	}
	app_client_record(audio, len);
}
