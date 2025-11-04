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
#include "ipc_platform.h"
#include "ipc_queue.h"


#define IPC_STATS
//#define IPC_DEBUG
//#define IPC_SLAVE_DATA_CHAN_IN_USER_MODE
//#define IPC_TEST_CASE
#define IPC_MSG_SEGMENT
#define IPC_MSG_SEGMENT_MAX                       3

#if !defined(TASK_CREATE_STATIC) || (TASK_CREATE_STATIC == 0)
#ifndef IPC_MSG_SEGMENT
#error "`IPC_MSG_SEGMENT` must be enabled if `TASK_CREATE_STATIC` is not enabled."
#endif
#endif

#if defined (IPC_MSG_SEGMENT) && (IPC_MSG_SEGMENT_MAX < 2)
#error "IPC_MSG_SEGMENT_MAX should >= 2"
#endif

#ifdef IPC_DEBUG

#define ipc_info(fmt, ...)   logDbg(fmt, ##__VA_ARGS__)
#define ipc_dbg(fmt, ...)    logDbg(fmt, ##__VA_ARGS__)
#define ipc_err(fmt, ...)    logDbg(fmt, ##__VA_ARGS__)
#else

#define ipc_info(fmt, ...)
#define ipc_dbg(fmt, ...)
#define ipc_err(fmt, ...)    logDbg(fmt, ##__VA_ARGS__)
#endif

#define IPC_ERR_OK                               (0)
#define IPC_ERR_INVALID                          (-1)
#define IPC_ERR_NO_MEM                           (-2)
#define IPC_ERR_BUFF_SIZE                        (-3)
#define IPC_ERR_PARAM                            (-4)
#define IPC_ERR_TIMEOUT                          (-5)
#define IPC_ERR_NO_BUFF                          (-6)
#define IPC_ERR_NOT_READY                        (-7)


#define IPC_CHAN_FLAGS_NO_LOCK                   0x00000001
#define IPC_CHAN_FLAGS_USER_MODE                 0x00000002
#define IPC_CHAN_FLAGS_FAST                      0x00000004
#define IPC_CHAN_FLAGS_REMOTE                    0x80000000

#define CHAN_LOCAL                               0
#define CHAN_REMOTE                              1


#define IPC_IRQ_MAX_NUM                          13

#define IPC_POLL_SHORT_INTERVAL_MS               1
#define IPC_POLL_LONG_INTERVAL_MS                2
#define IPC_CCB_NAME_SIZE                        10
#define IPC_IRQ_ID_OFFSET                        0
#define IPC_IRQ_ID_MASK                          0x3F
#define IPC_LINK_ID_OFFSET                       6
#define IPC_LINK_ID_MASK                         0x3
#define IPC_INVALID_MSG_ID                       0xFFFF

#define IPC_TIMEOUT                              200

#define IPC_DBG_STATUS_ACTIVE                    1
#define IPC_DBG_STATUS_SLEEP                     0

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

enum
{
    CORE_ID_MASTER,
    CORE_ID_SLAVE,
};

#define IPC_GET_LINK_ID_FROM_CHAN(chan)          ((chan >> IPC_LINK_ID_OFFSET) & IPC_LINK_ID_MASK)
#define IPC_GET_IRQ_ID_FROM_CHAN(chan)           ((chan >> IPC_IRQ_ID_OFFSET) & IPC_IRQ_ID_MASK)

#define IPC_EVT_LINKUP                           0x00000001
#define IPC_EVT_HALT                             0x00000002
#define IPC_EVT_PRINT                            0x00000004

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

#define IPC_CHAN_ANY                            (-1)
#define IPC_EP_ANY                              (-1)
/*The IPC channel ID consists of two parts: the link ID and the IPC IRQ number.*/
enum
{
    IPC_CHAN_MASTER_TXCFM = (CORE_ID_MASTER << IPC_LINK_ID_OFFSET) | (0 <<IPC_IRQ_ID_OFFSET),
    IPC_CHAN_MASTER_RXDESC,
    IPC_CHAN_MASTER_FAST,
    IPC_CHAN_MASTER_MSG,
    IPC_CHAN_MASTER_MAX,
};

enum
{
    IPC_CHAN_SLAVE_TXDESC = (CORE_ID_SLAVE << IPC_LINK_ID_OFFSET) | (0 <<IPC_IRQ_ID_OFFSET),
    IPC_CHAN_SLAVE_RXCFM,
    IPC_CHAN_SLAVE_FAST,
    IPC_CHAN_SLAVE_MSG,
    IPC_CHAN_SLAVE_MAX,
};

/*IPC endpoint index*/
enum
{
    IPC_EP_IND,
    IPC_EP_MRPC_WL_SRV,
    IPC_EP_MRPC_WL_CLT,
    IPC_EP_MRPC_SRV_TEST,
    IPC_EP_MRPC_CLT_TEST,
    IPC_EP_MRPC_CMN_SRV,
    IPC_EP_MRPC_CMN_CLT,
    IPC_EP_MAX,
};

enum
{
    IPC_MSG_RELEASE,
    IPC_MSG_HOLD,
};

#define EPMSG_FROM_BUF(buf)     ((struct ipc_epmsg*)((char *)(buf)-offsetof(struct ipc_epmsg, data)))
#define IPC_GET_EPMSG_LEN(buf)  (EPMSG_FROM_BUF(buf)->hdr.len)

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
    uint32_t send_notify;
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


union ipc_eid
{
    uint16_t val;
    struct
    {
        uint8_t chan; /*ipc channel id*/
        uint8_t ep;  /*endpoint id*/
    } id;
};

struct ipc_epmsg_hdr
{
    union ipc_eid dst_id;
    union ipc_eid src_id;
#ifdef IPC_MSG_SEGMENT
    void* next;
#endif
    uint16_t desc_idx;
    uint16_t len;  /*data length*/
};

struct ipc_epmsg
{
    struct ipc_epmsg_hdr hdr;
    uint8_t data[];
};

struct ipc_msg_hdr
{
    uint16_t id;
    uint16_t len;
    uint32_t data[];
};

struct ipc_msg_desc_hdr
{
    union ipc_eid dst_id;
    union ipc_eid src_id;

    uint16_t flags;
    uint16_t data_len;
#if defined(IPC_MSG_SEGMENT)
    uint32_t total_len;
#endif
};

struct ipc_msg_desc
{
    struct ipc_msg_desc_hdr hdr;
    void *data;
#if defined(IPC_MSG_SEGMENT)
    void *seg[IPC_MSG_SEGMENT_MAX - 1];/* The position of 'seg' must immediately follow 'data' */
#endif
};

struct ipc_status
{
    uint32_t fast_notify_status;
};

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
    volatile struct ipc_status *local_status;
    volatile struct ipc_status *remote_status;
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
void ipc_init(uint32_t link_id, volatile struct ipc_status *local, volatile struct ipc_status *remote);
struct ipc_queue* ipc_shared_queue_init(bool master, volatile struct vring_hdr *vring, volatile void *buf, int32_t unit_size, int32_t unit_num);
struct ipc_ccb* ipc_chan_create(char *name, int32_t chan, struct ipc_queue *rxq, ipc_chan_callback_t cb, void *cb_param, uint32_t flags);
uint8_t* ipc_get_tbuffer(struct ipc_ccb *ccb, uint16_t *size, uint32_t timeout);
int32_t ipc_send_tbuffer(struct ipc_ccb *ccb, struct ipc_msg_desc *desc);
int32_t ipc_sendto(struct ipc_ccb *ccb, struct ipc_msg_desc *desc, uint32_t timeout);
int32_t ipc_send(struct ipc_ccb *ccb, void *data, int32_t size, uint32_t timeout);
int32_t ipc_free_rbuffer(struct ipc_ccb *ccb, uint8_t *buffer, int32_t size);
uint8_t* ipc_get_rbuffer(struct ipc_ccb *ccb, struct ipc_msg_desc *desc, uint32_t timeout);
int32_t ipc_recvfrom(struct ipc_ccb *ccb, struct ipc_msg_desc *desc, uint32_t timeout);
int32_t ipc_buf_full(struct ipc_ccb *ccb);
void ipc_fast_notify(uint32_t chan, int32_t event);
uint32_t ipc_get_fast_notify_status(void);
union ipc_eid ipc_get_eid(uint32_t chan, uint32_t idx);
uint16_t ipc_get_seq(void);
struct ipc_ccb *ipc_get_ccb(uint32_t chan, int32_t type);
void ipc_status_set(struct ipc_ccb *ccb, uint32_t status);
#endif
