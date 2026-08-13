/**
 ****************************************************************************************
 *
 * Copyright (C) ListenAI 2024
 *
 *
 ****************************************************************************************
*/
#ifndef _IPC_CORE_H_
#define _IPC_CORE_H_

#include <stdbool.h>
#include "log_print.h"
#include "co_dl_list.h"
#include "rtos_al.h"
#include "ipc_types.h"
#include "ipc_platform.h"
#include "ipc_queue.h"


#define IPC_STATS
//#define IPC_DEBUG
//#define IPC_SLAVE_DATA_CHAN_IN_USER_MODE

#if !defined(TASK_CREATE_STATIC) || (TASK_CREATE_STATIC == 0)
#ifndef IPC_MSG_SEGMENT
#error "`IPC_MSG_SEGMENT` must be enabled if `TASK_CREATE_STATIC` is not enabled."
#endif
#endif

#ifdef IPC_DEBUG

#define ipc_info(fmt, ...)   //logDbg(fmt, ##__VA_ARGS__)
#define ipc_dbg(fmt, ...)    logDbg(fmt, ##__VA_ARGS__)
#define ipc_err(fmt, ...)    logDbg(fmt, ##__VA_ARGS__)

#else

#define ipc_info(fmt, ...)
#define ipc_dbg(fmt, ...)
#define ipc_err(fmt, ...)    logDbg(fmt, ##__VA_ARGS__)

#endif

#define ipc_print(fmt, ...)   logDbg(fmt, ##__VA_ARGS__)

#define IPC_IRQ_MAX_NUM                          13

#define IPC_POLL_SHORT_INTERVAL_MS               1
#define IPC_POLL_LONG_INTERVAL_MS                2
#define IPC_CCB_NAME_SIZE                        10
#define IPC_INVALID_MSG_ID                       0xFFFF

#ifdef IPC_STATS
#define IPC_STAT(ccb, trx, filed) {\
                                GLOBAL_INT_DISABLE();\
                                ccb->stats.trx.filed++; \
                                GLOBAL_INT_RESTORE();\
                             }
#else
#define IPC_STAT(ccb, filed)
#endif

#define IPC_ASSERT(expr) \
    if ((expr) != true) { \
        CLOGD("IPC_ASSERT: %s: %d" "\n", __FUNCTION__, __LINE__); \
        while(1); \
    }

#ifdef IPC_STATS
#define IPC_NAME(name)                           name
#else
#define IPC_NAME(name)                           NULL
#endif

#ifdef CFG_AMP_IPC_MASTER
#define  CORE_CUR                               CORE_ID_MASTER
#define  CORE_PEER                              CORE_ID_SLAVE
#else
#define  CORE_CUR                               CORE_ID_SLAVE
#define  CORE_PEER                              CORE_ID_MASTER
#endif

#ifdef IPC_STATS
struct rxq_stats
{
    uint32_t rx_ok;
    uint32_t rx_retry;
    uint32_t rx_int;
    uint16_t rxq_avail_rd_idx;
    uint16_t rxq_avail_wr_idx;
    uint16_t rxq_ready_rd_idx;
    uint16_t rxq_ready_wr_idx;
};

struct txq_stats
{
    uint32_t tx_ok;
    uint32_t tx_retry;
    uint32_t tx_failed;
    uint32_t send_signal;
    uint16_t txq_avail_rd_idx;
    uint16_t txq_avail_wr_idx;
    uint16_t txq_ready_rd_idx;
    uint16_t txq_ready_wr_idx;
};

struct ipc_stats
{
    uint16_t q_num;
    uint16_t q_size;
    union
    {
        struct rxq_stats rx;
        struct txq_stats tx;
    };
};
#endif


typedef int32_t (*ipc_chan_callback_t)(void *ccb, void *param);

struct ipc_chan_priv
{
    ipc_chan_callback_t callback;
    void *cb_param;
    struct dl_list epts;
};

/*ipc channel control block*/
struct ipc_ccb
{
    struct dl_list list;
    struct ipc_queue *vq;
    ipc_lock_t lock_for_ready; /*Lock for updating ready index*/
    //ipc_lock_t lock_for_avail; /*Lock for updating avail index*/
    uint32_t flags;
    uint32_t chan;
    struct ipc_chan_priv *priv;
#ifdef IPC_STATS
    char name[IPC_CCB_NAME_SIZE + 1];
    struct ipc_stats stats;
#endif
};

struct ipc_instance
{
    uint32_t link_id;
    volatile struct ipc_signal *local_signal;
    volatile struct ipc_signal *remote_signal;
    struct dl_list local_ccb;
    struct dl_list remote_ccb;
#ifdef IPC_STATS
    struct dl_list all_ccb;
    uint32_t rx_irq_cnt;
#endif
};

#ifdef IPC_STATS
struct ipc_instance* ipc_get_ep_dump(void);
#endif
void ipc_init(uint32_t link_id, volatile struct ipc_signal *local, volatile struct ipc_signal *remote);
struct ipc_queue* ipc_get_queue(volatile struct vring_hdr *vring);
struct ipc_ccb* ipc_chan_create(char *name, int32_t chan, struct ipc_queue *rxq, ipc_chan_callback_t cb, void *cb_param, uint32_t flags);
uint8_t* ipc_get_tbuffer(struct ipc_ccb *ccb, uint16_t *size, uint32_t timeout);
int32_t ipc_send_tbuffer(struct ipc_ccb *ccb, struct ipc_msg_desc *desc);
int32_t ipc_sendto(struct ipc_ccb *ccb, struct ipc_msg_desc *desc, uint32_t timeout);
int32_t ipc_send(struct ipc_ccb *ccb, void *data, int32_t size, uint32_t timeout);
int32_t ipc_free_rbuffer(struct ipc_ccb *ccb, uint8_t *buffer, int32_t size);
uint8_t* ipc_get_rbuffer(struct ipc_ccb *ccb, struct ipc_msg_desc *desc, uint32_t timeout);
int32_t ipc_recvfrom(struct ipc_ccb *ccb, struct ipc_msg_desc *desc, uint32_t timeout);
int32_t ipc_buf_full(struct ipc_ccb *ccb);
uint32_t ipc_get_signal_state(void);
union ipc_eid ipc_get_eid(uint32_t chan, uint32_t idx);
uint16_t ipc_get_seq(void);
struct ipc_ccb *ipc_get_ccb(uint32_t chan, int32_t type);
void ipc_status_set(struct ipc_ccb *ccb, uint32_t status);
#endif
