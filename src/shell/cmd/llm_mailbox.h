/* LLM IPC 邮箱协议：长文本走共享内存，ic_message 只发 ≤10B 通知
 *
 * 布局（LUNA 共享区尾部 0x20048000，两核可直接访问）：
 *   [0]  state:   0=idle 1=question_ready 2=answer_ready
 *   [4]  length:  文本字节数（不含 NUL）
 *   [8]  tokens:  生成 token 数（答案时有效）
 *   [12] ms:      生成耗时 ms
 *   [16] text[480]: 问题或答案文本（NUL 结尾）
 *
 * 流程：
 *   CP: 写 text+length → state=1 → ic_message(CMD, &state, 4)
 *   AP: 收通知 → 读 text → 推理 → 写 text+length+tokens+ms → state=2
 *       → ic_message(EVT, &state, 4)
 *   CP: 收通知 → state==2 → 读 text 打印答案
 */
#ifndef LLM_MAILBOX_H
#define LLM_MAILBOX_H

#include <stdint.h>

#define LLM_MAILBOX_ADDR   0x20048000u   /* LUNA 共享区尾部，两核可访问 */

/* ic_message 有效 type 值（系统枚举） */
#define LLM_IPC_TYPE_CMD   1   /* IC_MESSAGE_MSG_TYPE_CMD */
#define LLM_IPC_TYPE_EVT   2   /* IC_MESSAGE_MSG_TYPE_EVT */

/* mailbox state 值 */
#define LLM_MB_IDLE       0
#define LLM_MB_QUESTION   1
#define LLM_MB_ANSWER     2
#define LLM_MB_PIECE      3   /* 流式：每个 token 通知一次 */
#define LLM_MB_DONE       4   /* 全部生成完 */

#define LLM_MB_TEXT_MAX   480

typedef struct __attribute__((packed)) {
    volatile uint32_t state;
    volatile uint32_t length;
    volatile uint32_t tokens;
    volatile uint32_t ms;
    char text[LLM_MB_TEXT_MAX];
} llm_mailbox_t;

#define LLM_MB  ((volatile llm_mailbox_t *)LLM_MAILBOX_ADDR)

/* 通知负载（≤10 字节，满足 ic_message 限制） */
typedef struct __attribute__((packed)) {
    uint32_t state;       /* LLM_MB_QUESTION 或 LLM_MB_ANSWER */
} llm_notify_t;

#endif /* LLM_MAILBOX_H */
