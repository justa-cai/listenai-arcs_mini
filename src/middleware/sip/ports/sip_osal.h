/*
 * sip_osal.h - SIP SDK 的操作系统抽象层（单一头文件，每个目标平台一个实现）
 *
 * 核心协议代码（core 目录下的 .c）只允许依赖本头 + C 标准库，不允许出现任何
 * FreeRTOS / lwip / POSIX 头 —— 这是"设备固件与 Ubuntu 主机程序共用同一份
 * core .c"的前提（先例：lisa_sntp.c / gamepad_udp.c 同样零 FreeRTOS 依赖）。
 *
 * 平台实现：
 *   ports/sip_osal_freertos.c  设备（FreeRTOS + lwip BSD socket）
 *   ports/sip_osal_posix.c     Ubuntu 主机（glibc）
 */
#ifndef SIP_OSAL_H
#define SIP_OSAL_H

#include <stddef.h>
#include <stdint.h>

/* 大缓冲段属性。曾放 .psram.bss 省栈，但真机证明该段每 +8KB 会把整个
 * PSRAM 布局平移（.psram.noinit/.psram.text/.miniapp.psram.* 全后移），
 * 堆元数据被踩 → 之后任意 TLSF 大分配（如 lisa_thread 的线程栈）在
 * 伪地址上 Store Fault。现在缓冲回到任务栈（sip_sig 16KB，余量充足），
 * 宏保留为空以维持调用点写法。 */
#define SIP_BSS

/* 日志级别。DBG 在设备侧走 LISA_LOGI（elog 无 D 级别）。 */
enum {
    SIP_LOG_ERR = 0,
    SIP_LOG_WARN = 1,
    SIP_LOG_INFO = 2,
    SIP_LOG_DBG = 3,
};

/* 全局日志级别开关，初始化时由宿主设置；低于该级别的日志直接丢弃。 */
void sip_osal_set_log_level(int level);

/*
 * UDP socket：绑定 INADDR_ANY:port，设置 SO_REUSEADDR。
 * 返回 fd（>=0）或 -1。
 */
int sip_osal_udp_open(uint16_t port);

/* 向 ip:port 发送 UDP 数据报。返回发送字节数或 -1。 */
int sip_osal_udp_send(int fd, const char *ip, uint16_t port,
                      const void *data, size_t len);

/*
 * 带超时的 UDP 接收。返回值：
 *   >0  收到的字节数（from_ip/from_port 可为 NULL，表示不关心来源）
 *    0  超时（超时同时充当外层任务轮询节拍，见 gamepad_udp 范式）
 *   -1  socket 错误
 * from_ip 缓冲区调用方提供，建议 >= 16 字节。
 */
int sip_osal_udp_recv(int fd, void *buf, size_t len, uint32_t timeout_ms,
                      char *from_ip, uint16_t *from_port);

/* 关闭 socket；也用于从外部解除阻塞中的 recv（设备侧停止任务的既定手法）。 */
void sip_osal_udp_close(int fd);

/* 单调递增毫秒时钟（用于事务重传与状态机超时，不用墙钟）。 */
uint32_t sip_osal_now_ms(void);

/* 伪随机数（branch/tag/Call-ID/SSRC/seq 初值）。两端口各自用 xorshift32。 */
uint32_t sip_osal_rand(void);

void sip_osal_sleep_ms(uint32_t ms);

/*
 * 推导本机 IP：临时 UDP socket connect(peer_ip) 后 getsockname。
 * 失败返回 -1（调用方应中止呼叫并提示网络不可用）。out 建议 >= 16 字节。
 */
int sip_osal_get_local_ip(const char *peer_ip, char *out, size_t outlen);

/* tag 用于日志归属（设备侧 "sip"，主机侧 "sip_peer"）。 */
void sip_osal_log(int level, const char *tag, const char *fmt, ...);

#endif /* SIP_OSAL_H */
