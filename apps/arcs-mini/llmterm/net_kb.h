/* net_kb —— 网络键盘（UDP）+ 设备发现
 *
 * 协议（端口 38001，文本前缀 "LLMKB1:"，netcat 可手测）：
 *   PC→设备 广播 "LLMKB1:D"          发现探测，设备单播应答 "LLMKB1:d arcs-mini <ip>"
 *   PC→设备 单播 "LLMKB1:L" + 整行   整行输入（回车时发送，一行一包避免丢键）
 *   PC→设备 单播 "LLMKB1:K" + 1字节  单按键（'\n' 回车 / '\b' 退格；兼容保留）
 *   PC→设备 单播 "LLMKB1:H"          心跳（30s 无包判定 PC 离线）
 *
 * WiFi 拿到 IP 后（VOICE_MSG_WIFI_IP_GOT）启动 UDP 任务；
 * 按键经 FreeRTOS 队列交给 UI 侧轮询（worq.ui）。
 */
#ifndef NET_KB_H
#define NET_KB_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 注册 WiFi IP 事件（幂等，main 启动期调用一次） */
int net_kb_init(void);

/* 非阻塞取一个按键；无按键返回 -1，否则返回字节值 */
int net_kb_getkey(void);

/* 非阻塞取整行输入（LLMKB1:L 包，PC 回车时发送）；0=取到，-1=无 */
int net_kb_getline(char *buf, unsigned int len);

/* 调试用：向按键队列注入一串字符 + 回车（adb shell 演示用） */
int net_kb_test_inject(const char *str);

/* 程序化注入整行输入（云端 ASR 结果 / 调试命令）；任意线程安全。
 * 与 PC 的 LLMKB1:L 包同队列同路径，worq.ui 轮询后自动提交。
 * 返回 0=入队；-1=参数空或队列满（终端 busy，调用方自行提示） */
int net_kb_line_push(const char *line);

/* PC 在线（30s 内有心跳/按键）；ipstr 返回本机 IP 字符串 */
uint8_t net_kb_peer_alive(void);
const char *net_kb_ip_str(void);

#ifdef __cplusplus
}
#endif
#endif
