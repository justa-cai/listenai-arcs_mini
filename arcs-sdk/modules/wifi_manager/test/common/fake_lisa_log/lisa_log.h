#pragma once

// 这是一个模拟的lisa_log.h文件，用于测试mac_manager.c

#include <stdio.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LISA_LOGD(tag, fmt, ...) printf("[D][%s] " fmt "\n", tag, ##__VA_ARGS__)

#define LISA_LOGI(tag, fmt, ...) printf("[I][%s] " fmt "\n", tag, ##__VA_ARGS__)

#define LISA_LOGE(tag, fmt, ...) printf("[E][%s] " fmt "\n", tag, ##__VA_ARGS__)

#define LISA_LOGW(tag, fmt, ...) printf("[W][%s] " fmt "\n", tag, ##__VA_ARGS__)

#define LOGE(fmt, ...) printf("[E] " fmt "\n", ##__VA_ARGS__)
#define LOGI(fmt, ...) printf("[I] " fmt "\n", ##__VA_ARGS__)
#define LOGW(fmt, ...) printf("[W] " fmt "\n", ##__VA_ARGS__)
#define LOGD(fmt, ...) printf("[D] " fmt "\n", ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif
