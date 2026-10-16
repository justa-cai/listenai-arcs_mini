/* llm_client —— 端侧 LLM IPC 客户端服务层实现
 *
 * mailbox 协议 + cache 维护 + 增量游标读法见 src/shell/cmd/llm_mailbox.h
 * 与 apps/remote-ap/src/llm_service.c（AP 侧）。
 */
#include <string.h>
#include <stdio.h>

#include "FreeRTOS.h"

#include "ic_message.h"
#include "cache.h"
#include "lisa_log.h"

#include "llm_client.h"
#include "llm_mailbox.h"   /* src/shell/cmd 下共享协议头 */

#define TAG "llm.clnt"

#define LLM_SERVICE_ID   IC_MESSAGE_ID_LLM
#define LLM_CLIENT_SUBS_MAX 4

/* mailbox 是跨核共享内存，CP 侧 DCACHE 开启时：
 * 写完必须 Flush 回物理 SRAM（AP 才能读到），读前必须 Invalidate
 * （丢弃本核过期行）。__sync_synchronize 只是 fence，不维护 cache。 */
static void mb_flush(void)
{
    HAL_FlushDCache_by_Addr((uint32_t *)LLM_MAILBOX_ADDR, sizeof(llm_mailbox_t));
}
static void mb_invalidate(void)
{
    HAL_InvalidateDCache_by_Addr((uint32_t *)LLM_MAILBOX_ADDR, sizeof(llm_mailbox_t));
}

static struct {
    uint8_t inited;
    uint8_t busy;
    uint32_t read_offset;              /* 流式读取游标（已分发的字节数） */
    llm_client_cb_t cb[LLM_CLIENT_SUBS_MAX];
    void *user[LLM_CLIENT_SUBS_MAX];
} S;

static void dispatch(int evt, const char *delta, uint32_t len,
                     uint32_t tokens, uint32_t ms)
{
    for (int i = 0; i < LLM_CLIENT_SUBS_MAX; i++) {
        if (S.cb[i]) S.cb[i](S.user[i], evt, delta, len, tokens, ms);
    }
}

/* AP 侧回包（rpc_client 线程）：PIECE=增量，DONE=残余+统计。
 * 注意：ic_message 接收侧 msg_type 硬编码为 CMD（线上不传类型），
 * 只能按 state 分发。 */
static int32_t ipc_callback(ic_message_handle_info_t *h, ic_message_msg_info_t *m)
{
    (void)h;
    if (!m || !m->msg || m->len < 4) return -1;

    llm_notify_t *n = m->msg;

    if (n->state == LLM_MB_PIECE) {
        mb_invalidate();
        uint32_t avail = LLM_MB->length;
        if (avail > S.read_offset && avail <= LLM_MB_TEXT_MAX) {
            uint32_t delta = avail - S.read_offset;
            dispatch(LLM_CLIENT_EVT_PIECE,
                     (const char *)LLM_MB->text + S.read_offset, delta, 0, 0);
            S.read_offset = avail;
        }
    } else if (n->state == LLM_MB_DONE) {
        mb_invalidate();
        uint32_t avail = LLM_MB->length;
        if (avail > LLM_MB_TEXT_MAX) avail = 0;

        /* 剥离 AP 侧追加的 " |Q=[...]" 回显 */
        if (avail > 0) {
            char *qecho = strstr((const char *)LLM_MB->text, " |Q=[");
            if (qecho && (uint32_t)(qecho - (char *)LLM_MB->text) < avail) {
                avail = qecho - (char *)LLM_MB->text;
            }
        }

        if (avail > S.read_offset) {
            dispatch(LLM_CLIENT_EVT_DONE,
                     (const char *)LLM_MB->text + S.read_offset,
                     avail - S.read_offset, LLM_MB->tokens, LLM_MB->ms);
        } else if (avail == 0) {
            dispatch(LLM_CLIENT_EVT_LOST, NULL, 0, LLM_MB->tokens, LLM_MB->ms);
        } else {
            dispatch(LLM_CLIENT_EVT_DONE, NULL, 0, LLM_MB->tokens, LLM_MB->ms);
        }
        S.read_offset = 0;
        S.busy = 0;
        LLM_MB->state = LLM_MB_IDLE;
    }
    return 0;
}

int llm_client_init(void)
{
    if (S.inited) return 0;
    int ret = ic_message_register_by_id(LLM_SERVICE_ID, ipc_callback, NULL);
    if (ret != 0) {
        LISA_LOGE(TAG, "register failed: %d", ret);
        return -1;
    }
    S.inited = 1;
    LISA_LOGI(TAG, "llm client ready (service_id=0x%x)", LLM_SERVICE_ID);
    return 0;
}

int llm_client_subscribe(llm_client_cb_t cb, void *user)
{
    if (!cb) return -1;
    for (int i = 0; i < LLM_CLIENT_SUBS_MAX; i++) {
        if (S.cb[i] == NULL) {
            S.cb[i] = cb;
            S.user[i] = user;
            return 0;
        }
    }
    return -1;
}

int llm_client_ask(const char *question)
{
    if (!S.inited || !question || !question[0]) return -1;
    if (S.busy) return -2;
    if (!ic_message_remote_is_connected(LLM_SERVICE_ID)) return -1;

    /* 写问题到共享内存邮箱 */
    uint32_t qlen = strlen(question);
    if (qlen >= LLM_MB_TEXT_MAX) qlen = LLM_MB_TEXT_MAX - 1;
    memcpy((void *)LLM_MB->text, question, qlen + 1);
    LLM_MB->length = qlen;
    __sync_synchronize();
    LLM_MB->state = LLM_MB_QUESTION;
    __sync_synchronize();
    mb_flush();

    /* 发短通知给 AP（CMD, 4B） */
    llm_notify_t n = { .state = LLM_MB_QUESTION };
    int ret = ic_message_msg_send_by_id(LLM_SERVICE_ID, LLM_IPC_TYPE_CMD,
                                        &n, sizeof n);
    if (ret != 0) {
        LISA_LOGE(TAG, "send failed: %d (AP not ready?)", ret);
        LLM_MB->state = LLM_MB_IDLE;
        mb_flush();
        return -1;
    }
    S.read_offset = 0;
    S.busy = 1;
    return 0;
}

uint8_t llm_client_connected(void)
{
    return S.inited ? ic_message_remote_is_connected(LLM_SERVICE_ID) : 0;
}

uint8_t llm_client_busy(void)
{
    return S.busy;
}

void llm_client_mb_dump(void)
{
    mb_invalidate();
    LISA_LOGI(TAG, "mb_state=%u len=%u tok=%ums=%ums text:",
              LLM_MB->state, LLM_MB->length, LLM_MB->tokens, LLM_MB->ms);
    if (LLM_MB->length > 0 && LLM_MB->length <= LLM_MB_TEXT_MAX)
        LISA_LOGI(TAG, "%.*s", (int)LLM_MB->length, LLM_MB->text);
}
