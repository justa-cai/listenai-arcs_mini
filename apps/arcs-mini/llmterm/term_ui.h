/* term_ui 对外接口（跨模块/跨线程用） */
#ifndef TERM_UI_H
#define TERM_UI_H

#ifdef __cplusplus
extern "C" {
#endif

/* 非 worq.ui 线程输出一行灰字系统提示（如 ASR 状态）；workq 投递安全。
 * 文本 ≤79 字节，超长截断。 */
void term_ui_sys_hint(const char *s);

#ifdef __cplusplus
}
#endif
#endif
