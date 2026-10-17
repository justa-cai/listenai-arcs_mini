/* voice_asr —— 按住功能键说话 → 云端 ASR → 文本进终端自动提交
 *
 * 交互：按住 PB4（active-low）录音（屏幕提示 "(listening...)"），
 * 松开（或满 5s 自动截断）结束上行，云端 IAT 识别；识别文本经
 * net_kb_line_push 走终端整行通道自动提交给 LLM。
 *
 * 依赖 WiFi + LSC 云；离线 / LLM 忙 / 功能关闭时按住只提示不采集。
 */
#ifndef VOICE_ASR_H
#define VOICE_ASR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 初始化（幂等）：ebus 订阅 + 创建 asr.ptt 轮询线程。失败不阻断终端。 */
int voice_asr_init(void);

/* 运行时开关（默认开）。 */
void voice_asr_set_enable(uint8_t on);
uint8_t voice_asr_enabled(void);

/* 诊断。 */
uint8_t voice_asr_cloud_ready(void);
/* 状态字符串："idle"/"held"/"rec"/"wait" */
const char *voice_asr_state_str(void);
void voice_asr_stat_get(uint32_t *press, uint32_t *frames,
                        uint32_t *results, uint32_t *timeouts,
                        uint32_t *drops, uint32_t *offline);

#ifdef __cplusplus
}
#endif
#endif
