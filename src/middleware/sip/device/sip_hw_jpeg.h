#ifndef __SIP_HW_JPEG_H__
#define __SIP_HW_JPEG_H__

#include <stdint.h>

/*
 * sip_hw_jpeg - JPEG 硬件编码（常驻式，DMA2D 路线）。
 *
 * 输入 RGB565（相机原始输出），输出完整 JFIF（YUV422 采样）。
 * 一次 init 常驻；encode 每帧只做 Jpeg_Start/DMA2D/信号量等待。
 * 调用方负责 Jpeg0 外设互斥（s_jpeg_lock）——解码帧的 Initialize
 * 会覆盖编码配置，encode 内部每帧重新 Initialize。
 */
int sip_hw_jpeg_init(uint16_t width, uint16_t height);
/* 编码一帧：rgb565 输入，jpeg 输出（调用方缓冲，需 >= w*h*2）。
 * 返回 0 成功，*jpeg_len 为输出字节数。 */
int sip_hw_jpeg_encode(const uint8_t *rgb565, uint8_t *jpeg, uint32_t *jpeg_len);
/* 解码路径（sip_vidrx 持锁调 jpeg_decoder 前后）调用：置脏标志，
 * 下一次 encode 前重新 Jpeg_Initialize。 */
void sip_hw_jpeg_mark_decode(void);
void sip_hw_jpeg_stop(void);

#endif
