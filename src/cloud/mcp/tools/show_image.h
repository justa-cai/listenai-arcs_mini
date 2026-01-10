#ifndef __SHOW_IMAGE_H__
#define __SHOW_IMAGE_H__

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 文生图等待状态管理函数
void show_image_set_waiting_state(bool waiting);
bool show_image_is_waiting(void);
void show_image_cancel_waiting(void);
bool show_image_is_cancelled(void);

// 直接加载并显示图片（无状态检查）
int show_image_load_and_display(const char *url);

// This header is intentionally minimal as the show_image.c file
// uses static registration and doesn't expose any public APIs.
// All MCP tool registration happens via the MCP_REGISTER_TOOL_STATIC macro.

#ifdef __cplusplus
}
#endif

#endif /* __SHOW_IMAGE_H__ */
