/* llmterm —— 终端模块调试命令（adb 演示：无网络键盘时注入按键）
 *
 * 用法：
 *   llmterm key <text>   向终端注入一串字符并回车（等同键盘输入）
 *   llmterm stat         网络/键盘状态
 */
#include <stdio.h>
#include <string.h>

#include "shell.h"
#include "lisa_log.h"
#include "net_kb.h"
#include "llm_client.h"

#define TAG "llmterm"

static int llmterm_cmd_handler(int argc, char **argv)
{
    if (argc < 2) {
        LISA_LOGI(TAG, "usage:");
        LISA_LOGI(TAG, "  llmterm key <text>   inject keys + Enter");
        LISA_LOGI(TAG, "  llmterm stat         net/kb status");
        return 0;
    }

    if (strcmp(argv[1], "key") == 0 && argc >= 3) {
        char buf[128];
        size_t used = 0;
        buf[0] = '\0';
        for (int i = 2; i < argc && used + 1 < sizeof buf; i++) {
            size_t l = strlen(argv[i]);
            if (used) buf[used++] = ' ';
            if (used + l >= sizeof buf) l = sizeof buf - used - 1;
            memcpy(buf + used, argv[i], l);
            used += l;
            buf[used] = '\0';
        }
        net_kb_test_inject(buf);
        LISA_LOGI(TAG, "injected: '%s'", buf);
        return 0;
    }

    if (strcmp(argv[1], "stat") == 0) {
        LISA_LOGI(TAG, "ip=%s kb_peer=%d llm_conn=%d llm_busy=%d",
                  net_kb_ip_str(), net_kb_peer_alive(),
                  llm_client_connected(), llm_client_busy());
        return 0;
    }

    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, llmterm,
                 llmterm_cmd_handler, llm terminal debug helpers);
