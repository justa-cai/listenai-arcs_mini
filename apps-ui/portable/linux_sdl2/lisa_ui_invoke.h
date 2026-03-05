#ifndef __LISA_UI_INVOKE_H__
#define __LISA_UI_INVOKE_H__

#define LISA_UI_INVOKE_UI(__func, __arg, __arg_len) __func(__arg, __arg_len)
#define LISA_UI_INVOKE_BN(__func, __arg, __arg_len) __func(__arg, __arg_len)

int lisa_ui_invoke_init(void);

#endif
