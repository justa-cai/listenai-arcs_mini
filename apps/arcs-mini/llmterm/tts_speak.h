/* tts_speak —— LLM 答案逐句云端 TTS 朗读
 *
 * 作为 llm_client 的第三个订阅者拦截流式增量：在 rpc_client 线程内
 * 做纯内存的句子切分（. ! ? \n 为界），整句经 FreeRTOS 队列投递给
 * 低优先级 "tts.speak" 任务，串行调 voice_cloud_tts_synth() 云端合成，
 * 播放由既有 voice_player_tts 链路自动完成（焦点/PA/排队现成）。
 *
 * 云端不在线时静默跳过（不影响终端屏显）；新提问会打断上一段朗读。
 */
#ifndef TTS_SPEAK_H
#define TTS_SPEAK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 初始化（幂等）：创建队列/任务并注册 LLM 订阅。失败不阻断终端。 */
int tts_speak_init(void);

/* 新提问开始：丢弃未闭合句、清空排队句子并打断在途合成/播放。
 * 由 term_ui submit_line 在 ask 成功后调用（worq.ui 线程安全，
 * 网络 cancel 动作经队列传给 tts.speak 任务执行）。 */
void tts_speak_reset(void);

/* 运行时开关（默认开）。 */
void tts_speak_set_enable(uint8_t on);
uint8_t tts_speak_enabled(void);

/* 诊断：云端是否就绪 / 队列丢句计数。 */
uint8_t tts_speak_cloud_ready(void);
uint32_t tts_speak_dropped(void);

#ifdef __cplusplus
}
#endif
#endif
