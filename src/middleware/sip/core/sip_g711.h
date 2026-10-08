/*
 * sip_g711.h - G.711 语音编解码（PCMU=μ律 / PCMA=A律，8kHz 8bit）
 *
 * 纯查表/位运算，无状态、无依赖。通话音频的 20ms 包 = 160 个 G.711 字节。
 */
#ifndef SIP_G711_H
#define SIP_G711_H

#include <stdint.h>

/* RTP payload type 与编码的对应：0 = PCMU(μ律)，8 = PCMA(A律)。 */
#define SIP_G711_PT_PCMU 0u
#define SIP_G711_PT_PCMA 8u

uint8_t sip_g711_pcm_to_ulaw(int16_t pcm);
int16_t sip_g711_ulaw_to_pcm(uint8_t u_val);
uint8_t sip_g711_pcm_to_alaw(int16_t pcm);
int16_t sip_g711_alaw_to_pcm(uint8_t a_val);

/* 批量转换（设备/主机两侧的 20ms 帧路径）。 */
void sip_g711_encode(int pt, const int16_t *pcm, uint8_t *out, uint32_t samples);
void sip_g711_decode(int pt, const uint8_t *in, int16_t *pcm, uint32_t samples);

/* 判断 RTP payload type 是否是我们支持的 G.711，是则返回 true。 */
int sip_g711_pt_supported(int pt);

#endif /* SIP_G711_H */
