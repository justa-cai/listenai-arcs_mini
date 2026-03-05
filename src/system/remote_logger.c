#include <stdint.h>
#include <string.h>

#define TAG "remote_logger"
#include "lisa_log.h"

#define AP_LOG_BG_COLOR "\033[48;5;239m"  // 深灰色背景（保持原设计）
#define AP_LOG_COLOR_END "\033[0m"  /* 重置颜色 */

int remote_log_output_printf(const uint8_t *log, uint32_t len)
{
    // 查找log中的ANSI颜色代码
    const char *color_start = NULL;
    const char *color_end = NULL;

    // 在log中查找 ESC[ 序列开始
    for (uint32_t i = 0; i < len - 1; i++) {
        if (log[i] == '\033' && log[i + 1] == '[') {
            color_start = (const char *)&log[i];
            // 查找颜色代码结束位置（'m'字符）
            for (uint32_t j = i + 2; j < len; j++) {
                if (log[j] == 'm') {
                    color_end = (const char *)&log[j + 1];
                    break;
                }
            }
            break;
        }
    }

    // 如果找到了颜色代码，组合背景色和log的前景色
    if (color_start && color_end) {
        int color_len = color_end - color_start;
        LISA_LOG_RAW(AP_LOG_BG_COLOR "%.*s[ap]%.*s" AP_LOG_COLOR_END,
                     color_len, color_start,  // 应用log的颜色（前景色）
                     len, log);
    } else {
        // 没有找到颜色代码，使用背景色
        LISA_LOG_RAW(AP_LOG_BG_COLOR "[ap]"  "%.*s" AP_LOG_COLOR_END, len, log);
    }

    return 0;
}