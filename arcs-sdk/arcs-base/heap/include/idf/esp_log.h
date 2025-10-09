#include <stdarg.h>
#if CONFIG_MODULE_SHELL
#include "shell.h"
#endif

#ifndef CRLF
#define CRLF "\r\n"
#endif

#define ESP_EARLY_LOGI(TAG, FMT, ...) printk("[heap] " FMT CRLF, ##__VA_ARGS__)
#define ESP_EARLY_LOGD(TAG, FMT, ...) printk("[heap] " FMT CRLF, ##__VA_ARGS__)
