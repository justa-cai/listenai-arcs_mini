#pragma once

#include "lisa_touch.h"
#include <stdint.h>
#include <stdbool.h>

#define BL6XXX_POINT_REG    0x01

#ifdef __cplusplus
extern "C" {
#endif

// 声明 bl6133 的触摸设备实例
extern const struct touch_device touch_bl6133;

#ifdef __cplusplus
}
#endif
