#ifndef __MRPC_OTP_API_SERVER_H__
#define __MRPC_OTP_API_SERVER_H__

#include "mrpc_server.h"

typedef enum
{
    MRPC_MSG_ID_OTP_START = MRPC_SERVICE_TYPE_OTP << 24,
    MRPC_MSG_ID_NV_SELFCALI_ERASE_OTP,
    MRPC_MSG_ID_OTP_MAX
} ipc_msg_id_otp_t;

extern mrpc_msg_handler_t mrpc_msg_otp_handlers[MRPC_MSG_ID_OTP_MAX - MRPC_MSG_ID_OTP_START];

#endif