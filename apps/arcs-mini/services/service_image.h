#ifndef __service_image_H__
#define __service_image_H__

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

int service_image_init(void);

/* 文生图进入等待态（例如收到带qr标识的引导图URL） */
void service_image_waiting_start(void);

/* 取消等待态（例如用户开始了新的交互） */
void service_image_waiting_cancel(void);

/* 等待态允许显示；取消态丢弃一次；初始化态默认允许显示一次 */
bool service_image_waiting_get(void);

#ifdef __cplusplus
}
#endif

#endif
