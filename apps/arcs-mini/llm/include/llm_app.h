#ifndef ARCS_MINI_LLM_LLM_APP_H
#define ARCS_MINI_LLM_LLM_APP_H

#include <stdbool.h>
#include <stddef.h>

/* 端侧 LLM（esp_ai neo 引擎移植）加载/推理入口。
 *
 * 模型工件放 SD 卡：/SD:/llm/model.bin + /SD:/llm/tok.bin
 * （CRDP v3 blob 需 16 字节对齐，加载到 PSRAM）。 */

int llm_app_load(const char *model_path, const char *tok_path);
/* flash 驻留加载：模型/tok 已由 recovery 烧写到裸 flash 固定地址，
 * wte 直接 XIP，head/body/tok 拷入 PSRAM。地址为 flash 偏移（hex）。 */
int llm_app_load_flash(uint32_t model_off, uint32_t tok_off);
int llm_app_unload(void);
bool llm_app_is_loaded(void);

/* 同步问答：生成过程按 piece 逐段打日志。返回 0 成功。 */
int llm_app_ask(const char *question);

/* TinyStories 原生模板讲故事（Summary:/Story:）。 */
int llm_app_story(const char *topic);

/* 加载后打印 arena/内存占用情况。 */
void llm_app_info(void);

#endif /* ARCS_MINI_LLM_LLM_APP_H */
