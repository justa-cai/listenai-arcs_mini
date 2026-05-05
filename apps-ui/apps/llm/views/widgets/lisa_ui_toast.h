#ifndef __LISA_UI_TOAST_H__
#define __LISA_UI_TOAST_H__

#include <stdint.h>

void lisa_ui_toast_show(const char *txt);
void lisa_ui_toast_show_duration(const char *txt, uint32_t duration_ms);

#endif /* __LISA_UI_TOAST_H__ */
