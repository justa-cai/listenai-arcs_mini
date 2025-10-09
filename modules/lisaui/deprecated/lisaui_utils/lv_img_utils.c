/**
 * @file lv_img_utils.c
 * @author TsMax (QinYUN575@Foxmail.com)
 * @brief
 * @version 0.1
 * @date 2024-02-23
 *
 * @copyright Copyright (c) 2021 - 2024 shenzhen listenai co., ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "lv_img_utils.h"

#if CONFIG_LISAUI_ENV_ARCS_SDK
	#include "lisa_log.h"
	// #define LOG_INF CLOG
	#define LOG_INF
#else
	// #define LOG_INF printf
	#define LOG_INF LV_LOG_USER
#endif


int lv_img_png_src_init(lv_img_dsc_t *dsc, const void *data, uint32_t size)
{
	img_header_t header;
	LOG_INF("lv_img_png_src_init: data=%p, size=%d\r\n", data, size);

	if (img_png_get_info(data, size, &header))
	{
		LOG_INF("lv_img_png_src_init: get png info failed\r\n");
		return -1;
	}

	dsc->data = data;
	dsc->data_size = size;
	dsc->header.w = header.width;
	dsc->header.h = header.height;
	dsc->header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA;

	LOG_INF("lv_img_png_src_init: w=%d, h=%d\r\n", dsc->header.w, dsc->header.h);
	return 0;
}

int lv_img_png_res_init(lisaui_ui_res_img_png_t *res_img)
{
	LOG_INF("lv_img_png_res_init: res_img=%p\r\n", res_img);
	if (res_img == NULL)
	{
		LOG_INF("lv_img_png_res_init: res_img is NULL\r\n");
		return -1;
	}

	if (res_img->img == NULL || res_img->data == NULL || res_img->size == 0)
	{
		LOG_INF("lv_img_png_res_init: img or data is NULL\r\n");
		return -1;
	}

	lv_img_dsc_t *dsc = res_img->img;
	const void *data = res_img->data;
	uint32_t size = res_img->size;

	return lv_img_png_src_init(dsc, data, size);
}

int lv_img_gif_src_init(lv_img_dsc_t *dsc, const void *data, uint32_t size)
{
	img_header_t header;
	LOG_INF("lv_img_gif_src_init: data=%p, size=%d\r\n", data, size);

	if (img_gif_get_info(data, size, &header))
	{
		LOG_INF("lv_img_gif_src_init: get gif info failed\r\n");
		return -1;
	}

	dsc->data = data;
	dsc->data_size = size;
	dsc->header.w = header.width;
	dsc->header.h = header.height;
	dsc->header.cf = LV_IMG_CF_RAW;

	LOG_INF("lv_img_gif_src_init: w=%d, h=%d\r\n", dsc->header.w, dsc->header.h);
	return 0;
}

int lv_img_jpg_src_init(lv_img_dsc_t *dsc, const void *data, uint32_t size)
{
	img_header_t header;
	LOG_INF("lv_img_jpg_src_init: data=%p, size=%d\r\n", data, size);

	if (img_jpg_get_info(data, size, &header))
	{
		LOG_INF("lv_img_jpg_src_init: get jpg info failed\r\n");
		return -1;
	}

	dsc->data = data;
	dsc->data_size = size;
	dsc->header.w = header.width;
	dsc->header.h = header.height;
	dsc->header.cf = LV_IMG_CF_RAW;

	LOG_INF("lv_img_jpg_src_init: w=%d, h=%d\r\n", dsc->header.w, dsc->header.h);
	return 0;
}