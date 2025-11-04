/**
 ****************************************************************************************
 *
 * @file fhost_test_ipc.c
 *
 *
 * Copyright (C) ListenAI 2024
 *
 ****************************************************************************************
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "ls_rtos.h"
#include "ipc.h"
#include "mrpc_client.h"
#include "mrpc_server.h"
#include "ipc_test.h"
#include "mrpc_m2s_test_api_server.h"
#include "mrpc_s2m_test_api_server.h"

#define CLI_IPC_TEST_STACK_SIZE     256
#define CLI_IPC_TEST_PRIORITY       RTOS_TASK_PRIORITY(7)

#ifdef IPC_TEST_CASE
#define CLI_LOG(fmt, ...)      CLOG(fmt, ##__VA_ARGS__)

extern struct ipc_ep *mrpc_client_ep;
static bool test_stop = false;
static volatile int32_t  test_fail;
static struct ipc_ep *mrpc_test_ep;

#ifdef CFG_AMP_IPC_MASTER
extern mrpc_msg_handler_t mrpc_msg_s2m_test_handlers[];
extern ls_err_t m2s_echo_req(struct ipc_ep * mrpc_ep, struct cfg_ipc_echo * echo_req, struct cfg_ipc_echo_resp * echo_resp);
ls_err_t m2s_start_slave_test(struct ipc_ep * mrpc_ep, struct ipc_test_param * conf);
ls_err_t m2s_stop_slave_test(struct ipc_ep * mrpc_ep);
#else
extern mrpc_msg_handler_t mrpc_msg_m2s_test_handlers[];
extern ls_err_t s2m_echo_req(struct ipc_ep * mrpc_ep, struct cfg_ipc_echo * echo_req, struct cfg_ipc_echo_resp * echo_resp);
#endif

static RTOS_TASK_FCT(mrpc_test_task)
{
    uint32_t i = 0, id = 0, task_id;
#ifdef IPC_MSG_SEGMENT
    int32_t j;
#endif
    uint32_t sec_tx, usec_tx;
    uint32_t sec_rx, usec_rx;
    struct ipc_test_conf *conf = env;
    struct cfg_ipc_echo *req;
    struct cfg_ipc_echo_resp *reply;

    req = rtos_malloc(sizeof(struct cfg_ipc_echo));
    if (!req)
    {
        CLI_LOG("Failed to start ipc test.");
        goto ERR;
    }
    reply = rtos_malloc(sizeof(struct cfg_ipc_echo_resp));
    if (!reply)
    {
        CLI_LOG("Failed to start ipc test.");
        goto ERR;
    }

    id = conf->task_id * 100000;
    if (conf->type)
        id |= 1<<31;
    else
        id &= ~(1<<31);
    CLI_LOG("Test case: name=mrpc_test task=0x%x type=%d cnt=%d start_id=0x%x\n", conf->task_id, conf->type, conf->cnt, id);

    while (!test_stop && (i++ < conf->cnt))
    {
        rtos_get_sys_time(0, &sec_tx, &usec_tx);
        req->echo.pkt_id  = id++;
        req->echo.sec_tx  = sec_tx;
        req->echo.usec_tx = usec_tx;
        memset(reply, 0, sizeof(struct cfg_ipc_echo_resp));
#ifdef IPC_MSG_SEGMENT
        for (j = 0; j < IPC_TEST_DATA_LEN; j++)
        {
            req->data[j] = id + j;
        }
#endif
#ifdef CFG_AMP_IPC_MASTER
        if (!m2s_echo_req(conf->ep, req, reply))
#else
        if (!s2m_echo_req(conf->ep, req, reply))
#endif
        {
            if (reply->echo.pkt_id != id)
            {
                CLI_LOG("!!!!Err: task_id=%u type=%d, req=0x%x, reply=0x%x\n", conf->task_id, conf->type,
                        (id - 1), reply->echo.pkt_id);
                test_fail++;
                break;
            }
            else
            {
#ifdef IPC_MSG_SEGMENT
                uint32_t *ptr = (uint32_t*)reply->data;

                for (j = 0; j < IPC_TEST_DATA_LEN/4; j++)
                {
                    if (ptr[j] != id + j)
                    {
                        CLI_LOG("!!!!Err: task_id=%u type=%d, req=0x%x, data[%d] 0x%x != 0x%x\n", conf->task_id, conf->type,
                            (id - 1), j, reply->data[j], id+j);
                        test_fail++;
                        break;
                    }
                }
#endif
                rtos_get_sys_time(0, &sec_rx, &usec_rx);
                if (usec_rx < usec_tx)
                {
                    sec_rx -= 1;
                    usec_rx += 1000000;
                }
                CLI_LOG("task_id=%u type=%d, req=0x%x time=%u.%06u\n", conf->task_id, conf->type, (id - 1), (sec_rx - sec_tx), (usec_rx - usec_tx));
            }
        }
        else
        {
            CLI_LOG("Err of task %d: send 0x%x\n", conf->task_id, (id - 1));
            test_fail++;
            break;
        }
    }

    CLI_LOG("Done with task %d type %d fail %d\n", conf->task_id, conf->type, test_fail);
ERR:
    rtos_free(conf);
    rtos_task_delete(NULL);
}

void ipc_test_stop(void)
{
    test_stop = true;
}

int32_t ipc_test_start(struct ipc_test_param *param)
{
    int32_t i;
    struct ipc_test_conf *test_conf;

    CLI_LOG("Start ipc test: task_num=%d, cnt=%d", param->task_num, param->cnt);
    test_stop = false;
    test_fail = 0;

    for (i = 0; i < param->task_num; i++)
    {
        test_conf = rtos_malloc(sizeof(struct ipc_test_conf));
        if (!test_conf)
        {
            CLI_LOG("Failed to start ipc test.");
            return -1;
        }
        test_conf->task_id = (i << 1) + 0;
        test_conf->cnt     = param->cnt;
        test_conf->type    = 0;
        test_conf->ep      = mrpc_client_ep;
        if (rtos_task_create(mrpc_test_task, "ipc_test", APPLICATION_TASK,
                         CLI_IPC_TEST_STACK_SIZE, test_conf, CLI_IPC_TEST_PRIORITY, NULL))
        {
            rtos_free(test_conf);
            return -1;
        }

        test_conf = rtos_malloc(sizeof(struct ipc_test_conf));
        if (!test_conf)
        {
            CLI_LOG("Failed to start ipc test.");
            return -1;
        }
        test_conf->task_id = (i << 1) + 1;
        test_conf->cnt     = param->cnt;
        test_conf->type    = 1;
        test_conf->ep      = mrpc_test_ep;
        if (rtos_task_create(mrpc_test_task, "ipc_test", APPLICATION_TASK,
                         CLI_IPC_TEST_STACK_SIZE, test_conf, CLI_IPC_TEST_PRIORITY, NULL))
        {
            rtos_free(test_conf);
            return -1;
        }
    }

    return 0;
}

#ifdef CFG_AMP_IPC_MASTER
int32_t s2m_echo_req(void *dummy, struct cfg_ipc_echo *req, struct cfg_ipc_echo_resp *resp)
{
    resp->echo.pkt_id = req->echo.pkt_id + 1;

    return 0;
}

void ipc_test_start_slave(struct ipc_test_param *conf)
{
    m2s_start_slave_test(mrpc_client_ep, conf);
}

void ipc_test_stop_slave(void)
{
    m2s_stop_slave_test(mrpc_client_ep);
}
#else
int32_t m2s_echo_req(void *dummy, struct cfg_ipc_echo *req, struct cfg_ipc_echo_resp *resp)
{
    int32_t i;
#ifdef IPC_MSG_SEGMENT
    uint32_t *ptr;
#endif

    resp->echo.pkt_id = req->echo.pkt_id + 1;

#if 0
    if ( ( (srv_id == IPC_EP_MRPC_SRV) && (req->echo.pkt_id & (1<<31)) )
          || ((srv_id == IPC_EP_MRPC_SRV_TEST) && !(req->echo.pkt_id & (1<<31)) )
        )
    {
        CLOGD("ERR: SER %d ID 0x%x\n", srv_id, resp->echo.pkt_id);
        GLOBAL_INT_DISABLE();
        while(1);
    }
#endif
#ifdef IPC_MSG_SEGMENT
    ptr = (uint32_t*)resp->data;
    for (i = 0; i < IPC_TEST_DATA_LEN/4; i++)
    {
        ptr[i] = resp->echo.pkt_id + i;
    }
#endif

    return 0;
}

int32_t m2s_start_slave_test(void *dummy, struct ipc_test_param *conf)
{
    ipc_test_start(conf);

    return 0;
}

int32_t m2s_stop_slave_test(void *dummy)
{
	ipc_test_stop();

    return 0;
}
#endif


void ipc_test_case_init(struct mrpc_server_env *mrpc_server)
{
    struct mrpc_server_env *test_server;

#ifdef CFG_AMP_IPC_MASTER
    test_server = mrpc_server_init(IPC_CHAN_MASTER_MSG, IPC_EP_MRPC_SRV_TEST);
    mrpc_service_register(test_server, MRPC_SERVICE_TYPE_S2M_TEST, mrpc_msg_s2m_test_handlers, MRPC_MSG_ID_S2M_TEST_MAX);
    mrpc_test_ep = mrpc_client_create(IPC_CHAN_MASTER_MSG, IPC_CHAN_SLAVE_MSG, IPC_EP_MRPC_CLT_TEST, IPC_EP_MRPC_SRV_TEST);

    mrpc_service_register(mrpc_server, MRPC_SERVICE_TYPE_S2M_TEST, mrpc_msg_s2m_test_handlers, MRPC_MSG_ID_S2M_TEST_MAX);
#else
    test_server = mrpc_server_init(IPC_CHAN_SLAVE_MSG, IPC_EP_MRPC_SRV_TEST);
    mrpc_service_register(test_server, MRPC_SERVICE_TYPE_M2S_TEST, mrpc_msg_m2s_test_handlers, MRPC_MSG_ID_M2S_TEST_MAX);
    mrpc_test_ep = mrpc_client_create(IPC_CHAN_SLAVE_MSG, IPC_CHAN_MASTER_MSG, IPC_EP_MRPC_CLT_TEST, IPC_EP_MRPC_SRV_TEST);

    mrpc_service_register(mrpc_server, MRPC_SERVICE_TYPE_M2S_TEST, mrpc_msg_m2s_test_handlers, MRPC_MSG_ID_M2S_TEST_MAX);
#endif
}
#endif
