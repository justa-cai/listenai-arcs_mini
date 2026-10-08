/*
 * sip_g711.c - G.711 A律/μ律 编解码实现
 *
 * 采用 ITU G.711 参考实现（CCITT Sun 版，公有领域）的位运算写法：
 * 编码走分段搜索，解码走移位重建，无静态表、无状态。每样本几条指令，
 * RV32 300MHz 上 8k samples/s 的占用可以忽略。
 */
#include "sip_g711.h"

/* 分段边界：seg_end[i] 是第 i 段的最大幅值。 */
static const int16_t seg_uend[8] = {0x3F, 0x7F, 0xFF, 0x1FF,
                                    0x3FF, 0x7FF, 0xFFF, 0x1FFF};
static const int16_t seg_aend[8] = {0x1F, 0x3F, 0x7F, 0xFF,
                                    0x1FF, 0x3FF, 0x7FF, 0xFFF};

static int seg_search(int val, const int16_t *table, int count)
{
    int i;
    for (i = 0; i < count; ++i) {
        if (val <= table[i]) {
            return i;
        }
    }
    return count;
}

uint8_t sip_g711_pcm_to_ulaw(int16_t pcm)
{
    const int BIAS = 0x84;
    const int CLIP = 8159;
    int16_t mask;
    int16_t seg;
    uint8_t uval;

    /* u 律按 14 位精度处理。 */
    pcm >>= 2;
    if (pcm < 0) {
        pcm = -pcm;
        mask = 0x7F;
    } else {
        mask = 0xFF;
    }
    if (pcm > CLIP) {
        pcm = CLIP;
    }
    pcm += (int16_t)(BIAS >> 2);

    seg = (int16_t)seg_search(pcm, seg_uend, 8);

    /* 段号 + 段内量化位，取反得到传输码字。 */
    if (seg >= 8) {
        return (uint8_t)(0x7F ^ mask);
    }
    uval = (uint8_t)((seg << 4) | ((pcm >> (seg + 1)) & 0x0F));
    return (uint8_t)(uval ^ mask);
}

int16_t sip_g711_ulaw_to_pcm(uint8_t u_val)
{
    int16_t t;

    u_val = (uint8_t)~u_val;
    /* 量化位取出并加偏置，再按段号左移。 */
    t = (int16_t)(((u_val & 0x0F) << 3) + 0x84);
    t <<= (u_val & 0x70) >> 4;
    return (int16_t)((u_val & 0x80) ? (0x84 - t) : (t - 0x84));
}

uint8_t sip_g711_pcm_to_alaw(int16_t pcm)
{
    int16_t mask;
    int16_t seg;
    uint8_t aval;

    /* A 律按 13 位精度处理。 */
    pcm >>= 3;
    if (pcm >= 0) {
        mask = 0xD5;
    } else {
        mask = 0x55;
        pcm = (int16_t)(-pcm - 1);
    }

    seg = (int16_t)seg_search(pcm, seg_aend, 8);
    if (seg >= 8) {
        return (uint8_t)(0x7F ^ mask);
    }
    aval = (uint8_t)(seg << 4);
    if (seg < 2) {
        aval |= (uint8_t)((pcm >> 1) & 0x0F);
    } else {
        aval |= (uint8_t)((pcm >> seg) & 0x0F);
    }
    return (uint8_t)(aval ^ mask);
}

int16_t sip_g711_alaw_to_pcm(uint8_t a_val)
{
    int16_t t;
    int seg;

    a_val ^= 0x55;
    t = (int16_t)((a_val & 0x0F) << 4);
    seg = (a_val & 0x70) >> 4;
    switch (seg) {
    case 0:
        t += 8;
        break;
    case 1:
        t += 0x108;
        break;
    default:
        t += 0x108;
        t <<= seg - 1;
        break;
    }
    return (int16_t)((a_val & 0x80) ? t : -t);
}

void sip_g711_encode(int pt, const int16_t *pcm, uint8_t *out, uint32_t samples)
{
    if (pt == SIP_G711_PT_PCMA) {
        for (uint32_t i = 0; i < samples; ++i) {
            out[i] = sip_g711_pcm_to_alaw(pcm[i]);
        }
    } else {
        for (uint32_t i = 0; i < samples; ++i) {
            out[i] = sip_g711_pcm_to_ulaw(pcm[i]);
        }
    }
}

void sip_g711_decode(int pt, const uint8_t *in, int16_t *pcm, uint32_t samples)
{
    if (pt == SIP_G711_PT_PCMA) {
        for (uint32_t i = 0; i < samples; ++i) {
            pcm[i] = sip_g711_alaw_to_pcm(in[i]);
        }
    } else {
        for (uint32_t i = 0; i < samples; ++i) {
            pcm[i] = sip_g711_ulaw_to_pcm(in[i]);
        }
    }
}

int sip_g711_pt_supported(int pt)
{
    return pt == SIP_G711_PT_PCMU || pt == SIP_G711_PT_PCMA;
}
