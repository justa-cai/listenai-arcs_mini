#ifndef __MODEL_SIP_CALL_H__
#define __MODEL_SIP_CALL_H__

#include <stdbool.h>

/* 注册服务事件桥（lisa_ui_model_init 调用一次）。 */
int model_sip_call_init(void);

/* 从拨号页发起呼叫（对端来自 KV user.sip.peer）。 */
int model_sip_call_dial(void);
int model_sip_call_answer(void);
int model_sip_call_hangup(void);

const char *model_sip_call_peer(void);

#endif
