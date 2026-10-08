/*
 * sip_service.h - 设备侧 SIP 通话服务（对 apps-ui / app_button / shell 的接口）
 *
 * 生命周期：sip_service_init() 在 main 里调用一次，常驻监听 5060 来电；
 * 媒体任务（音频/视频）在会话建立时懒启动，仿 video_player 的哲学。
 */
#ifndef SIP_SERVICE_H
#define SIP_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

/* 状态机（与 core/sip_ua.h 的对话状态一一对应）。 */
typedef enum {
    SIP_SVC_IDLE = 0,      /* 空闲，监听来电 */
    SIP_SVC_INVITING,      /* 已发 INVITE，等 100/180 */
    SIP_SVC_RINGING_OUT,   /* 对端振铃（收到 180） */
    SIP_SVC_RINGING_IN,    /* 对端呼入，本端振铃 */
    SIP_SVC_ACTIVE,        /* 通话中 */
    SIP_SVC_ENDED,         /* 已结束（短暂显示后回 IDLE） */
} sip_svc_state_t;

int sip_service_init(void);

sip_svc_state_t sip_service_state(void);
bool sip_service_in_call(void);
const char *sip_service_state_name(void);
const char *sip_service_peer_ip(void);

/* 发起呼叫（peer_port 为 0 时用 5060）。 */
int sip_service_start_call(const char *peer_ip, uint16_t peer_port);
int sip_service_answer(void);
int sip_service_reject(void);
int sip_service_hangup(void);
void sip_service_cancel(void);

/* mic 上行入口：app_wakeup 的 post-AEC 通道每帧调用（16k 单声道）。 */
void sip_service_on_mic_pcm(const int16_t *pcm, uint32_t samples);

#endif /* SIP_SERVICE_H */
