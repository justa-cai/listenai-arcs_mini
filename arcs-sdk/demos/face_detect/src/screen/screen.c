/*
 * Copyright (c) 2026 Anhui Listenai Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "stdio.h"
#include <string.h>
#include <stdbool.h>

#include "workqueue.h"

#include "screen.h"

#define TAG "screen"

#include "lisa_log.h"

#define REAL_TIME_COMPARE_STATUS (CONFIG_ONLY_FACE_REGISTER)
#define COMPARE_SCORE_THRESHOLD (1.0f * CONFIG_FACE_COMPARE_SCORE_THRESHOLD / 100)

#define DISPLAY_WIDTH   CONFIG_PANEL_ST7789P3_HEIGHT  
#define DISPLAY_HEIGHT  CONFIG_PANEL_ST7789P3_WIDTH

#define PREVIEW_WIDTH  (DISPLAY_WIDTH)
#define PREVIEW_HEIGHT (DISPLAY_HEIGHT)

#define SPAN(n) (8 * n)
#define BIT_MASK(n) ((1 << (n)) - 1)

#define COLOR_RED    (lv_color_hex(0xFF0000))
#define COLOR_GREEN  (lv_color_hex(0x00FF00))
#define COLOR_YELLOW (lv_color_hex(0xFFCC00))
#define COLOR_WHITE  (lv_color_hex(0x999999))

#define MAX_RESULT_CNT (5)

static lv_obj_t *preview;

static lv_obj_t *preview;
static lv_obj_t *face_rect;
static lv_obj_t *detect_status;
#if REAL_TIME_COMPARE_STATUS
static lv_obj_t *compare_status;
#endif
static lv_obj_t *verify_status;
static lv_obj_t *register_status;

static lv_img_dsc_t preview_img;
static __attribute__((section(".psram.bss"))) uint16_t preview_buf[PREVIEW_WIDTH * PREVIEW_HEIGHT];

typedef struct {
    acomp_fd_result_info_t *info;
    uint8_t *img_data;
    uint16_t img_width;
    uint16_t img_height;
    uint32_t img_len;
}screen_msg_t;

static workqueue_t *screen_wq = NULL;

lv_obj_t *screen_create(void)
{
	lv_obj_t *screen = lv_obj_create(NULL);
	lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);

	preview = lv_img_create(screen);
	lv_obj_set_pos(preview, 0, 0);
	lv_obj_set_size(preview, PREVIEW_WIDTH, PREVIEW_HEIGHT);

	lv_obj_t *guide = lv_label_create(screen);
	lv_obj_set_style_text_color(guide, COLOR_RED, LV_PART_MAIN);
	lv_obj_set_size(guide, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
	lv_obj_align(guide, LV_ALIGN_BOTTOM_MID, 0, SPAN(-1));
	lv_label_set_text_static(guide, "K1: REGISTER "
#if REAL_TIME_COMPARE_STATUS == 0
        "| K2: RECOGNIZE"
#endif
#if defined(CONFIG_FD_ANTI_SPOOFING)
					" | K3: IR"
#endif /* CONFIG_FD_ANTI_SPOOFING */
	);

	face_rect = lv_obj_create(screen);
	lv_obj_set_style_bg_opa(face_rect, LV_OPA_TRANSP, LV_PART_MAIN);
	lv_obj_set_style_border_width(face_rect, 1, LV_PART_MAIN);
	lv_obj_set_style_border_color(face_rect, COLOR_YELLOW, LV_PART_MAIN);
	lv_obj_set_style_radius(face_rect, 0, LV_PART_MAIN);
	lv_obj_add_flag(face_rect, LV_OBJ_FLAG_HIDDEN);

	detect_status = lv_label_create(screen);
	lv_obj_set_style_text_color(detect_status, COLOR_WHITE, LV_PART_MAIN);
	lv_obj_set_size(detect_status, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
	lv_obj_align(detect_status, LV_ALIGN_TOP_LEFT, SPAN(1), SPAN(1));
	lv_label_set_text(detect_status, "has_face: 0");

#if REAL_TIME_COMPARE_STATUS
	compare_status = lv_label_create(screen);
	lv_obj_set_style_text_color(compare_status, COLOR_WHITE, LV_PART_MAIN);
	lv_obj_set_size(compare_status, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
	lv_obj_align(compare_status, LV_ALIGN_TOP_LEFT, SPAN(1), SPAN(3));
	lv_label_set_text(compare_status, "compare: id: 0, score: 0");
    lv_obj_add_flag(compare_status, LV_OBJ_FLAG_HIDDEN);
#endif

	verify_status = lv_label_create(screen);
	lv_obj_set_style_text_color(verify_status, COLOR_YELLOW, LV_PART_MAIN);
	lv_obj_set_size(verify_status, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
	lv_obj_align(verify_status, LV_ALIGN_BOTTOM_LEFT, SPAN(1), SPAN(-6));
	lv_label_set_text(verify_status, "----");

	register_status = lv_label_create(screen);
	lv_obj_set_style_text_color(register_status, COLOR_YELLOW, LV_PART_MAIN);
	lv_obj_set_size(register_status, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
	lv_obj_align(register_status, LV_ALIGN_BOTTOM_LEFT, SPAN(1), SPAN(-4));
	lv_label_set_text(register_status, "registered: 0");

	preview_img.header.cf = LV_IMG_CF_TRUE_COLOR;
	preview_img.header.w = PREVIEW_WIDTH;
	preview_img.header.h = PREVIEW_HEIGHT;
	preview_img.data = (uint8_t *)preview_buf;
	preview_img.data_size = sizeof(preview_buf);

    screen_wq = workqueue_create("screen_wq", 9, 10, 8096);
    if (screen_wq == NULL) {
        LISA_LOGE(TAG, "Failed to create screen_wq");
    }

	return screen;
}

static inline uint16_t yuyv_to_rgb565(uint8_t y, uint8_t u, uint8_t v)
{
    int r, g, b;
    int c = y - 16;
    int d = u - 128;
    int e = v - 128;

    r = (298 * c + 409 * e + 128) >> 8;
    g = (298 * c - 100 * d - 208 * e + 128) >> 8;
    b = (298 * c + 516 * d + 128) >> 8;

    r = r < 0 ? 0 : (r > 255 ? 255 : r);
    g = g < 0 ? 0 : (g > 255 ? 255 : g);
    b = b < 0 ? 0 : (b > 255 ? 255 : b);

    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

/**
 * @brief 进行图片缩放，以及YUYV422转RGB565
 *
 * @param img_data[in] 人脸图像数据
 * @param img_width[in] 图像宽度
 * @param img_height[in] 图像高度
 * @param img_len[in] 图像长度
 * @param output_buf[out] 输出图像缓存
 *
 * @return void
 *
 */
static void process_image(uint8_t *img_data, int img_width, int img_height, int img_len, uint16_t *output_buf)
{
    bool is_yuyv = (CONFIG_IMAGE_FORMAT == 2);
    bool need_scale = false;

    if ((img_width == 160 && img_height == 120) || 
        (img_width == 640 && img_height == 480)) {
        need_scale = true;
    }

    if (is_yuyv) {
        if (need_scale) {
            if (img_width == 160 && img_height == 120) {
                for (int y = 0; y < 240; y++) {
                    for (int x = 0; x < 320; x++) {
                        int src_x = x / 2;
                        int src_y = y / 2;
                        int pixel_pair_idx = src_y * 160 + (src_x & ~1);
                        int byte_idx = pixel_pair_idx * 2;
                        
                        uint8_t y_val = img_data[byte_idx + (src_x & 1) * 2];
                        uint8_t u_val = img_data[byte_idx + 1];
                        uint8_t v_val = img_data[byte_idx + 3];
                        
                        output_buf[y * 320 + x] = yuyv_to_rgb565(y_val, u_val, v_val);
                    }
                }
            } else if (img_width == 640 && img_height == 480) {
                for (int y = 0; y < 240; y++) {
                    for (int x = 0; x < 320; x++) {
                        int src_x = x * 2;
                        int src_y = y * 2;
                        int pixel_pair_idx = src_y * 640 + (src_x & ~1);
                        int byte_idx = pixel_pair_idx * 2;
                        
                        uint8_t y_val = img_data[byte_idx + (src_x & 1) * 2];
                        uint8_t u_val = img_data[byte_idx + 1];
                        uint8_t v_val = img_data[byte_idx + 3];
                        
                        output_buf[y * 320 + x] = yuyv_to_rgb565(y_val, u_val, v_val);
                    }
                }
            }
        } else if (img_width == 320 && img_height == 240) {
            for (int i = 0; i < img_width * img_height / 2; i++) {
                int byte_idx = i * 4;
                uint8_t y0 = img_data[byte_idx];
                uint8_t u = img_data[byte_idx + 1];
                uint8_t y1 = img_data[byte_idx + 2];
                uint8_t v = img_data[byte_idx + 3];
                
                output_buf[i * 2] = yuyv_to_rgb565(y0, u, v);
                output_buf[i * 2 + 1] = yuyv_to_rgb565(y1, u, v);
            }
        } else {
            for (int i = 0; i < img_width * img_height / 2; i++) {
                int byte_idx = i * 4;
                uint8_t y0 = img_data[byte_idx];
                uint8_t u = img_data[byte_idx + 1];
                uint8_t y1 = img_data[byte_idx + 2];
                uint8_t v = img_data[byte_idx + 3];
                
                output_buf[i * 2] = yuyv_to_rgb565(y0, u, v);
                output_buf[i * 2 + 1] = yuyv_to_rgb565(y1, u, v);
            }
        }
    } else {
        if (need_scale) {
            if (img_width == 160 && img_height == 120) {
                uint16_t *src = (uint16_t *)img_data;
                for (int y = 0; y < 240; y++) {
                    for (int x = 0; x < 320; x++) {
                        int src_x = x / 2;
                        int src_y = y / 2;
                        output_buf[y * 320 + x] = src[src_y * 160 + src_x];
                    }
                }
            } else if (img_width == 640 && img_height == 480) {
                uint16_t *src = (uint16_t *)img_data;
                for (int y = 0; y < 240; y++) {
                    for (int x = 0; x < 320; x++) {
                        int src_x = x * 2;
                        int src_y = y * 2;
                        output_buf[y * 320 + x] = src[src_y * 640 + src_x];
                    }
                }
            }
        } else {
            memcpy(output_buf, img_data, img_len);
        }
    }
}

void screen_update_info(const acomp_fd_result_info_t *info, uint8_t *img_data, int img_width, int img_height, int img_len)
{
    if (img_data) {
        process_image(img_data, img_width, img_height, img_len, preview_buf);
        lv_img_set_src(preview, &preview_img);
    }

    if (info) {
        uint32_t result_cnt = info->results_cnt;

        if (result_cnt > 0) {
            uint32_t max_area_index = info->max_area_results_index;
            acomp_fd_result_t *result = (acomp_fd_result_t *)&info->results[max_area_index];

            /* 人脸个数，活体状态和分数 */
            char live_score[64] = {0};
            memset(live_score, 0, sizeof(live_score));
            snprintf(live_score, sizeof(live_score), "%.6f", result->live_result.scores[1]);
            lv_label_set_text_fmt(detect_status, "has_face: %d, Live: %d, LiveScore: %s", result_cnt, result->live_result.status, live_score);
    
            /* 最大匹配得分 */
        #if REAL_TIME_COMPARE_STATUS
            int max_score_index = 0;
            float max_score = 0.0f;
            if (result->compare_cnt > 0) {
                for (int i = 0; i < result->compare_cnt; i++) {
                    if (result->compare_scores[i] > max_score) {
                        max_score = result->compare_scores[i];
                        max_score_index = i;
                    }
                }

                char max_score_str[64] = {0}; 
                memset(max_score_str, 0, 64);
                snprintf(max_score_str, 64, "%.3f", max_score);

                lv_label_set_text_fmt(compare_status, "compare: id: %d, score: %s", max_score_index, max_score_str);
                lv_obj_clear_flag(compare_status, LV_OBJ_FLAG_HIDDEN);

                bool pass = (max_score > COMPARE_SCORE_THRESHOLD ? true : false);
                lv_obj_set_style_border_color(face_rect, pass ? COLOR_GREEN : COLOR_RED, LV_PART_MAIN);
            }
        #endif
            /* 人脸方框 */
            float scale_x = 1.0f;
            float scale_y = 1.0f;
            
            if (img_width == 160 && img_height == 120) {
                scale_x = 320.0f / 160.0f;
                scale_y = 240.0f / 120.0f;
            } else if (img_width == 640 && img_height == 480) {
                scale_x = 320.0f / 640.0f;
                scale_y = 240.0f / 480.0f;
            }
            
            int scaled_x = (int)(result->face_rect.x * scale_x);
            int scaled_y = (int)(result->face_rect.y * scale_y);
            int scaled_w = (int)(result->face_rect.w * scale_x);
            int scaled_h = (int)(result->face_rect.h * scale_y);
            
            lv_obj_set_pos(face_rect, scaled_x, scaled_y);
            lv_obj_set_size(face_rect, scaled_w, scaled_h);
            lv_obj_clear_flag(face_rect, LV_OBJ_FLAG_HIDDEN);

        } else {
            lv_label_set_text_fmt(detect_status, "has_face: %d", 0);
    
            lv_obj_add_flag(face_rect, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(verify_status, "----");
            lv_obj_set_style_text_color(verify_status, COLOR_YELLOW, LV_PART_MAIN);
            lv_obj_set_style_text_color(register_status, COLOR_YELLOW, LV_PART_MAIN);
            lv_obj_set_style_border_color(face_rect, COLOR_YELLOW, LV_PART_MAIN);
        #if REAL_TIME_COMPARE_STATUS
            lv_obj_add_flag(compare_status, LV_OBJ_FLAG_HIDDEN);
        #endif
        }
    }

#if defined(CONFIG_DISPLAY_DEBUG_SHOW_FPS)
	frame_cnt++;
#endif /* CONFIG_DISPLAY_DEBUG_SHOW_FPS */
}

static void screen_wq_handler(void *para)
{
    screen_msg_t *msg = (screen_msg_t *)para;
    LISA_LOGD(LOG_TAG, "screen_wq_handler");

    screen_update_info(msg->info, msg->img_data, msg->img_width, msg->img_height, msg->img_len);

    if(msg->info) {
        psram_free(msg->info);
    }
    psram_free(msg);
}

void screen_update_fd_info(acomp_fd_result_info_t *info, uint8_t *img_data, int img_width, int img_height, int img_len)
{
    screen_msg_t *msg = (screen_msg_t *)psram_malloc(sizeof(screen_msg_t));

    msg->info = info;
    msg->img_data = img_data;
    msg->img_width = img_width;
    msg->img_height = img_height;
    msg->img_len = img_len;

    int ret = workqueue_submit(screen_wq, screen_wq_handler, msg);
    if (ret == 0) {
        LISA_LOGE(TAG, "workqueue submit failed:%d", ret);
        if(msg->info) {
            psram_free(msg->info);
        }
        psram_free(msg);
    }
}

void screen_update_registered(uint32_t count)
{
	lv_label_set_text_fmt(register_status, "registered: %d", count);
}

static char score_str[256] = {0};
void screen_update_compare(uint32_t index, bool pass, float score)
{
    memset(score_str, 0, 256);
    snprintf(score_str, 256, "%.4f", score);
    LISA_LOGI(TAG, "screen_update_compare: index: %d, score: %f, score_str: %s, pass: %d", index, score, score_str, pass);
	lv_label_set_text_fmt(verify_status, "id: %d, score: %s", index, score_str);
	lv_obj_set_style_text_color(verify_status, pass ? COLOR_GREEN : COLOR_RED, LV_PART_MAIN);
	lv_obj_set_style_text_color(register_status, pass ? COLOR_GREEN : COLOR_RED, LV_PART_MAIN);
	lv_obj_set_style_border_color(face_rect, pass ? COLOR_GREEN : COLOR_RED, LV_PART_MAIN);
}
