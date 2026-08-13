#ifndef __MRPC_M2S_TEST_API_SERVER_H__
#define __MRPC_M2S_TEST_API_SERVER_H__

#include "mrpc_types.h"

typedef enum
{
    MRPC_MSG_ID_M2S_TEST_START = MRPC_SERVICE_TYPE_M2S_TEST << 24,
    MRPC_MSG_ID_M2S_ECHO_REQ,
    MRPC_MSG_ID_M2S_START_SLAVE_TEST,
    MRPC_MSG_ID_M2S_STOP_SLAVE_TEST,
    MRPC_MSG_ID_M2S_TEST_MAX
} ipc_msg_id_m2s_test_t;

extern mrpc_msg_handler_t mrpc_msg_m2s_test_handlers[MRPC_MSG_ID_M2S_TEST_MAX - MRPC_MSG_ID_M2S_TEST_START];

#endif