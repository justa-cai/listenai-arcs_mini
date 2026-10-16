/* llm —— 端侧 LLM 客户端（AP 侧服务经 ic_message 交互）
 *
 * 用法：
 *   llm ask <问题>        发送问题到 AP，等待答案
 *   llm info              查看 AP 侧服务状态
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "cmd.h"
#include "shell.h"
#include "ic_message.h"
#include "lisa_log.h"

#define TAG "llm.cli"

#define LLM_SERVICE_ID   IC_MESSAGE_ID_LLM
#define LLM_MSG_CHAT_REQ  0x01
#define LLM_MSG_CHAT_RESP 0x02
#define LLM_MSG_CHAT_DONE 0x03

/* 接收状态 */
static struct {
    char answer[512];
    int answer_len;
    int gen_tokens;
    int gen_ms;
    bool done;
    SemaphoreHandle_t sem;
    bool inited;
} R;

/* AP 侧回包回调 */
static int32_t llm_client_callback(ic_message_handle_info_t *h,
                                   ic_message_msg_info_t *m)
{
    (void)h;
    if (!m || !m->msg || m->len == 0) return -1;

    switch (m->msg_type) {
    case LLM_MSG_CHAT_RESP:
        if (m->len > sizeof R.answer) m->len = sizeof R.answer;
        memcpy(R.answer, m->msg, m->len);
        R.answer[m->len - 1] = '\0';
        R.answer_len = m->len - 1;
        LISA_LOGI(TAG, "answer: %s", R.answer);
        break;
    case LLM_MSG_CHAT_DONE: {
        struct { int32_t tokens; int32_t ms; } *st = m->msg;
        if (m->len >= 8) {
            R.gen_tokens = st->tokens;
            R.gen_ms = st->ms;
        }
        R.done = true;
        if (R.sem) xSemaphoreGive(R.sem);
        break;
    }
    default:
        break;
    }
    return 0;
}

static int llm_client_init(void)
{
    if (R.inited) return 0;
    R.sem = xSemaphoreCreateBinary();
    if (!R.sem) return -1;
    int ret = ic_message_register_by_id(LLM_SERVICE_ID, llm_client_callback, NULL);
    if (ret != 0) {
        LISA_LOGE(TAG, "register failed: %d", ret);
        return -1;
    }
    R.inited = true;
    LISA_LOGI(TAG, "llm client ready (service_id=0x%x)", LLM_SERVICE_ID);
    return 0;
}

static int llm_cmd_help(int argc, char **argv)
{
    (void)argc; (void)argv;
    printf("usage:\n");
    printf("  llm ask <question>   send to AP, wait for answer\n");
    printf("  llm info             check service status\n");
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
        char buf[256];
        if (join_args(argc, argv, 2, buf, sizeof buf) != 0) {
            printf("need a question\n");
            return -1;
        }
        if (llm_client_init() != 0) {
            printf("llm client init failed\n");
            return -1;
        }
        /* 清状态 */
        R.answer[0] = '\0';
        R.done = false;
        R.gen_tokens = R.gen_ms = 0;

        LISA_LOGI(TAG, "sending: '%s'", buf);
        int ret = ic_message_msg_send_by_id(LLM_SERVICE_ID, LLM_MSG_CHAT_REQ,
                                            buf, strlen(buf) + 1);
        if (ret != 0) {
            printf("send failed: %d (AP not ready?)\n", ret);
            return -1;
        }

        /* 等 AP 推理完（超时 120s） */
        if (xSemaphoreTake(R.sem, pdMS_TO_TICKS(120000)) != pdTRUE) {
            printf("timeout waiting for AP response\n");
            return -1;
        }

        printf("ANSWER(%dms, %d tok): %s\n", R.gen_ms, R.gen_tokens, R.answer);
        return 0;
    }

    if (strcmp(argv[1], "info") == 0) {
        if (llm_client_init() != 0) {
            printf("client init failed\n");
            return -1;
        }
        printf("service_id=0x%x connected=%d\n",
               LLM_SERVICE_ID, ic_message_remote_is_connected(LLM_SERVICE_ID));
        return 0;
    }

    return llm_cmd_help(argc, argv);
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, llm,
                 llm_cmd_handler, on-device llm via IPC);
