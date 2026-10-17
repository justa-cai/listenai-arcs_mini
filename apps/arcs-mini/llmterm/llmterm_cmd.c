/* llmterm —— 终端模块调试命令（adb 演示：无网络键盘时注入按键）
 *
 * 用法：
 *   llmterm key <text>    向终端注入一串字符并回车（等同键盘输入）
 *   llmterm stat          网络/键盘/TTS/ASR 状态
 *   llmterm tts on|off    答案云端 TTS 朗读开关（默认 on）
 *   llmterm asr on|off    按住说话云端识别开关（默认 on）
 *   llmterm asr inject <text>  模拟识别结果整行提交（调试用）
 */
#include <stdio.h>
#include <string.h>

#include "shell.h"
#include "lisa_log.h"
#include "net_kb.h"
#include "llm_client.h"
#include "tts_speak.h"
#include "voice_asr.h"

#define TAG "llmterm"

static int llmterm_cmd_handler(int argc, char **argv)
{
    if (argc < 2) {
        LISA_LOGI(TAG, "usage:");
        LISA_LOGI(TAG, "  llmterm key <text>   inject keys + Enter");
        LISA_LOGI(TAG, "  llmterm stat         net/kb/tts/asr status");
        LISA_LOGI(TAG, "  llmterm tts on|off   cloud TTS speaking");
        LISA_LOGI(TAG, "  llmterm asr on|off|inject <text>");
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

    if (strcmp(argv[1], "tts") == 0) {
        if (argc >= 3) {
            tts_speak_set_enable(strcmp(argv[2], "on") == 0);
        }
        LISA_LOGI(TAG, "tts enabled=%d cloud=%d dropped=%u",
                  tts_speak_enabled(), tts_speak_cloud_ready(),
                  (unsigned)tts_speak_dropped());
        return 0;
    }

    if (strcmp(argv[1], "asr") == 0) {
        if (argc >= 4 && strcmp(argv[2], "inject") == 0) {
            char buf[128];
            size_t used = 0;
            buf[0] = '\0';
            for (int i = 3; i < argc && used + 1 < sizeof buf; i++) {
                size_t l = strlen(argv[i]);
                if (used) buf[used++] = ' ';
                if (used + l >= sizeof buf) l = sizeof buf - used - 1;
                memcpy(buf + used, argv[i], l);
                used += l;
                buf[used] = '\0';
            }
            if (net_kb_line_push(buf) != 0) {
                LISA_LOGW(TAG, "asr inject dropped (line queue full)");
            } else {
                LISA_LOGI(TAG, "asr injected: '%s'", buf);
            }
            return 0;
        }
        if (argc >= 3) {
            voice_asr_set_enable(strcmp(argv[2], "on") == 0);
        }
        uint32_t press, frames, results, tmo, drops, off;
        voice_asr_stat_get(&press, &frames, &results, &tmo, &drops, &off);
        LISA_LOGI(TAG, "asr enabled=%d cloud=%d state=%s press=%u frames=%u "
                  "ok=%u tmo=%u drop=%u off=%u",
                  voice_asr_enabled(), voice_asr_cloud_ready(),
                  voice_asr_state_str(), (unsigned)press, (unsigned)frames,
                  (unsigned)results, (unsigned)tmo, (unsigned)drops,
                  (unsigned)off);
        return 0;
    }

    if (strcmp(argv[1], "stat") == 0) {
        uint32_t press, frames, results, tmo, drops, off;
        voice_asr_stat_get(&press, &frames, &results, &tmo, &drops, &off);
        LISA_LOGI(TAG, "ip=%s kb_peer=%d llm_conn=%d llm_busy=%d "
                  "tts=%d/%d drop=%u asr=%d/%d %s ok=%u",
                  net_kb_ip_str(), net_kb_peer_alive(),
                  llm_client_connected(), llm_client_busy(),
                  tts_speak_enabled(), tts_speak_cloud_ready(),
                  (unsigned)tts_speak_dropped(),
                  voice_asr_enabled(), voice_asr_cloud_ready(),
                  voice_asr_state_str(), (unsigned)results);
        return 0;
    }

    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, llmterm,
                 llmterm_cmd_handler, llm terminal debug helpers);
