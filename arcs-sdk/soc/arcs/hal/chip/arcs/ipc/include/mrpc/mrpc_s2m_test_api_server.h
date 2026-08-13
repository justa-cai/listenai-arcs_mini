#ifndef __MRPC_S2M_TEST_API_SERVER_H__
#define __MRPC_S2M_TEST_API_SERVER_H__

#include "mrpc_types.h"

typedef enum
{
    MRPC_MSG_ID_S2M_TEST_START = MRPC_SERVICE_TYPE_S2M_TEST << 24,
    MRPC_MSG_ID_S2M_ECHO_REQ,
    MRPC_MSG_ID_S2M_TEST_MAX
} ipc_msg_id_s2m_test_t;

extern mrpc_msg_handler_t mrpc_msg_s2m_test_handlers[MRPC_MSG_ID_S2M_TEST_MAX - MRPC_MSG_ID_S2M_TEST_START];

#endif