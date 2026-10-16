/* llm —— 端侧 LLM 客户端命令（IPC 逻辑在 src/framework/llm_client.c）
 *
 * 用法：
 *   llm ask <问题>   异步；答案稍后在日志中出现（打字机 + DONE 统计）
 *   llm last         dump mailbox（启动自检结果 / 上一次回答）
 *   llm info          服务状态
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "cmd.h"
#include "shell.h"
#include "lisa_log.h"
#include "llm_client.h"
#include "llm_mailbox.h"

#define TAG "llm.cli"

/* 订阅回调（rpc_client 线程）：打字机打印 + DONE 统计 */
static void shell_on_llm_evt(void *user, int evt, const char *delta,
                             uint32_t len, uint32_t tokens, uint32_t ms)
{
    (void)user;
    if (evt == LLM_CLIENT_EVT_PIECE && delta && len) {
        LISA_LOGI(TAG, "> %.*s", (int)len, delta);
    } else if (evt == LLM_CLIENT_EVT_DONE) {
        if (delta && len) LISA_LOGI(TAG, "> %.*s", (int)len, delta);
        LISA_LOGI(TAG, "DONE(%ums, %u tok, %u ms/tok)",
                  ms, tokens, tokens > 0 ? ms / tokens : 0);
    } else if (evt == LLM_CLIENT_EVT_LOST) {
        LISA_LOGE(TAG, "DONE: (generate failed)");
    }
}

static int llm_client_start(void)
{
    static bool s_inited = false;
    if (s_inited) return 0;
    if (llm_client_init() != 0) return -1;
    if (llm_client_subscribe(shell_on_llm_evt, NULL) != 0) {
        LISA_LOGE(TAG, "subscribe failed");
        return -1;
    }
    s_inited = true;
    LISA_LOGI(TAG, "llm shell client ready");
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
        if (llm_client_start() != 0) {
            LISA_LOGE(TAG, "client init failed");
            return -1;
        }
        int ret = llm_client_ask(buf);
        if (ret == -2) {
            LISA_LOGE(TAG, "busy: a request is already in flight");
            return -1;
        }
        if (ret != 0) {
            LISA_LOGE(TAG, "ask failed (AP not ready?)");
            return -1;
        }
        LISA_LOGI(TAG, "sent: '%s' (%u B), answer will appear in log",
                  buf, (unsigned)strlen(buf));
        return 0;
    }

    if (strcmp(argv[1], "last") == 0) {
        llm_client_mb_dump();
        return 0;
    }

    if (strcmp(argv[1], "info") == 0) {
        if (llm_client_start() != 0) {
            LISA_LOGE(TAG, "client init failed");
            return -1;
        }
        LISA_LOGI(TAG, "connected=%d busy=%d",
                  llm_client_connected(), llm_client_busy());
        return 0;
    }

    return llm_cmd_help(argc, argv);
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, llm,
                 llm_cmd_handler, on-device llm via IPC);
