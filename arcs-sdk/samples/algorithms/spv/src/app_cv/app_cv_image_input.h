#pragma once

#include <stdint.h>

int app_cv_image_stream_init(void);
int app_cv_image_send_test_image(void);
void app_cv_image_on_frame_done(uint32_t fb_addr);

uint32_t app_cv_mock_image_count(void);
uint32_t app_cv_mock_image_width(void);
uint32_t app_cv_mock_image_height(void);
uint32_t app_cv_mock_image_size(void);
uint32_t app_cv_mock_image_byte_addr(uint32_t index);
uint32_t app_cv_mock_frame_status(uint32_t index);
