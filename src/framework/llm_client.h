/* llm_client —— 端侧 LLM IPC 客户端服务层（CP 侧，多订阅者）
 *
 * 独占 ic_message IC_MESSAGE_ID_LLM 注册，向业务（shell / 终端 UI 等）
 * 提供订阅式流式回包与 ask 提交。原逻辑抽自 src/shell/cmd/llm.c。
 *
 * 线程模型：订阅回调在 urpc "rpc_client" 线程执行（栈 2048，勿阻塞、
 * 勿直接调 LVGL —— UI 侧需经 LISA_UI_INVOKE_UI 投递）。
 */
#ifndef LLM_CLIENT_H
#define LLM_CLIENT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    LLM_CLIENT_EVT_PIECE = 1, /* delta=本次新增文本（打字机增量） */
    LLM_CLIENT_EVT_DONE  = 2, /* delta=残余文本（已剥离 Q-echo），tokens/ms 有效 */
    LLM_CLIENT_EVT_LOST  = 3, /* 生成失败 / 空答案 */
};

typedef void (*llm_client_cb_t)(void *user, int evt, const char *delta,
                                uint32_t len, uint32_t tokens, uint32_t ms);

/* 初始化并独占注册 IPC 服务（幂等，可重复调用） */
int llm_client_init(void);

/* 订阅回包事件（最多 LLM_CLIENT_SUBS_MAX 个订阅者） */
int llm_client_subscribe(llm_client_cb_t cb, void *user);

/* 提交问题（写 mailbox + cache flush + CMD 通知）。
 * 返回 0=已提交；-1=参数/未初始化/未连接；-2=已有请求在途 */
int llm_client_ask(const char *question);

uint8_t llm_client_connected(void); /* AP 服务是否在线 */
uint8_t llm_client_busy(void);      /* 是否有请求在途 */

/* mailbox 原始读取（诊断：启动自检结果 / 上一次回答） */
void llm_client_mb_dump(void);

#ifdef __cplusplus
}
#endif
#endif
