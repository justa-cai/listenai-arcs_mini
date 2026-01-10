#include <stdio.h>
#include <string.h>
#include "app_tone.h"
#include "lisa_log.h"
#include "lisa_mem.h"
#include "config_parser.h"
#define TAG "tone"

static tone_hdr_t *s_tone_hdr = NULL;
// 如果是多套音频打包成一个的情况
// 第二套音频需要跳过的数量
static uint32_t s_skip_count = 0;

int app_tone_default_init()
{
	const Config *config = config_get();
	void *tone_addr = NULL;

	if (config && config->resources.count > 0)
	{
		for (size_t i = 0; i < config->resources.count; i++)
		{
			const ResourceConfig *res = &config->resources.items[i];
			if (strcmp(res->name, "tone") == 0)
			{
				tone_addr = (void *)res->address;
			}
		}
	}

	int ret = app_tone_init((uint32_t)tone_addr);
	if (ret != 0)
	{
		LISA_LOGE(TAG, "tone default init fail");
		return -1;
	}

	return 0;
}

static void _app_tone_uninit()
{
	if (s_tone_hdr)
	{
		if (s_tone_hdr->item)
		{
			for (int i = 0; i < s_tone_hdr->total_cnt; i++) {
				if (s_tone_hdr->item[i].url) {
					lisa_mem_free(s_tone_hdr->item[i].url);
				}
			}
			lisa_mem_free(s_tone_hdr->item);
		}

		lisa_mem_free(s_tone_hdr);
		s_tone_hdr = NULL;
	}
}

int app_tone_init(uint32_t flash_addr)
{
	// 先逆初始化
	_app_tone_uninit();

	uint32_t total_mem_size = 0;

	s_tone_hdr = lisa_mem_calloc(1, sizeof(tone_hdr_t));
	if (!s_tone_hdr)
	{
		return -1;
	}

	total_mem_size += sizeof(tone_hdr_t);

	uint32_t buf_len;
	uint32_t *p_buf_len = NULL;
	uint32_t tone_offset = flash_addr;
	uint32_t tone_len = 0;
	//获取tone的个数
	uint16_t buf[1] = {0};
	// 用于组tone地址的临时Buffer
	char tmp_buf[MAX_URL_LEN];
	uint32_t tmp_size = 0;

	memcpy((void *)buf, (void *)tone_offset, sizeof(buf));
	s_tone_hdr->total_cnt = buf[0];

#if 0
	if (s_tone_hdr->total_cnt > MAX_TONE_CNT)
	{
		LISA_LOGE(TAG, "tone count %d invalid", s_tone_hdr->total_cnt);
		goto TONE_INIT_FAIL;
	}
#endif

	LISA_LOGI(TAG, "tone init addr: %p, count: %d", flash_addr, s_tone_hdr->total_cnt);

	total_mem_size += (s_tone_hdr->total_cnt * sizeof(tone_dsc_t));
	s_tone_hdr->item = lisa_mem_calloc(s_tone_hdr->total_cnt, sizeof(tone_dsc_t));
	if (!s_tone_hdr->item)
	{
		LISA_LOGE(TAG, "tone item no mem...");
		goto TONE_INIT_FAIL;
	}

	buf_len = buf[0] * sizeof(uint32_t);
	p_buf_len = (uint32_t *)lisa_mem_alloc(buf_len);
	if (!p_buf_len)
	{
		LISA_LOGE(TAG, "tone buf no mem...");
		goto TONE_INIT_FAIL;
	}

	//此时取tone.bin的头信息;
	tone_offset += sizeof(buf);
	memcpy((void *)p_buf_len, (void *)tone_offset, buf_len);
	tone_offset += buf_len;

	for (int i = 0; i < s_tone_hdr->total_cnt; i++)
	{
		s_tone_hdr->item[i].tone_id = i;
		tone_len = p_buf_len[i];

		if (tone_len > 0) {
			memset(tmp_buf, 0, MAX_URL_LEN);
			sprintf(tmp_buf, "mem://addr=%ldsize=%d", (long)tone_offset, tone_len);
			tmp_size = strlen(tmp_buf) + 1;
			s_tone_hdr->item[i].url = lisa_mem_alloc(tmp_size);
			LISA_ASSERT(s_tone_hdr->item[i].url, "alloc tone url fail");
			strcpy(s_tone_hdr->item[i].url, tmp_buf);

			total_mem_size += tmp_size;
		} else {
			s_tone_hdr->item[i].url = NULL;
		}

		tone_offset += tone_len;
	}

	if (p_buf_len)
	{
		lisa_mem_free(p_buf_len);
	}

	LISA_LOGD(TAG, "tone init use mem: %d", total_mem_size);

	return 0;

TONE_INIT_FAIL:
	_app_tone_uninit();
	return -1;
}

static tone_dsc_t *__get_tone_by_id(uint16_t tone_id)
{
	tone_dsc_t *item = NULL;
	if (s_tone_hdr && s_tone_hdr->item)
	{
		if (tone_id >= s_tone_hdr->total_cnt)
		{
			return NULL;
		}

		item = &s_tone_hdr->item[tone_id];
		if (item->url == NULL)
		{
			return NULL;
		}
	}
	return item;
}

char * app_tone_get_url(uint16_t tone_id)
{
// 如果是第三种配置方案
#if (VOICEID_TYPE == 3)
	tone_dsc_t *dsc_t = NULL;
	if (s_skip_count > 0) {
		uint16_t next_id = tone_id + s_skip_count;
		LISA_LOGV(TAG, "tone_id: %d, next_id: %d", tone_id, next_id);
		dsc_t = __get_tone_by_id(next_id);
		if (dsc_t) {
			return dsc_t->url;
		} else {
			LISA_LOGV(TAG, "get next_id: %d fail, use tone_id: %d", next_id, tone_id);
			// 如果第二套音频获取失败情况下
			// 从第一套音频获取, 比如公共的音频
			dsc_t = __get_tone_by_id(tone_id);
			if (dsc_t) {
				return dsc_t->url;
			}
		}
	} else {
		// 不需要跳过时, 直接从第一套音频获取
		dsc_t = __get_tone_by_id(tone_id);
		if (dsc_t) {
			return dsc_t->url;
		}
	}

	return NULL;
#else
	tone_dsc_t *dsc_t = __get_tone_by_id(tone_id);
	if (dsc_t) {
		return dsc_t->url;
	}
	return NULL;
#endif
}

int app_tone_reload(uint32_t custom_addr)
{
	LISA_LOGD(TAG, "app tone reload by 0x%X", custom_addr);
	int ret = app_tone_init(custom_addr);
	if (ret == -1)
	{
		app_tone_default_init();
		return -1;
	}
	return 0;
}

void app_tone_skip_count(int skip_count)
{
	s_skip_count = skip_count;
}

int app_tone_override(uint16_t tone_id, const void *addr, uint32_t size)
{
	tone_dsc_t *item = __get_tone_by_id(tone_id);
	if (!item) {
		return -1;
	}

	if (item->url) {
		lisa_mem_free(item->url);
		item->url = NULL;
	}

	char tmp_buf[MAX_URL_LEN];
	uint32_t tmp_size = 0;
	memset(tmp_buf, 0, MAX_URL_LEN);
	sprintf(tmp_buf, "mem://addr=%usize=%u", (uint32_t)addr, size);
	tmp_size = strlen(tmp_buf) + 1;
	item->url = lisa_mem_alloc(tmp_size);
	LISA_ASSERT(item->url, "alloc tone url fail");
	strcpy(item->url, tmp_buf);

	return 0;
}
