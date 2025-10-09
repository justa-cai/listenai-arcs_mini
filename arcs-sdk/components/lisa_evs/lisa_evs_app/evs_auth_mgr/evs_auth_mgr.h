#ifndef __LISA_EVS_AUTH_MGR_H__
#define __LISA_EVS_AUTH_MGR_H__

#include "lisa_evs_config.h"

//开始evs鉴权操作
int start_evs_auth(const lisa_evs_config_t *const config);

//get access_token
const char *get_evs_access_token();

#endif //__LISA_EVS_AUTH_MGR_H__
