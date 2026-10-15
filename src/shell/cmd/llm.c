/* llm —— 端侧 LLM 调试命令（esp_ai neo 引擎移植）
 *
 * 用法：
 *   llm load [model路径 tok路径]     支持 http://（局域网下载进 PSRAM）
 *                                   默认 /SD:/llm/model.bin + tok.bin
 *   llm ask <问题>                  问答（User:/Bot: 模板）
 *   llm story <主题>                TinyStories 讲故事
 *   llm info                        加载/arena/内存状态
 *   llm free                        卸载释放 PSRAM
 * 例：llm load http://192.168.31.205:8000/model.bin http://192.168.31.205:8000/tok.bin
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "cmd.h"
#include "shell.h"
#include "llm_app.h"

static int llm_cmd_help(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    printf("usage:\n");
    printf("  llm load flash [model_off tok_off]  default 920000 EC0000\n");
    printf("  llm load [model_path tok_path]      file/http path\n");
    printf("  llm ask <question>\n");
    printf("  llm story <topic>\n");
    printf("  llm info\n");
    printf("  llm free\n");
    return 0;
}

/* 把 argv[start..argc) 拼成一个句子（shell 的 "..." 不会保引号原样拼接） */
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
    if (argc < 2) {
        return llm_cmd_help(argc, argv);
    }

    char buf[480];

    if (strcmp(argv[1], "load") == 0) {
        if (argc >= 3 && strcmp(argv[2], "flash") == 0) {
            uint32_t model_off = argc >= 4 ? strtoul(argv[3], NULL, 16) : 0x920000;
            uint32_t tok_off = argc >= 5 ? strtoul(argv[4], NULL, 16) : 0xEC0000;
            return llm_app_load_flash(model_off, tok_off);
        }
        return llm_app_load(argc >= 4 ? argv[2] : NULL,
                            argc >= 4 ? argv[3] : NULL) == 0 ? 0 : -1;
    }
    if (strcmp(argv[1], "free") == 0) {
        return llm_app_unload();
    }
    if (strcmp(argv[1], "info") == 0) {
        llm_app_info();
        return 0;
    }
    if (strcmp(argv[1], "ask") == 0) {
        if (join_args(argc, argv, 2, buf, sizeof buf) != 0) {
            printf("need a question\n");
            return -1;
        }
        return llm_app_ask(buf);
    }
    if (strcmp(argv[1], "story") == 0) {
        if (join_args(argc, argv, 2, buf, sizeof buf) != 0) {
            printf("need a topic\n");
            return -1;
        }
        return llm_app_story(buf);
    }
    return llm_cmd_help(argc, argv);
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, llm,
                 llm_cmd_handler, on-device llm debug);
