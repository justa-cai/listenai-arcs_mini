#ifndef __LISA_AIUI_RID_MAN_H__
#define __LISA_AIUI_RID_MAN_H__

#include <stdint.h>

int lisa_aiui_rid_man_init(void);
int lisa_aiui_rid_list_add(uint32_t rid, void *data);
int lisa_aiui_rid_list_remove(uint32_t rid);
int lisa_aiui_rid_list_get(uint32_t rid, void **data);
int lisa_ui_rid_list_clear(void);

#endif /* __LISA_AIUI_RID_MAN_H__ */
