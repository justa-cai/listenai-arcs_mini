/* llm —— 端侧 LLM 客户端（AP 服务，共享内存邮箱 + ic_message 通知）
 *
 * 用法：
 *   llm ask <问题>   异步；答案稍后在日志中出现（I/llm.cli ANSWER: ...）
 *   llm info          服务状态
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "FreeRTOS.h"
#include "task.h"

#include "cmd.h"
#include "shell.h"
#include "ic_message.h"
#include "cache.h"
#include "lisa_log.h"
#include "llm_mailbox.h"

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

#define TAG "llm.cli"

#define LLM_SERVICE_ID   IC_MESSAGE_ID_LLM

static bool s_inited = false;
static uint32_t s_read_offset = 0;   /* 流式读取偏移（追踪已打印部分） */

/* AP 侧回包回调（异步）：PIECE=增量打字机，DONE=最终统计。
 * 注意：ic_message 接收侧 msg_type 硬编码为 CMD（线上不传类型），
 * 不能用 msg_type 区分方向，只能按 state 分发。 */
static int32_t llm_client_callback(ic_message_handle_info_t *h,
                                   ic_message_msg_info_t *m)
{
    (void)h;
    if (!m || !m->msg || m->len < 4) return -1;

    llm_notify_t *n = m->msg;

    if (n->state == LLM_MB_PIECE) {
        /* 流式：打印新到的增量部分（打字机效果） */
        mb_invalidate();
        uint32_t avail = LLM_MB->length;
        if (avail > s_read_offset && avail <= LLM_MB_TEXT_MAX) {
            uint32_t delta = avail - s_read_offset;
            LISA_LOGI(TAG, "> %.*s", (int)delta,
                      LLM_MB->text + s_read_offset);
            s_read_offset = avail;
        }
    } else if (n->state == LLM_MB_DONE) {
        /* 最终：补齐残余 + 统计 */
        mb_invalidate();
        uint32_t avail = LLM_MB->length;
        if (avail > s_read_offset && avail <= LLM_MB_TEXT_MAX) {
            LISA_LOGI(TAG, "> %.*s", (int)(avail - s_read_offset),
                      LLM_MB->text + s_read_offset);
        }
        s_read_offset = 0;
        if (avail > 0) {
            LISA_LOGI(TAG, "DONE(%ums, %u tok, %u ms/tok): %.*s",
                      LLM_MB->ms, LLM_MB->tokens,
                      LLM_MB->tokens > 0 ? LLM_MB->ms / LLM_MB->tokens : 0,
                      (int)avail, LLM_MB->text);
        } else {
            LISA_LOGE(TAG, "DONE: (generate failed)");
        }
        LLM_MB->state = LLM_MB_IDLE;
    }
    return 0;
}

static int llm_client_init(void)
{
    if (s_inited) return 0;
    int ret = ic_message_register_by_id(LLM_SERVICE_ID, llm_client_callback, NULL);
    if (ret != 0) {
        LISA_LOGE(TAG, "register failed: %d", ret);
        return -1;
    }
    s_inited = true;
    LISA_LOGI(TAG, "llm client ready (service_id=0x%x)", LLM_SERVICE_ID);
    return 0;
}

static int llm_cmd_help(int argc, char **argv)
{
    (void)argc; (void)argv;
    LISA_LOGI(TAG, "usage:");
    LISA_LOGI(TAG, "  llm ask <question>   async; answer in log when ready");
    LISA_LOGI(TAG, "  llm last             dump mailbox (selftest/last answer)");
    LISA_LOGI(TAG, "  llm info             service status");
    return 0;
}

static int join_args(int argc, char **argv, int start, char *out, size_t cap)
{
    size_t used = 0;
    out[0] = '\0';
    for (int i = start; i < argc && used + 1 < cap; i++) {
        size_t len = strlen(argv[i]);
        if (used && used + 1 < cap) out[used++] = ' ';
        if (used + len >= cap) len = cap - used - 1;
        memcpy(out + used, argv[i], len);
        used += len;
        out[used] = '\0';
    }
    return used > 0 ? 0 : -1;
}

static int llm_cmd_handler(int argc, char **argv)
{
    if (argc < 2) return llm_cmd_help(argc, argv);

    if (strcmp(argv[1], "ask") == 0) {
        char buf[LLM_MB_TEXT_MAX];
        if (join_args(argc, argv, 2, buf, sizeof buf) != 0) {
            LISA_LOGE(TAG, "need a question");
            return -1;
        }
        if (llm_client_init() != 0) {
            LISA_LOGE(TAG, "client init failed");
            return -1;
        }

        /* 写问题到共享内存邮箱 */
        uint32_t qlen = strlen(buf);
        memcpy((void *)LLM_MB->text, buf, qlen + 1);
        LLM_MB->length = qlen;
        __sync_synchronize();
        LLM_MB->state = LLM_MB_QUESTION;
        __sync_synchronize();
        mb_flush();

        /* 写后回读校验：确认数据真的到达物理 SRAM（AP 视角） */
        mb_invalidate();
        int echo_ok = strcmp((const char *)LLM_MB->text, buf) == 0;
        LISA_LOGI(TAG, "echo verify: %s", echo_ok ? "OK" : "MISMATCH");

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
        LISA_LOGI(TAG, "sent: '%s' (%u B), answer will appear in log", buf, qlen);
        return 0;
    }

    if (strcmp(argv[1], "last") == 0) {
        /* 直接读邮箱（启动自检结果 / 上一次回答） */
        mb_invalidate();
        LISA_LOGI(TAG, "mb_state=%u len=%u tok=%ums=%ums text:", LLM_MB->state, LLM_MB->length, LLM_MB->tokens, LLM_MB->ms);
        if (LLM_MB->length > 0 && LLM_MB->length <= LLM_MB_TEXT_MAX)
            LISA_LOGI(TAG, "%.*s", (int)LLM_MB->length, LLM_MB->text);
        return 0;
    }

    if (strcmp(argv[1], "info") == 0) {
        if (llm_client_init() != 0) {
            LISA_LOGE(TAG, "client init failed");
            return -1;
        }
        mb_invalidate();
        LISA_LOGI(TAG, "service_id=0x%x connected=%d mb_state=%u",
                  LLM_SERVICE_ID,
                  ic_message_remote_is_connected(LLM_SERVICE_ID),
                  LLM_MB->state);
        return 0;
    }

    return llm_cmd_help(argc, argv);
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, llm,
                 llm_cmd_handler, on-device llm via IPC);
