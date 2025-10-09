#ifndef __MRPC_OTP_API_MSG_H__
#define __MRPC_OTP_API_MSG_H__
#include <stdio.h>
#include <stdint.h>
#include "ls_err.h"
#include "ls_wifi_type.h"
#include "mrpc.h"

typedef struct
{
    struct mrpc_req_msg hdr;
} mrpc_nv_selfcali_erase_otp_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_nv_selfcali_erase_otp_resp_t;

#endif //__MRPC_OTP_API_MSG_H__