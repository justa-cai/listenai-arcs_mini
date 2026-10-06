/*
 * h264dec.c - H.264 Baseline Profile (CAVLC, I/P) 纯 C 软件解码器
 *
 * 从 TinyH264 (pschatzmann/TinyH264) 的 C++ header-only 实现重构为 C。
 * 按原始模块的顺序组织（bitreader / nal / sps_pps / slice_header /
 * cavlc / transform / intra_pred / motion / mv_predict / mb_info /
 * macroblock / macroblock_inter / deblock / frame / tables / decoder 顶层），
 * 算法逐函数等价翻译，包含上游经 ffmpeg 逐像素比对验证过的所有易错点
 * （均有注释标明）：
 *   - CAVLC level 后缀长度增长顺序（先 0->1 再判增长，两步都做）
 *   - te(v) ref_idx：numActiveRefs<=1 不读位、==2 读反相单比特
 *   - MV 预测：方向性捷径先于通用流程；零匹配且 B/C 真不可用而 A 可用
 *     时直接取 A（多参考帧流才会暴露的规则）
 *   - Intra4x4 模式预测的联合 DC 条件与独立"非 4x4 编码"回退两层规则
 *   - 色度 DC 预测按 4x4 象限分别求均值（右下象限取上+左 4..7）
 *   - 去块 bS 由同位置亮度块属性推导，色度用 4 组 bS 各滤波 2 采样
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "h264dec.h"

/* ---- 分配钩子：默认固件构建走 PSRAM 堆，桌面验证走 malloc ---- */
#ifndef H264_DEC_ALLOC
#if defined(CONFIG_PSRAM_HEAP)
#include "sysheap.h"
#define H264_DEC_ALLOC(sz) psram_malloc(sz)
#define H264_DEC_FREE(p) psram_free(p)
#else
#include <stdlib.h>
#define H264_DEC_ALLOC(sz) malloc(sz)
#define H264_DEC_FREE(p) free(p)
#endif
#endif

/* ---- 调试日志：默认关闭（嵌入式零开销） ---- */
#if defined(H264_DEC_DEBUG) && H264_DEC_DEBUG
#include <stdio.h>
#define H264_LOG(...) do { fprintf(stderr, __VA_ARGS__); } while (0)
#else
#define H264_LOG(...) do {} while (0)
#endif

/* ---- 编译期上限（与上游 h264_config.h 对应） ---- */
#ifndef H264_DEC_MAX_W
#define H264_DEC_MAX_W 320
#endif
#ifndef H264_DEC_MAX_H
#define H264_DEC_MAX_H 240
#endif
#ifndef H264_DEC_MAX_REFS
#define H264_DEC_MAX_REFS 3
#endif
#ifndef H264_DEC_MAX_NAL
#define H264_DEC_MAX_NAL 32768
#endif
#define H264_MAX_SPS 4
#define H264_MAX_PPS 4
#define H264_MAX_POC_CYCLE 8
#define H264_MB_SIZE 16

/* ================================================================== */
/* 常量（上游 h264_nal_types.h / h264_mb_info.h / h264_tables.h）       */
/* ================================================================== */

enum {
    H264_NAL_SLICE_NON_IDR = 1,
    H264_NAL_SLICE_IDR = 5,
    H264_NAL_SEI = 6,
    H264_NAL_SPS = 7,
    H264_NAL_PPS = 8,
};

/* 宏块编码类型（上游 MbType） */
enum {
    H264_MBT_INTRA4X4 = 0,
    H264_MBT_INTRA16X16 = 1,
    H264_MBT_INTRA_PCM = 2,
    H264_MBT_P_SKIP = 3,
    H264_MBT_INTER = 4,
};

/* P slice mb_type 0..4（上游 PMbType）；>=5 为 P slice 内 Intra 宏块 */
enum {
    H264_P_L0_16x16 = 0,
    H264_P_L0_L0_16x8 = 1,
    H264_P_L0_L0_8x16 = 2,
    H264_P_8x8 = 3,
    H264_P_8x8ref0 = 4,
};
/* sub_mb_type */
enum { H264_SUB_8x8 = 0, H264_SUB_8x4 = 1, H264_SUB_4x8 = 2, H264_SUB_4x4 = 3 };

/* slice_type % 5（上游 SliceTypeBase） */
enum {
    H264_SLICE_P = 0,
    H264_SLICE_B = 1,
    H264_SLICE_I = 2,
    H264_SLICE_SP = 3,
    H264_SLICE_SI = 4,
};

/* Intra4x4 / Intra16x16 / 色率预测模式（上游 h264_intra_pred.h） */
enum {
    H264_I4_VERTICAL = 0,
    H264_I4_HORIZONTAL = 1,
    H264_I4_DC = 2,
    H264_I4_DIAG_DOWN_LEFT = 3,
    H264_I4_DIAG_DOWN_RIGHT = 4,
    H264_I4_VERTICAL_RIGHT = 5,
    H264_I4_HORIZONTAL_DOWN = 6,
    H264_I4_VERTICAL_LEFT = 7,
    H264_I4_HORIZONTAL_UP = 8,
};
enum {
    H264_I16_VERTICAL = 0,
    H264_I16_HORIZONTAL = 1,
    H264_I16_DC = 2,
    H264_I16_PLANE = 3,
};
enum {
    H264_CHROMA_DC = 0,
    H264_CHROMA_HORIZONTAL = 1,
    H264_CHROMA_VERTICAL = 2,
    H264_CHROMA_PLANE = 3,
};

/* ================================================================== */
/* 数值表（上游 h264_tables.h / h264_transform.h / h264_deblock.h，     */
/* 数值与 ffmpeg libavcodec 交叉核对过）                                */
/* ================================================================== */

/* 4x4 zig-zag 扫描：scanIndex -> raster（Fig 8-8 / Table 8-13） */
static const uint8_t kZigZag4x4[16] = {
    0, 1, 4, 8, 5, 2, 3, 6, 9, 12, 13, 10, 7, 11, 14, 15,
};

/* luma4x4BlkIdx(0..15) -> 宏块内 4x4 块网格坐标（clause 6.4.3） */
static const uint8_t kBlk4x4X[16] = {0, 1, 0, 1, 2, 3, 2, 3,
                                     0, 1, 0, 1, 2, 3, 2, 3};
static const uint8_t kBlk4x4Y[16] = {0, 0, 1, 1, 0, 0, 1, 1,
                                     2, 2, 3, 3, 2, 2, 3, 3};

/* (row,col) -> luma4x4BlkIdx，kBlk4x4X/Y 的逆映射 */
static const uint8_t kInvBlk4x4[4][4] = {
    {0, 1, 4, 5},
    {2, 3, 6, 7},
    {8, 9, 12, 13},
    {10, 11, 14, 15},
};

/* coded_block_pattern: me(v) codeNum(0..47) -> (Chroma<<4)|Luma，4:2:0，
 * Table 9-4。Intra_4x4/8x8 与 其它(Inter) 两张表分开。 */
static const uint8_t kCbpIntra4x4[48] = {
    47, 31, 15, 0,  23, 27, 29, 30, 7,  11, 13, 14, 39, 43, 45, 46,
    16, 3,  5,  10, 12, 19, 21, 26, 28, 35, 37, 42, 44, 1,  2,  4,
    8,  17, 18, 20, 24, 6,  9,  22, 25, 32, 33, 34, 36, 40, 38, 41,
};
static const uint8_t kCbpInter[48] = {
    0,  16, 1,  2,  4,  8,  32, 3,  5,  10, 12, 15, 47, 7,  11, 13,
    14, 6,  9,  31, 35, 37, 42, 44, 33, 34, 36, 40, 39, 43, 45, 46,
    17, 18, 20, 24, 19, 21, 26, 28, 23, 27, 29, 30, 22, 25, 38, 41,
};

/* normAdjust4x4(qP%6, class)，class=(row%2)+(col%2)（clause 8.5.9，
 * 平坦 weightScale=16，Baseline 唯一可能的情况） */
static const uint8_t kNormAdjust4x4[6][3] = {
    {10, 13, 16}, {11, 14, 18}, {13, 16, 20},
    {14, 18, 23}, {16, 20, 25}, {18, 23, 29},
};

/* Table 8-15：QPI -> QPC（8bit） */
static const uint8_t kChromaQpTable[52] = {
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,
    16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 29, 30,
    31, 32, 32, 33, 34, 34, 35, 35, 36, 36, 37, 37, 37, 38, 38, 38,
    39, 39, 39, 39,
};

/* 去块 alpha/beta（Table 8-16），indexA/B = Clip3(0,51,qPav+Offset) */
static const uint8_t kDeblockAlpha[52] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    4,  4,  5,  6,  7,  8,  9,  10, 12, 13, 15, 17, 20, 22, 25, 28,
    32, 36, 40, 45, 50, 56, 63, 71, 80, 90, 101, 113, 127, 144, 162, 182,
    203, 226, 255, 255,
};
static const uint8_t kDeblockBeta[52] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    2,  2,  2,  3,  3,  3,  3,  4,  4,  4,  6,  6,  7,  7,  8,  8,
    9,  9,  10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 16, 16,
    17, 17, 18, 18,
};
/* tC0（Table 8-17），[indexA][bS]，bS 0..3（bS==4 走强滤波分支） */
static const int8_t kDeblockTc0[52][4] = {
    {-1, 0, 0, 0}, {-1, 0, 0, 0}, {-1, 0, 0, 0}, {-1, 0, 0, 0},
    {-1, 0, 0, 0}, {-1, 0, 0, 0}, {-1, 0, 0, 0}, {-1, 0, 0, 0},
    {-1, 0, 0, 0}, {-1, 0, 0, 0}, {-1, 0, 0, 0}, {-1, 0, 0, 0},
    {-1, 0, 0, 0}, {-1, 0, 0, 0}, {-1, 0, 0, 0}, {-1, 0, 0, 0},
    {-1, 0, 0, 0}, {-1, 0, 0, 1}, {-1, 0, 0, 1}, {-1, 0, 0, 1},
    {-1, 0, 0, 1}, {-1, 0, 1, 1}, {-1, 0, 1, 1}, {-1, 1, 1, 1},
    {-1, 1, 1, 1}, {-1, 1, 1, 1}, {-1, 1, 1, 1}, {-1, 1, 1, 2},
    {-1, 1, 1, 2}, {-1, 1, 1, 2}, {-1, 1, 1, 2}, {-1, 1, 2, 3},
    {-1, 1, 2, 3}, {-1, 2, 2, 3}, {-1, 2, 2, 4}, {-1, 2, 3, 4},
    {-1, 2, 3, 4}, {-1, 3, 3, 5}, {-1, 3, 4, 6}, {-1, 3, 4, 6},
    {-1, 4, 5, 7}, {-1, 4, 5, 8}, {-1, 4, 6, 9}, {-1, 5, 7, 10},
    {-1, 6, 8, 11}, {-1, 6, 8, 13}, {-1, 7, 10, 14}, {-1, 8, 11, 16},
    {-1, 9, 12, 18}, {-1, 10, 13, 20}, {-1, 11, 15, 23}, {-1, 13, 17, 25},
};

/* ================================================================== */
/* CAVLC VLC 表（上游 h264_cavlc_tables.h，数值转录自 ffmpeg，保持      */
/* {len,bits} 对布局供通用前缀码匹配器使用）                            */
/* ================================================================== */

/* coeff_token，[table][TotalCoeff*4 + TrailingOnes]，table 0..2 由 nC
 * 选择（nC>=8 是 6bit 定长码，不走此表；表 3 仅自检用） */
static const uint8_t kCoeffTokenLen[4][4 * 17] = {
    {
        1, 0, 0, 0,
        6, 2, 0, 0,     8, 6, 3, 0,     9, 8, 7, 5,    10, 9, 8, 6,
       11,10, 9, 7,    13,11,10, 8,    13,13,11, 9,    13,13,13,10,
       14,14,13,11,    14,14,14,13,    15,15,14,14,    15,15,15,14,
       16,15,15,15,    16,16,16,15,    16,16,16,16,    16,16,16,16,
    },
    {
        2, 0, 0, 0,
        6, 2, 0, 0,     6, 5, 3, 0,     7, 6, 6, 4,     8, 6, 6, 4,
        8, 7, 7, 5,     9, 8, 8, 6,    11, 9, 9, 6,    11,11,11, 7,
       12,11,11, 9,    12,12,12,11,    12,12,12,11,    13,13,13,12,
       13,13,13,13,    13,14,13,13,    14,14,14,13,    14,14,14,14,
    },
    {
        4, 0, 0, 0,
        6, 4, 0, 0,     6, 5, 4, 0,     6, 5, 5, 4,     7, 5, 5, 4,
        7, 5, 5, 4,     7, 6, 6, 4,     7, 6, 6, 4,     8, 7, 7, 5,
        8, 8, 7, 6,     9, 8, 8, 7,     9, 9, 8, 8,     9, 9, 9, 8,
       10, 9, 9, 9,    10,10,10,10,    10,10,10,10,    10,10,10,10,
    },
    {
        6, 0, 0, 0,
        6, 6, 0, 0,     6, 6, 6, 0,     6, 6, 6, 6,     6, 6, 6, 6,
        6, 6, 6, 6,     6, 6, 6, 6,     6, 6, 6, 6,     6, 6, 6, 6,
        6, 6, 6, 6,     6, 6, 6, 6,     6, 6, 6, 6,     6, 6, 6, 6,
        6, 6, 6, 6,     6, 6, 6, 6,     6, 6, 6, 6,     6, 6, 6, 6,
    }
};

static const uint8_t kCoeffTokenBits[4][4 * 17] = {
    {
        1, 0, 0, 0,
        5, 1, 0, 0,     7, 4, 1, 0,     7, 6, 5, 3,     7, 6, 5, 3,
        7, 6, 5, 4,    15, 6, 5, 4,    11,14, 5, 4,     8,10,13, 4,
       15,14, 9, 4,    11,10,13,12,    15,14, 9,12,    11,10,13, 8,
       15, 1, 9,12,    11,14,13, 8,     7,10, 9,12,     4, 6, 5, 8,
    },
    {
        3, 0, 0, 0,
       11, 2, 0, 0,     7, 7, 3, 0,     7,10, 9, 5,     7, 6, 5, 4,
        4, 6, 5, 6,     7, 6, 5, 8,    15, 6, 5, 4,    11,14,13, 4,
       15,10, 9, 4,    11,14,13,12,     8,10, 9, 8,    15,14,13,12,
       11,10, 9,12,     7,11, 6, 8,     9, 8,10, 1,     7, 6, 5, 4,
    },
    {
       15, 0, 0, 0,
       15,14, 0, 0,    11,15,13, 0,     8,12,14,12,    15,10,11,11,
       11, 8, 9,10,     9,14,13, 9,     8,10, 9, 8,    15,14,13,13,
       11,14,10,12,    15,10,13,12,    11,14, 9,12,     8,10,13, 8,
       13, 7, 9,12,     9,12,11,10,     5, 8, 7, 6,     1, 4, 3, 2,
    },
    {
        3, 0, 0, 0,
        0, 1, 0, 0,     4, 5, 6, 0,     8, 9,10,11,    12,13,14,15,
       16,17,18,19,    20,21,22,23,    24,25,26,27,    28,29,30,31,
       32,33,34,35,    36,37,38,39,    40,41,42,43,    44,45,46,47,
       48,49,50,51,    52,53,54,55,    56,57,58,59,    60,61,62,63,
    }
};

/* 色度 DC(2x2, 4:2:0) coeff_token，[TotalCoeff*4 + TrailingOnes] */
static const uint8_t kChromaDcCoeffTokenLen[4 * 5] = {
    2, 0, 0, 0,
    6, 1, 0, 0,
    6, 6, 3, 0,
    6, 7, 7, 6,
    6, 8, 8, 7,
};
static const uint8_t kChromaDcCoeffTokenBits[4 * 5] = {
    1, 0, 0, 0,
    7, 1, 0, 0,
    4, 6, 1, 0,
    3, 3, 2, 5,
    2, 3, 2, 0,
};

/* total_zeros（Table 9-7/9-8），行 = TotalCoeff-1（1..15）。
 * 行 i 有 (16-i) 个有效项（total_zeros = 0..15-i）。 */
static const uint8_t kTotalZerosLen[15][16] = {
    {1,3,3,4,4,5,5,6,6,7,7,8,8,9,9,9},
    {3,3,3,3,3,4,4,4,4,5,5,6,6,6,6},
    {4,3,3,3,4,4,3,3,4,5,5,6,5,6},
    {5,3,4,4,3,3,3,4,3,4,5,5,5},
    {4,4,4,3,3,3,3,3,4,5,4,5},
    {6,5,3,3,3,3,3,3,4,3,6},
    {6,5,3,3,3,2,3,4,3,6},
    {6,4,5,3,2,2,3,3,6},
    {6,6,4,2,2,3,2,5},
    {5,5,3,2,2,2,4},
    {4,4,3,3,1,3},
    {4,4,2,1,3},
    {3,3,1,2},
    {2,2,1},
    {1,1},
};
static const uint8_t kTotalZerosBits[15][16] = {
    {1,3,2,3,2,3,2,3,2,3,2,3,2,3,2,1},
    {7,6,5,4,3,5,4,3,2,3,2,3,2,1,0},
    {5,7,6,5,4,3,4,3,2,3,2,1,1,0},
    {3,7,5,4,6,5,4,3,3,2,2,1,0},
    {5,4,3,7,6,5,4,3,2,1,1,0},
    {1,1,7,6,5,4,3,2,1,1,0},
    {1,1,5,4,3,3,2,1,1,0},
    {1,1,1,3,3,2,2,1,0},
    {1,0,1,3,2,1,1,1},
    {1,0,1,3,2,1,1},
    {0,1,1,2,1,3},
    {0,1,1,1,1},
    {0,1,1,1},
    {0,1,1},
    {0,1},
};

/* 色度 DC total_zeros（Table 9-9a），行 = TotalCoeff-1（1..3） */
static const uint8_t kChromaDcTotalZerosLen[3][4] = {
    {1, 2, 3, 3},
    {1, 2, 2, 0},
    {1, 1, 0, 0},
};
static const uint8_t kChromaDcTotalZerosBits[3][4] = {
    {1, 1, 1, 0},
    {1, 1, 0, 0},
    {1, 0, 0, 0},
};

/* run_before（Table 9-10）。行 0..5 = zerosLeft 1..6（行 i 有 i+2 个有效
 * 项），行 6 是 ">6" 表（15 个有效项）。 */
static const uint8_t kRunBeforeLen[7][16] = {
    {1,1},
    {1,2,2},
    {2,2,2,2},
    {2,2,2,3,3},
    {2,2,3,3,3,3},
    {2,3,3,3,3,3,3},
    {3,3,3,3,3,3,3,4,5,6,7,8,9,10,11},
};
static const uint8_t kRunBeforeBits[7][16] = {
    {1,0},
    {1,1,0},
    {3,2,1,0},
    {3,2,1,1,0},
    {3,2,3,2,1,0},
    {3,0,1,3,2,5,4},
    {7,6,5,4,3,2,1,1,1,1,1,1,1,1,1},
};

/* nC -> coeff_token 表选择（clause 9.2.1；nC>=8 由调用方走定长码） */
static int nC_to_coeff_token_table(int nC) {
    if (nC < 2) return 0;
    if (nC < 4) return 1;
    if (nC < 8) return 2;
    return 3;
}

/* ================================================================== */
/* 比特读取器（上游 h264_bitreader.h）：MSB-first，RBSP 上的           */
/* u(n)/ue(v)/se(v)/byte_align/more_rbsp_data                          */
/* ================================================================== */

typedef struct {
    const uint8_t *data;
    size_t size;
    size_t byte_pos;
    int bit_pos;      /* 当前字节内下一个要读的位，0 = MSB */
    int error;
    long stop_bit_index; /* 惰性缓存，more_rbsp_data 首次调用时计算 */
} h264_br_t;

static void br_init(h264_br_t *br, const uint8_t *data, size_t size) {
    br->data = data;
    br->size = size;
    br->byte_pos = 0;
    br->bit_pos = 0;
    br->error = 0;
    br->stop_bit_index = -1;
}

static uint32_t br_read_bit(h264_br_t *br) {
    if (br->byte_pos >= br->size) {
        br->error = 1;
        return 0;
    }
    uint32_t bit = (br->data[br->byte_pos] >> (7 - br->bit_pos)) & 1u;
    if (++br->bit_pos == 8) {
        br->bit_pos = 0;
        br->byte_pos++;
    }
    return bit;
}

/* u(n)：MSB 优先读 n 位（0 <= n <= 32） */
static uint32_t br_u(h264_br_t *br, int n) {
    uint32_t val = 0;
    while (n-- > 0) {
        val = (val << 1) | br_read_bit(br);
    }
    return val;
}

static int br_flag(h264_br_t *br) { return br_u(br, 1) != 0; }

/* ue(v)：无符号 Exp-Golomb（clause 9.1） */
static uint32_t br_ue(h264_br_t *br) {
    int zeros = 0;
    while (zeros < 32) {
        if (br_read_bit(br) != 0) break;
        zeros++;
        if (br->error) break;
    }
    if (zeros >= 32 || br->error) {
        br->error = 1;
        return 0;
    }
    uint32_t info = zeros > 0 ? br_u(br, zeros) : 0;
    return ((1u << zeros) - 1) + info;
}

/* se(v)：有符号 Exp-Golomb（clause 9.1.1） */
static int32_t br_se(h264_br_t *br) {
    uint32_t k = br_ue(br);
    int32_t v = (int32_t)((k + 1) >> 1);
    return (k & 1) ? v : -v;
}

static void br_byte_align(h264_br_t *br) {
    if (br->bit_pos != 0) {
        br->bit_pos = 0;
        br->byte_pos++;
    }
}

/* rbsp_stop_one_bit 的位偏移（缓冲内最后一个 '1' 位）。全 0 返回 -1。 */
static size_t br_find_stop_bit(const h264_br_t *br) {
    size_t last = br->size;
    while (last > 0 && br->data[last - 1] == 0) last--;
    if (last == 0) return (size_t)-1;
    uint8_t b = br->data[last - 1];
    int lsb_pos = 0;
    for (int i = 0; i < 8; i++) {
        if (b & (1u << i)) {
            lsb_pos = i;
            break;
        }
    }
    return (last - 1) * 8 + (7 - lsb_pos);
}

/* more_rbsp_data()（clause 7.2）：rbsp_trailing_bits 之前是否还有语法元素 */
static int br_more_rbsp_data(h264_br_t *br) {
    size_t cur_bit = br->byte_pos * 8 + (size_t)br->bit_pos;
    if (cur_bit >= br->size * 8) return 0;
    if (br->stop_bit_index < 0) br->stop_bit_index = (long)br_find_stop_bit(br);
    if (br->stop_bit_index < 0) return 0;
    return (long)cur_bit < br->stop_bit_index;
}

/* ================================================================== */
/* NAL 解复用（上游 h264_nal.h）：Annex-B start code 扫描 +             */
/* emulation prevention 剥离                                           */
/* ================================================================== */

typedef struct {
    uint8_t type;      /* nal_unit_type，5 位 */
    uint8_t ref_idc;   /* nal_ref_idc，2 位 */
    const uint8_t *rbsp; /* 剥离 EP 后的载荷（指向 scratch） */
    size_t rbsp_size;
} h264_nal_t;

/* 在 buf[pos..) 找下一个 00 00 01 start code。返回 NAL 头字节的偏移，
 * 没有更多 start code 返回 (size_t)-1。*nal_end 收 NAL 数据结束偏移。 */
static size_t nal_find_next_start_code(const uint8_t *buf, size_t size,
                                       size_t pos, size_t *nal_end) {
    size_t i = pos;
    while (i + 2 < size) {
        if (buf[i] == 0 && buf[i + 1] == 0 && buf[i + 2] == 1) {
            size_t nal_start = i + 3;
            size_t j = nal_start;
            while (j + 2 < size) {
                if (buf[j] == 0 && buf[j + 1] == 0 && buf[j + 2] == 1) break;
                j++;
            }
            size_t end = (j + 2 < size) ? j : size;
            /* 尾部属于下一个 start code 前导 0 的 0 字节剪掉 */
            while (end > nal_start && buf[end - 1] == 0) end--;
            *nal_end = end;
            return nal_start;
        }
        i++;
    }
    return (size_t)-1;
}

/* 剥离 emulation prevention（00 00 03 中的 03），dst 可与 src 前部别名。
 * 返回写入 dst 的字节数。 */
static size_t nal_strip_ep(const uint8_t *src, size_t src_size,
                           uint8_t *dst, size_t dst_cap) {
    size_t o = 0;
    int zero_run = 0;
    for (size_t i = 0; i < src_size && o < dst_cap; i++) {
        uint8_t b = src[i];
        if (zero_run >= 2 && b == 0x03) {
            /* 丢弃该字节；03 之后的字节是真实数据，可能自身开启新 0 串 */
            zero_run = 0;
            continue;
        }
        dst[o++] = b;
        zero_run = (b == 0) ? zero_run + 1 : 0;
    }
    return o;
}

typedef struct {
    uint8_t *scratch;
    size_t scratch_cap;
    const uint8_t *buf;
    size_t size;
    size_t pos;
} h264_nal_rd_t;

static void nal_rd_reset(h264_nal_rd_t *rd, const uint8_t *buf, size_t size) {
    rd->buf = buf;
    rd->size = size;
    rd->pos = 0;
}

static int nal_rd_next(h264_nal_rd_t *rd, h264_nal_t *nal) {
    size_t nal_end;
    size_t start = nal_find_next_start_code(rd->buf, rd->size, rd->pos, &nal_end);
    if (start == (size_t)-1 || start >= nal_end) return 0;
    rd->pos = nal_end;

    uint8_t header = rd->buf[start];
    nal->ref_idc = (header >> 5) & 0x3;
    nal->type = header & 0x1F;

    size_t payload_size = nal_end - (start + 1);
    size_t written = nal_strip_ep(rd->buf + start + 1, payload_size,
                                  rd->scratch, rd->scratch_cap);
    nal->rbsp = rd->scratch;
    nal->rbsp_size = written;
    return 1;
}

/* ================================================================== */
/* 帧（上游 h264_frame.h）：YUV420 三平面，按 max_w x max_h 一次分配    */
/* ================================================================== */

typedef struct {
    uint8_t *y;
    uint8_t *u;
    uint8_t *v;
    int max_w, max_h;    /* 分配上限 */
    int width, height;   /* 编码尺寸（宏块对齐） */
    int stride_y, stride_c;
    uint32_t frame_num;
    int is_ref;
} h264_frame_t;

static void frame_init(h264_frame_t *f, int max_w, int max_h) {
    memset(f, 0, sizeof(*f));
    f->max_w = max_w;
    f->max_h = max_h;
}

/* 释放三平面并复位尺寸（保留分配上限），失败回滚也走这里：
 * 不允许半分配状态残留 */
static void frame_release(h264_frame_t *f) {
    if (f->y) H264_DEC_FREE(f->y);
    if (f->u) H264_DEC_FREE(f->u);
    if (f->v) H264_DEC_FREE(f->v);
    frame_init(f, f->max_w, f->max_h);
}

static int frame_ensure_alloc(h264_frame_t *f) {
    if (f->y) return 1;
    f->y = (uint8_t *)H264_DEC_ALLOC((size_t)f->max_w * f->max_h);
    if (!f->y) return 0;
    f->u = (uint8_t *)H264_DEC_ALLOC((size_t)(f->max_w / 2) * (f->max_h / 2));
    f->v = (uint8_t *)H264_DEC_ALLOC((size_t)(f->max_w / 2) * (f->max_h / 2));
    if (!f->u || !f->v) {
        frame_release(f);
        return 0;
    }
    return 1;
}

static void frame_set_max_dim(h264_frame_t *f, int w, int h) {
    if (f->y) frame_release(f);
    f->max_w = w;
    f->max_h = h;
}

static int frame_set_size(h264_frame_t *f, int w, int h) {
    if (!frame_ensure_alloc(f)) return 0;
    f->width = w;
    f->height = h;
    f->stride_y = w;
    f->stride_c = w / 2;
    return 1;
}

static uint8_t *frame_y_row(h264_frame_t *f, int row) {
    return f->y + (size_t)row * f->stride_y;
}
static uint8_t *frame_u_row(h264_frame_t *f, int row) {
    return f->u + (size_t)row * f->stride_c;
}
static uint8_t *frame_v_row(h264_frame_t *f, int row) {
    return f->v + (size_t)row * f->stride_c;
}
static const uint8_t *frame_y_row_c(const h264_frame_t *f, int row) {
    return f->y + (size_t)row * f->stride_y;
}

static int frame_copy_from(h264_frame_t *f, const h264_frame_t *other) {
    if (!frame_ensure_alloc(f)) return 0;
    f->width = other->width;
    f->height = other->height;
    f->stride_y = other->stride_y;
    f->stride_c = other->stride_c;
    f->frame_num = other->frame_num;
    f->is_ref = other->is_ref;
    memcpy(f->y, other->y, (size_t)other->stride_y * other->height);
    memcpy(f->u, other->u, (size_t)other->stride_c * (other->height / 2));
    memcpy(f->v, other->v, (size_t)other->stride_c * (other->height / 2));
    return 1;
}

/* O(1) 指针交换（滑动窗移位用，等价上游 std::swap(Frame, Frame)） */
static void frame_swap(h264_frame_t *a, h264_frame_t *b) {
    h264_frame_t t = *a;
    *a = *b;
    *b = t;
}

/* ================================================================== */
/* 宏块元数据（上游 h264_mb_info.h）：逐宏块解码状态 + slice 归属       */
/* ================================================================== */

typedef struct {
    uint8_t type;                       /* H264_MBT_* */
    uint8_t intra4x4_pred_mode[16];     /* 仅 INTRA4X4 有效 */
    uint8_t intra16x16_pred_mode;
    uint8_t chroma_pred_mode;
    uint8_t cbp_luma;                   /* 4 位，每个 8x8 亮度象限一位 */
    uint8_t cbp_chroma;                 /* 0..2 */
    int8_t qp_y;
    uint8_t disable_deblock_idc;        /* 0=正常 1=全关 2=仅关 slice 边界 */
    int8_t alpha_c0_off_div2;
    int8_t beta_off_div2;
    /* 非零系数计数（CAVLC nC 预测用）：0..15 亮度 4x4，16..19 Cb，20..23 Cr */
    uint8_t nnz[24];
    /* 1/4 亮度采样为单位的 MV，每 4x4 块一组（分区填充展开到 16 格） */
    int16_t mv[16][2];
    /* 每 4x4 块的 ref_idx_l0（不可用/Intra 为 -1） */
    int8_t ref_idx[16];
} h264_mbinfo_t;

static int mbinfo_is_intra(const h264_mbinfo_t *mb) {
    return mb->type == H264_MBT_INTRA4X4 || mb->type == H264_MBT_INTRA16X16 ||
           mb->type == H264_MBT_INTRA_PCM;
}

typedef struct {
    int mb_w, mb_h;
    int max_mbs;
    int16_t *slice_id;   /* -1 = 尚未解码 */
    h264_mbinfo_t *mb;
} h264_mbtab_t;

static void mbtab_init(h264_mbtab_t *t, int max_w, int max_h) {
    memset(t, 0, sizeof(*t));
    t->max_mbs = ((max_w + 15) / 16) * ((max_h + 15) / 16);
}

static void mbtab_release(h264_mbtab_t *t) {
    if (t->slice_id) H264_DEC_FREE(t->slice_id);
    if (t->mb) H264_DEC_FREE(t->mb);
    int max_mbs = t->max_mbs;
    memset(t, 0, sizeof(*t));
    t->max_mbs = max_mbs;
}

static void mbtab_set_max_dim(h264_mbtab_t *t, int max_w, int max_h) {
    if (t->slice_id) mbtab_release(t);
    t->max_mbs = ((max_w + 15) / 16) * ((max_h + 15) / 16);
}

static int mbtab_reset(h264_mbtab_t *t, int mb_w, int mb_h) {
    if (!t->slice_id) {
        t->slice_id = (int16_t *)H264_DEC_ALLOC((size_t)t->max_mbs * sizeof(int16_t));
        t->mb = (h264_mbinfo_t *)H264_DEC_ALLOC((size_t)t->max_mbs * sizeof(h264_mbinfo_t));
        if (!t->slice_id || !t->mb) {
            mbtab_release(t);
            return 0;
        }
    }
    t->mb_w = mb_w;
    t->mb_h = mb_h;
    int n = mb_w * mb_h;
    for (int i = 0; i < n; i++) {
        t->slice_id[i] = -1;
        memset(&t->mb[i], 0, sizeof(t->mb[i]));
    }
    return 1;
}

static int mbtab_addr(const h264_mbtab_t *t, int mb_x, int mb_y) {
    return mb_y * t->mb_w + mb_x;
}

static h264_mbinfo_t *mbtab_at(h264_mbtab_t *t, int mb_x, int mb_y) {
    return &t->mb[mbtab_addr(t, mb_x, mb_y)];
}
static const h264_mbinfo_t *mbtab_at_c(const h264_mbtab_t *t, int mb_x,
                                       int mb_y) {
    return &t->mb[mbtab_addr(t, mb_x, mb_y)];
}

static void mbtab_begin_mb(h264_mbtab_t *t, int mb_x, int mb_y, int slice_id) {
    t->slice_id[mbtab_addr(t, mb_x, mb_y)] = (int16_t)slice_id;
}

static int mbtab_slice_id_at(const h264_mbtab_t *t, int mb_x, int mb_y) {
    if (mb_x < 0 || mb_y < 0 || mb_x >= t->mb_w || mb_y >= t->mb_h) return -1;
    return t->slice_id[mbtab_addr(t, mb_x, mb_y)];
}

/* 邻居可用 = 已按光栅序解码过且属于同一 slice（无 MBAFF/FMO 的化简） */
static int mbtab_decoded(const h264_mbtab_t *t, int mb_x, int mb_y,
                         int slice_id) {
    if (mb_x < 0 || mb_y < 0 || mb_x >= t->mb_w || mb_y >= t->mb_h) return 0;
    return t->slice_id[mbtab_addr(t, mb_x, mb_y)] == slice_id;
}

static int mbtab_left_available(const h264_mbtab_t *t, int x, int y, int sid) {
    return mbtab_decoded(t, x - 1, y, sid);
}
static int mbtab_top_available(const h264_mbtab_t *t, int x, int y, int sid) {
    return mbtab_decoded(t, x, y - 1, sid);
}
static int mbtab_top_left_available(const h264_mbtab_t *t, int x, int y,
                                    int sid) {
    return mbtab_decoded(t, x - 1, y - 1, sid);
}
static int mbtab_top_right_available(const h264_mbtab_t *t, int x, int y,
                                     int sid) {
    return mbtab_decoded(t, x + 1, y - 1, sid);
}

/* nC 预测（clause 9.2.1）：-1 表示邻居不可用 */
static int predict_nc(int nnz_left, int nnz_top) {
    int have_left = nnz_left >= 0;
    int have_top = nnz_top >= 0;
    if (have_left && have_top) return (nnz_left + nnz_top + 1) >> 1;
    if (have_left) return nnz_left;
    if (have_top) return nnz_top;
    return 0;
}

/* ================================================================== */
/* SPS/PPS（上游 h264_sps_pps.h）                                      */
/* ================================================================== */

typedef struct {
    int valid;
    int unsupported;   /* 解析成功但描述了不支持的码流 */

    uint8_t profile_idc;
    uint8_t level_idc;
    uint32_t id;

    uint32_t log2_max_frame_num_minus4;
    uint32_t pic_order_cnt_type;
    uint32_t log2_max_poc_lsb_minus4;
    int delta_pic_order_always_zero;
    int32_t offset_for_non_ref_pic;
    int32_t offset_for_top_to_bottom;
    uint32_t num_ref_in_poc_cycle;
    int32_t offset_for_ref_frame[H264_MAX_POC_CYCLE];

    uint32_t max_num_ref_frames;
    int gaps_in_frame_num_allowed;

    uint32_t pic_width_in_mbs_minus1;
    uint32_t pic_height_in_map_units_minus1;
    int frame_mbs_only;
    int mb_adaptive_frame_field;
    int direct8x8_inference;

    int frame_cropping;
    uint32_t crop_left, crop_right, crop_top, crop_bottom;

    /* VUI timing_info（声明帧率用） */
    int timing_info_present;
    uint32_t num_units_in_tick;
    uint32_t time_scale;

    /* 派生（finalize 填充） */
    uint32_t pic_width_in_mbs;
    uint32_t pic_height_in_mbs;
    uint32_t coded_width, coded_height;    /* 宏块对齐 */
    uint32_t display_width, display_height; /* 裁剪后 */
} h264_sps_t;

typedef struct {
    int valid;
    int unsupported;
    uint32_t id;
    uint32_t sps_id;
    int entropy_coding_mode;        /* 0 = CAVLC（唯一支持） */
    int bottom_field_pic_order_in_frame_present;
    uint32_t num_slice_groups_minus1; /* >0 = FMO，不支持 */
    uint32_t num_ref_idx_l0_default_active_minus1;
    uint32_t num_ref_idx_l1_default_active_minus1;
    int weighted_pred;
    uint8_t weighted_bipred_idc;
    int32_t pic_init_qp_minus26;
    int32_t pic_init_qs_minus26;
    int32_t chroma_qp_index_offset;
    int deblocking_filter_control_present;
    int constrained_intra_pred;
    int redundant_pic_cnt_present;
    int transform8x8_mode;
    int32_t second_chroma_qp_index_offset;
} h264_pps_t;

/* profile_idc 需要读 High profile 色度扩展字段的取值（spec 7.3.2.1.1） */
static int sps_has_chroma_extension(uint8_t profile_idc) {
    switch (profile_idc) {
        case 100: case 110: case 122: case 244: case 44:
        case 83: case 86: case 118: case 128: case 138:
        case 139: case 134: case 135:
            return 1;
        default:
            return 0;
    }
}

static void sps_finalize(h264_sps_t *sps) {
    sps->pic_width_in_mbs = sps->pic_width_in_mbs_minus1 + 1;
    /* frame_mbs_only 必为 1（否则 parseSps 已判 unsupported） */
    sps->pic_height_in_mbs = sps->pic_height_in_map_units_minus1 + 1;
    sps->coded_width = sps->pic_width_in_mbs * H264_MB_SIZE;
    sps->coded_height = sps->pic_height_in_mbs * H264_MB_SIZE;

    /* 4:2:0：CropUnitX=2，CropUnitY=2*(2-frame_mbs_only)=2 */
    uint32_t crop_unit_x = 2, crop_unit_y = 2;
    sps->display_width = sps->coded_width;
    sps->display_height = sps->coded_height;
    if (sps->frame_cropping) {
        uint32_t cw = crop_unit_x * (sps->crop_left + sps->crop_right);
        uint32_t ch = crop_unit_y * (sps->crop_top + sps->crop_bottom);
        sps->display_width = (cw < sps->coded_width) ? sps->coded_width - cw : 0;
        sps->display_height = (ch < sps->coded_height) ? sps->coded_height - ch : 0;
    }
}

/* vui_parameters() 前段走到 timing_info 为止（字段顺序与 ffmpeg
 * ff_h2645_decode_common_vui_params 交叉核对） */
static void parse_vui_timing_info(h264_br_t *br, h264_sps_t *sps) {
    if (br_flag(br)) {  /* aspect_ratio_info_present_flag */
        uint32_t ar_idc = br_u(br, 8);
        if (ar_idc == 255) {
            br_u(br, 16);  /* sar_width */
            br_u(br, 16);  /* sar_height */
        }
    }
    if (br_flag(br)) {  /* overscan_info_present_flag */
        br_flag(br);    /* overscan_appropriate_flag */
    }
    if (br_flag(br)) {  /* video_signal_type_present_flag */
        br_u(br, 3);    /* video_format */
        br_flag(br);    /* video_full_range_flag */
        if (br_flag(br)) { /* colour_description_present_flag */
            br_u(br, 8);
            br_u(br, 8);
            br_u(br, 8);
        }
    }
    if (br_flag(br)) {  /* chroma_loc_info_present_flag */
        br_ue(br);
        br_ue(br);
    }

    sps->timing_info_present = br_flag(br);
    if (sps->timing_info_present) {
        sps->num_units_in_tick = br_u(br, 32);
        sps->time_scale = br_u(br, 32);
        br_flag(br);  /* fixed_frame_rate_flag */
        if (sps->num_units_in_tick == 0 || sps->time_scale == 0) {
            /* spec 非法（会除零），按不存在处理（与 ffmpeg 同样的宽容） */
            sps->timing_info_present = 0;
        }
    }
}

/* 返回 1 且 sps->unsupported=1 表示"解析出结构合法但不支持的特性"；
 * valid=1 才是真正可用。 */
static int parse_sps(h264_br_t *br, h264_sps_t *sps) {
    memset(sps, 0, sizeof(*sps));

    sps->profile_idc = (uint8_t)br_u(br, 8);
    br_u(br, 8);  /* constraint_set0..5 + reserved_zero_2bits */
    sps->level_idc = (uint8_t)br_u(br, 8);
    sps->id = br_ue(br);

    if (sps_has_chroma_extension(sps->profile_idc)) {
        uint32_t chroma_format_idc = br_ue(br);
        if (chroma_format_idc == 3) br_u(br, 1); /* separate_colour_plane */
        br_ue(br);  /* bit_depth_luma_minus8 */
        br_ue(br);  /* bit_depth_chroma_minus8 */
        br_u(br, 1); /* qpprime_y_zero_transform_bypass */
        int scaling = br_flag(br);
        if (scaling || chroma_format_idc != 1) {
            /* High profile scaling list / 非 4:2:0 */
            H264_LOG("SPS: scaling lists / non-4:2:0 not supported\n");
            sps->unsupported = 1;
            return 1;
        }
    }

    sps->log2_max_frame_num_minus4 = br_ue(br);
    sps->pic_order_cnt_type = br_ue(br);
    if (sps->pic_order_cnt_type == 0) {
        sps->log2_max_poc_lsb_minus4 = br_ue(br);
    } else if (sps->pic_order_cnt_type == 1) {
        sps->delta_pic_order_always_zero = br_flag(br);
        sps->offset_for_non_ref_pic = br_se(br);
        sps->offset_for_top_to_bottom = br_se(br);
        sps->num_ref_in_poc_cycle = br_ue(br);
        if (sps->num_ref_in_poc_cycle > H264_MAX_POC_CYCLE) {
            H264_LOG("SPS: poc cycle %u too long\n", sps->num_ref_in_poc_cycle);
            sps->unsupported = 1;
            return 1;
        }
        for (uint32_t i = 0; i < sps->num_ref_in_poc_cycle; i++) {
            sps->offset_for_ref_frame[i] = br_se(br);
        }
    }
    /* pic_order_cnt_type == 2：无额外字段 */

    sps->max_num_ref_frames = br_ue(br);
    sps->gaps_in_frame_num_allowed = br_flag(br);
    sps->pic_width_in_mbs_minus1 = br_ue(br);
    sps->pic_height_in_map_units_minus1 = br_ue(br);
    sps->frame_mbs_only = br_flag(br);
    if (!sps->frame_mbs_only) {
        sps->mb_adaptive_frame_field = br_flag(br);
        /* 隔行（PAFF/MBAFF）超出 Baseline-only 范围 */
        H264_LOG("SPS: interlaced (PAFF/MBAFF) not supported\n");
        sps->unsupported = 1;
        return 1;
    }
    sps->direct8x8_inference = br_flag(br);
    sps->frame_cropping = br_flag(br);
    if (sps->frame_cropping) {
        sps->crop_left = br_ue(br);
        sps->crop_right = br_ue(br);
        sps->crop_top = br_ue(br);
        sps->crop_bottom = br_ue(br);
    }
    if (br_flag(br)) { /* vui_parameters_present */
        parse_vui_timing_info(br, sps);
    }

    if (br->error) {
        H264_LOG("SPS: bitstream error/truncation\n");
        sps->unsupported = 1;
        return 1;
    }

    sps_finalize(sps);
    sps->valid = 1;
    return 1;
}

static int parse_pps(h264_br_t *br, h264_pps_t *pps) {
    memset(pps, 0, sizeof(*pps));

    pps->id = br_ue(br);
    pps->sps_id = br_ue(br);
    pps->entropy_coding_mode = br_flag(br);
    pps->bottom_field_pic_order_in_frame_present = br_flag(br);
    pps->num_slice_groups_minus1 = br_ue(br);
    if (pps->num_slice_groups_minus1 > 0) {
        /* FMO：Baseline 合法但实际编码器从不使用，标记不支持 */
        H264_LOG("PPS: FMO not supported\n");
        pps->unsupported = 1;
        return 1;
    }
    pps->num_ref_idx_l0_default_active_minus1 = br_ue(br);
    pps->num_ref_idx_l1_default_active_minus1 = br_ue(br);
    pps->weighted_pred = br_flag(br);
    pps->weighted_bipred_idc = (uint8_t)br_u(br, 2);
    pps->pic_init_qp_minus26 = br_se(br);
    pps->pic_init_qs_minus26 = br_se(br);
    pps->chroma_qp_index_offset = br_se(br);
    pps->deblocking_filter_control_present = br_flag(br);
    pps->constrained_intra_pred = br_flag(br);
    pps->redundant_pic_cnt_present = br_flag(br);

    pps->second_chroma_qp_index_offset = pps->chroma_qp_index_offset;
    if (br_more_rbsp_data(br)) {
        pps->transform8x8_mode = br_flag(br);
        int scaling = br_flag(br);
        if (scaling) {
            H264_LOG("PPS: scaling lists not supported\n");
            pps->unsupported = 1;
            return 1;
        }
        pps->second_chroma_qp_index_offset = br_se(br);
    }

    if (br->error) {
        H264_LOG("PPS: bitstream error/truncation\n");
        pps->unsupported = 1;
        return 1;
    }

    if (pps->entropy_coding_mode) {
        /* CABAC：只实现了 CAVLC */
        H264_LOG("PPS: CABAC not supported (CAVLC only)\n");
        pps->unsupported = 1;
        return 1;
    }

    pps->valid = 1;
    return 1;
}

/* ================================================================== */
/* slice_header（上游 h264_slice_header.h）                            */
/* ================================================================== */

typedef struct {
    int valid;
    int unsupported;

    uint32_t first_mb_in_slice;
    uint32_t slice_type_raw;
    uint8_t slice_type;   /* raw % 5 */
    uint32_t pps_id;
    uint32_t frame_num;
    int is_idr;
    uint32_t idr_pic_id;

    uint32_t poc_lsb;
    int32_t delta_pic_order_cnt_bottom;
    int32_t delta_pic_order_cnt[2];

    uint32_t redundant_pic_cnt;

    int num_ref_idx_active_override;
    uint32_t num_ref_idx_l0_active_minus1; /* 生效值 */

    int ref_pic_list_mod_flag_l0;

    int adaptive_ref_pic_marking;

    int32_t slice_qp_delta;
    int32_t slice_qp;   /* pps.pic_init_qp_minus26 + 26 + delta */

    uint32_t disable_deblocking_filter_idc;
    int32_t slice_alpha_c0_offset_div2;
    int32_t slice_beta_offset_div2;
} h264_sh_t;

/* ref_pic_list_modification()，仅 list0（无 B slice）。
 * 显式重排未实现——解析以保持位同步，随后上层判 unsupported。 */
static int parse_ref_pic_list_mod_l0(h264_br_t *br, h264_sh_t *sh) {
    sh->ref_pic_list_mod_flag_l0 = br_flag(br);
    if (sh->ref_pic_list_mod_flag_l0) {
        uint32_t idc;
        int guard = 0;
        do {
            idc = br_ue(br);
            if (idc == 0 || idc == 1) {
                br_ue(br);  /* abs_diff_pic_num_minus1 */
            } else if (idc == 2) {
                br_ue(br);  /* long_term_pic_num */
            }
            if (br->error || ++guard > 64) return 0;
        } while (idc != 3);
    }
    return 1;
}

/* dec_ref_pic_marking()。MMCO 自适应标记未实现（仅滑动窗），
 * 语法仍需消费以保持位同步。 */
static int parse_dec_ref_pic_marking(h264_br_t *br, h264_sh_t *sh) {
    if (sh->is_idr) {
        br_flag(br);  /* no_output_of_prior_pics_flag */
        br_flag(br);  /* long_term_reference_flag */
        return !br->error;
    }
    sh->adaptive_ref_pic_marking = br_flag(br);
    if (!sh->adaptive_ref_pic_marking) return !br->error;

    uint32_t op;
    int guard = 0;
    do {
        op = br_ue(br);
        switch (op) {
            case 1: br_ue(br); break;             /* difference_of_pic_nums_minus1 */
            case 2: br_ue(br); break;             /* long_term_pic_num */
            case 3: br_ue(br); br_ue(br); break;  /* + long_term_frame_idx */
            case 4: br_ue(br); break;             /* max_long_term_frame_idx_plus1 */
            case 5: break;                        /* reset frame numbering */
            case 6: br_ue(br); break;             /* long_term_frame_idx */
            default: break;                       /* 0 = 结束 */
        }
        if (br->error || ++guard > 64) return 0;
    } while (op != 0);
    return 1;
}

static int parse_slice_header(h264_br_t *br, uint8_t nal_type, uint8_t nal_ref_idc,
                              const h264_sps_t *sps, const h264_pps_t *pps,
                              h264_sh_t *sh) {
    (void)nal_ref_idc;
    memset(sh, 0, sizeof(*sh));
    sh->is_idr = (nal_type == H264_NAL_SLICE_IDR);

    sh->first_mb_in_slice = br_ue(br);
    sh->slice_type_raw = br_ue(br);
    sh->slice_type = (uint8_t)(sh->slice_type_raw % 5);
    sh->pps_id = br_ue(br);

    uint32_t frame_num_bits = sps->log2_max_frame_num_minus4 + 4;
    sh->frame_num = br_u(br, (int)frame_num_bits);

    /* field_pic_flag / bottom_field_flag 不存在：frame_mbs_only=0
     * 的流在 SPS 已判 unsupported */

    if (sh->is_idr) {
        sh->idr_pic_id = br_ue(br);
    }

    if (sps->pic_order_cnt_type == 0) {
        uint32_t poc_lsb_bits = sps->log2_max_poc_lsb_minus4 + 4;
        sh->poc_lsb = br_u(br, (int)poc_lsb_bits);
        if (pps->bottom_field_pic_order_in_frame_present) {
            sh->delta_pic_order_cnt_bottom = br_se(br);
        }
    } else if (sps->pic_order_cnt_type == 1 &&
               !sps->delta_pic_order_always_zero) {
        sh->delta_pic_order_cnt[0] = br_se(br);
        if (pps->bottom_field_pic_order_in_frame_present) {
            sh->delta_pic_order_cnt[1] = br_se(br);
        }
    }

    if (pps->redundant_pic_cnt_present) {
        sh->redundant_pic_cnt = br_ue(br);
    }

    if (sh->slice_type != H264_SLICE_I && sh->slice_type != H264_SLICE_SI) {
        sh->num_ref_idx_active_override = br_flag(br);
        if (sh->num_ref_idx_active_override) {
            sh->num_ref_idx_l0_active_minus1 = br_ue(br);
        } else {
            sh->num_ref_idx_l0_active_minus1 =
                pps->num_ref_idx_l0_default_active_minus1;
        }
    }

    if (sh->slice_type != H264_SLICE_I && sh->slice_type != H264_SLICE_SI) {
        if (!parse_ref_pic_list_mod_l0(br, sh)) {
            sh->unsupported = 1;
            return 1;
        }
        if (sh->ref_pic_list_mod_flag_l0) {
            /* 显式参考列表重排：保持位同步后标记不支持 */
            H264_LOG("SH: explicit ref list reordering not supported\n");
            sh->unsupported = 1;
            return 1;
        }
    }

    if (pps->weighted_pred && sh->slice_type == H264_SLICE_P) {
        /* 加权预测是 Main/High 特性，Baseline 编码器不会置位 */
        H264_LOG("SH: weighted prediction not supported\n");
        sh->unsupported = 1;
        return 1;
    }

    if (nal_ref_idc != 0) {
        if (!parse_dec_ref_pic_marking(br, sh)) {
            sh->unsupported = 1;
            return 1;
        }
        if (sh->adaptive_ref_pic_marking) {
            /* MMCO 自适应标记未实现，仅滑动窗 */
            H264_LOG("SH: adaptive (MMCO) ref marking not supported\n");
            sh->unsupported = 1;
            return 1;
        }
    }

    /* entropy_coding_mode=0（CAVLC）由 parse_pps 保证 */
    sh->slice_qp_delta = br_se(br);
    sh->slice_qp = pps->pic_init_qp_minus26 + 26 + sh->slice_qp_delta;

    if (pps->deblocking_filter_control_present) {
        sh->disable_deblocking_filter_idc = br_ue(br);
        if (sh->disable_deblocking_filter_idc != 1) {
            sh->slice_alpha_c0_offset_div2 = br_se(br);
            sh->slice_beta_offset_div2 = br_se(br);
        }
    }

    if (br->error) {
        H264_LOG("SH: bitstream error/truncation\n");
        sh->unsupported = 1;
        return 1;
    }

    if (sh->slice_type != H264_SLICE_I && sh->slice_type != H264_SLICE_P) {
        H264_LOG("SH: slice_type=%u (B/SP/SI) not supported\n",
                 sh->slice_type_raw);
        sh->unsupported = 1;
        return 1;
    }

    sh->valid = 1;
    return 1;
}

/* ================================================================== */
/* 反量化 + 反变换（上游 h264_transform.h），仅平坦 scaling list        */
/* ================================================================== */

static int chroma_qp(int luma_qp, int chroma_qp_index_offset) {
    int qpi = luma_qp + chroma_qp_index_offset;
    if (qpi < 0) qpi = 0;
    if (qpi > 51) qpi = 51;
    return kChromaQpTable[qpi];
}

static uint8_t clip255(int32_t v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (uint8_t)v;
}

/* clause 8.5.9，raster 序 16 项原地。skipDC 时位置 0 不动（I_16x16 亮度
 * AC / 色度 AC 块，DC 来自独立的 Hadamard 块） */
static void dequant4x4(int32_t *c, int qp, int skip_dc) {
    int m = qp % 6;
    int shift = qp / 6;
    for (int idx = 0; idx < 16; idx++) {
        if (skip_dc && idx == 0) continue;
        int row = idx >> 2, col = idx & 3;
        int cls = (row & 1) + (col & 1);
        int32_t scale = (int32_t)kNormAdjust4x4[m][cls] * 16;
        if (shift >= 4) {
            c[idx] = c[idx] * scale * (1 << (shift - 4));
        } else {
            c[idx] = (c[idx] * scale + (1 << (3 - shift))) >> (4 - shift);
        }
    }
}

/* 4x4 Walsh-Hadamard（I_16x16 亮度 DC，clause 8.5.10） */
static void hadamard4x4(int32_t *block) {
    int32_t tmp[16];
    for (int i = 0; i < 4; i++) {
        int32_t d0 = block[i + 0 * 4], d1 = block[i + 1 * 4];
        int32_t d2 = block[i + 2 * 4], d3 = block[i + 3 * 4];
        int32_t e0 = d0 + d2, e1 = d0 - d2, e2 = d1 - d3, e3 = d1 + d3;
        tmp[i + 0 * 4] = e0 + e3;
        tmp[i + 1 * 4] = e1 + e2;
        tmp[i + 2 * 4] = e1 - e2;
        tmp[i + 3 * 4] = e0 - e3;
    }
    for (int j = 0; j < 4; j++) {
        int32_t d0 = tmp[j * 4 + 0], d1 = tmp[j * 4 + 1];
        int32_t d2 = tmp[j * 4 + 2], d3 = tmp[j * 4 + 3];
        int32_t e0 = d0 + d2, e1 = d0 - d2, e2 = d1 - d3, e3 = d1 + d3;
        block[j * 4 + 0] = e0 + e3;
        block[j * 4 + 1] = e1 + e2;
        block[j * 4 + 2] = e1 - e2;
        block[j * 4 + 3] = e0 - e3;
    }
}

static void dequant_luma_dc4x4(int32_t *f, int qp) {
    int m = qp % 6;
    int shift = qp / 6;
    int32_t scale = (int32_t)kNormAdjust4x4[m][0] * 16;
    for (int i = 0; i < 16; i++) {
        if (shift >= 6) {
            f[i] = f[i] * scale * (1 << (shift - 6));
        } else {
            f[i] = (f[i] * scale + (1 << (5 - shift))) >> (6 - shift);
        }
    }
}

/* 2x2 Walsh-Hadamard（色度 DC，4:2:0，raster {c00,c01,c10,c11}） */
static void hadamard2x2(int32_t *f) {
    int32_t c00 = f[0], c01 = f[1], c10 = f[2], c11 = f[3];
    f[0] = c00 + c01 + c10 + c11;
    f[1] = c00 - c01 + c10 - c11;
    f[2] = c00 + c01 - c10 - c11;
    f[3] = c00 - c01 - c10 + c11;
}

static void dequant_chroma_dc2x2(int32_t *f, int chroma_qp) {
    int m = chroma_qp % 6;
    int shift = chroma_qp / 6;
    int32_t scale = (int32_t)kNormAdjust4x4[m][0] * 16;
    for (int i = 0; i < 4; i++) {
        f[i] = (f[i] * scale * (1 << shift)) >> 5;
    }
}

/* 4x4 核心反变换（clause 8.5.12.2），含 (+32)>>6 归一化，输出即空间域
 * 残差，直接供 add_residual_4x4 */
static void idct4x4(int32_t *block) {
    int32_t tmp[16];
    for (int i = 0; i < 4; i++) {
        int32_t d0 = block[i + 0 * 4], d1 = block[i + 1 * 4];
        int32_t d2 = block[i + 2 * 4], d3 = block[i + 3 * 4];
        int32_t e0 = d0 + d2, e1 = d0 - d2;
        int32_t e2 = (d1 >> 1) - d3, e3 = d1 + (d3 >> 1);
        tmp[i + 0 * 4] = e0 + e3;
        tmp[i + 1 * 4] = e1 + e2;
        tmp[i + 2 * 4] = e1 - e2;
        tmp[i + 3 * 4] = e0 - e3;
    }
    for (int j = 0; j < 4; j++) {
        int32_t d0 = tmp[j * 4 + 0], d1 = tmp[j * 4 + 1];
        int32_t d2 = tmp[j * 4 + 2], d3 = tmp[j * 4 + 3];
        int32_t e0 = d0 + d2, e1 = d0 - d2;
        int32_t e2 = (d1 >> 1) - d3, e3 = d1 + (d3 >> 1);
        block[j * 4 + 0] = (e0 + e3 + 32) >> 6;
        block[j * 4 + 1] = (e1 + e2 + 32) >> 6;
        block[j * 4 + 2] = (e1 - e2 + 32) >> 6;
        block[j * 4 + 3] = (e0 - e3 + 32) >> 6;
    }
}

/* 预测 + 残差重建（Clip1，clause 8.5.12.1） */
static void add_residual_4x4(uint8_t *dst, int stride, const int32_t *residual) {
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            dst[y * stride + x] =
                clip255((int32_t)dst[y * stride + x] + residual[y * 4 + x]);
        }
    }
}

/* ================================================================== */
/* CAVLC 熵解码（上游 h264_cavlc.h，clause 9.2）                       */
/* ================================================================== */

/* 通用前缀码匹配：逐位读并与每个 {len,bits} 项比对，首个长度+值都匹配的
 * 项即结果。表保证是合法前缀码，未用槽 len==0 永不匹配。 */
static int decode_vlc(h264_br_t *br, const uint8_t *lens, const uint8_t *bits,
                      int count, int max_len, int *out_index) {
    uint32_t code = 0;
    for (int len = 1; len <= max_len; len++) {
        code = (code << 1) | br_u(br, 1);
        if (br->error) return 0;
        for (int i = 0; i < count; i++) {
            if (lens[i] == len && bits[i] == code) {
                *out_index = i;
                return 1;
            }
        }
    }
    return 0;
}

/* coeff_token（clause 9.2.1，Table 9-5）。nC==-1 为色度 DC 专用表。 */
static int decode_coeff_token(h264_br_t *br, int nC, uint32_t *total_coeff,
                              uint32_t *trailing_ones) {
    if (nC == -1) {
        int idx;
        if (!decode_vlc(br, kChromaDcCoeffTokenLen, kChromaDcCoeffTokenBits,
                        20, 8, &idx))
            return 0;
        *total_coeff = (uint32_t)idx >> 2;
        *trailing_ones = (uint32_t)idx & 3;
        return 1;
    }
    if (nC >= 8) {
        /* Table 9-5 nC>=8：6bit 定长。code==3 是 TotalCoeff=0 的专用槽
         * （其余值隐含 TC=1,T=3 不可能） */
        uint32_t code = br_u(br, 6);
        if (br->error) return 0;
        if (code == 3) {
            *total_coeff = 0;
            *trailing_ones = 0;
        } else {
            *total_coeff = (code >> 2) + 1;
            *trailing_ones = code & 3;
        }
        return 1;
    }
    int table = nC_to_coeff_token_table(nC);
    int idx;
    if (!decode_vlc(br, kCoeffTokenLen[table], kCoeffTokenBits[table],
                    4 * 17, 16, &idx))
        return 0;
    *total_coeff = (uint32_t)idx >> 2;
    *trailing_ones = (uint32_t)idx & 3;
    return 1;
}

/* totalCoeff 个带符号 level（clause 9.2.2），高扫描位在前 */
static int decode_levels(h264_br_t *br, uint32_t total_coeff,
                         uint32_t trailing_ones, int32_t *level) {
    for (uint32_t i = 0; i < trailing_ones; i++) {
        level[i] = br_flag(br) ? -1 : 1;
    }

    int suffix_length = (total_coeff > 10 && trailing_ones < 3) ? 1 : 0;
    for (uint32_t i = trailing_ones; i < total_coeff; i++) {
        int level_prefix = 0;
        while (br_u(br, 1) == 0) {
            level_prefix++;
            if (br->error || level_prefix > 28) return 0;
        }

        int level_suffix_size;
        if (level_prefix == 14 && suffix_length == 0) {
            level_suffix_size = 4;
        } else if (level_prefix >= 15) {
            level_suffix_size = level_prefix - 3;
        } else {
            level_suffix_size = suffix_length;
        }
        uint32_t level_suffix = level_suffix_size > 0 ? br_u(br, level_suffix_size) : 0;
        if (br->error) return 0;

        int32_t level_code =
            (int32_t)((level_prefix < 15 ? level_prefix : 15) << suffix_length) +
            (int32_t)level_suffix;
        if (level_prefix >= 15 && suffix_length == 0) level_code += 15;
        if (level_prefix >= 16) level_code += (1 << (level_prefix - 3)) - 4096;
        if (i == trailing_ones && trailing_ones < 3) level_code += 2;

        int32_t lvl = (level_code % 2 == 0) ? (level_code + 2) >> 1
                                            : (-level_code - 1) >> 1;
        level[i] = lvl;

        /* clause 9.2.2.1：注意两步都无条件执行——先 0->1 再判增长。
         * 更"字面"的互斥读法曾实测破坏与 ffmpeg 的逐像素一致。 */
        if (suffix_length == 0) suffix_length = 1;
        int32_t abs_lvl = lvl < 0 ? -lvl : lvl;
        if (abs_lvl > (3 << (suffix_length - 1)) && suffix_length < 6) {
            suffix_length++;
        }
    }
    return 1;
}

/* total_zeros（Table 9-7/8/9a） */
static int decode_total_zeros(h264_br_t *br, uint32_t total_coeff,
                              int max_num_coeff, uint32_t *zeros_left) {
    if ((int)total_coeff == max_num_coeff) {
        *zeros_left = 0;
        return 1;
    }
    int idx;
    if (max_num_coeff == 4) {
        if (total_coeff < 1 || total_coeff > 3) return 0;
        if (!decode_vlc(br, kChromaDcTotalZerosLen[total_coeff - 1],
                        kChromaDcTotalZerosBits[total_coeff - 1], 4, 3, &idx))
            return 0;
    } else {
        if (total_coeff < 1 || total_coeff > 15) return 0;
        if (!decode_vlc(br, kTotalZerosLen[total_coeff - 1],
                        kTotalZerosBits[total_coeff - 1], 16, 9, &idx))
            return 0;
    }
    *zeros_left = (uint32_t)idx;
    return 1;
}

/* run_before（Table 9-10） */
static int decode_run_before(h264_br_t *br, uint32_t zeros_left,
                             uint32_t *run_before) {
    int idx;
    if (zeros_left < 7) {
        if (!decode_vlc(br, kRunBeforeLen[zeros_left - 1],
                        kRunBeforeBits[zeros_left - 1], 16, 3, &idx))
            return 0;
    } else {
        if (!decode_vlc(br, kRunBeforeLen[6], kRunBeforeBits[6], 16, 11, &idx))
            return 0;
    }
    *run_before = (uint32_t)idx;
    return 1;
}

/* residual_block_cavlc()（clause 9.2 顶层入口）：按扫描位序解出最多
 * max_num_coeff 个系数到 coeffLevel[0..max_num_coeff)。zigzag->2D 是
 * 调用方的事。total_coeff_out 成功时总是被写入（含 0，供 nnz 缓存）。 */
static int residual_block_cavlc(h264_br_t *br, int nC, int max_num_coeff,
                                int32_t *coeff_level, uint32_t *total_coeff_out) {
    for (int i = 0; i < max_num_coeff; i++) coeff_level[i] = 0;

    uint32_t total_coeff, trailing_ones;
    if (!decode_coeff_token(br, nC, &total_coeff, &trailing_ones)) return 0;
    if (total_coeff_out) *total_coeff_out = total_coeff;
    if (total_coeff == 0) return 1;
    if ((int)total_coeff > max_num_coeff) return 0;

    int32_t level[16];
    if (!decode_levels(br, total_coeff, trailing_ones, level)) return 0;

    uint32_t zeros_left = 0;
    if (!decode_total_zeros(br, total_coeff, max_num_coeff, &zeros_left))
        return 0;

    uint32_t run_val[16];
    for (uint32_t i = 0; i + 1 < total_coeff; i++) {
        uint32_t rb = 0;
        if (zeros_left > 0) {
            if (!decode_run_before(br, zeros_left, &rb)) return 0;
        }
        run_val[i] = rb;
        zeros_left -= rb;
    }
    run_val[total_coeff - 1] = zeros_left;

    int coeff_num = -1;
    for (int i = (int)total_coeff - 1; i >= 0; i--) {
        coeff_num += (int)run_val[i] + 1;
        if (coeff_num < 0 || coeff_num >= max_num_coeff) return 0;
        coeff_level[coeff_num] = level[i];
    }
    return 1;
}

/* ================================================================== */
/* 帧内预测（上游 h264_intra_pred.h，clause 8.3）                      */
/* ================================================================== */

/* 右上邻居永不可用的 4x4 块（光栅解码序推论，见上游 h264_mb_info.h） */
static int blk4x4_top_right_always_unavailable(int blk_idx) {
    return blk_idx == 3 || blk_idx == 7 || blk_idx == 11 || blk_idx == 13 ||
           blk_idx == 15;
}

typedef struct {
    int have_left, have_top, have_top_left, have_top_right;
    uint8_t left[4];
    uint8_t top[4];
    uint8_t top_right[4]; /* 不可用时以 top[3] 填充替换 */
    uint8_t top_left;
} h264_neighbors4x4_t;

static h264_neighbors4x4_t gather_neighbors4x4(const h264_frame_t *f, int px,
                                               int py, int blk_idx,
                                               int mb_left_avail,
                                               int mb_top_avail,
                                               int mb_top_left_avail,
                                               int mb_top_right_avail) {
    h264_neighbors4x4_t n;
    memset(&n, 0, sizeof(n));
    int bx = kBlk4x4X[blk_idx], by = kBlk4x4Y[blk_idx];

    n.have_left = (bx > 0) || mb_left_avail;
    n.have_top = (by > 0) || mb_top_avail;
    n.have_top_left = (bx > 0 && by > 0) ||
                      (bx == 0 && by > 0 && mb_left_avail) ||
                      (bx > 0 && by == 0 && mb_top_avail) ||
                      (bx == 0 && by == 0 && mb_top_left_avail);

    if (blk4x4_top_right_always_unavailable(blk_idx)) {
        n.have_top_right = 0;
    } else if (by == 0) {
        n.have_top_right = (bx == 3) ? mb_top_right_avail : mb_top_avail;
    } else {
        n.have_top_right = 1; /* 块内局部，光栅序保证已重建 */
    }

    if (n.have_left) {
        for (int i = 0; i < 4; i++)
            n.left[i] = frame_y_row_c(f, py + i)[px - 1];
    }
    if (n.have_top) {
        const uint8_t *row = frame_y_row_c(f, py - 1);
        for (int i = 0; i < 4; i++) n.top[i] = row[px + i];
    }
    if (n.have_top_left) {
        n.top_left = frame_y_row_c(f, py - 1)[px - 1];
    }
    if (n.have_top_right) {
        const uint8_t *row = frame_y_row_c(f, py - 1);
        for (int i = 0; i < 4; i++) n.top_right[i] = row[px + 4 + i];
    } else if (n.have_top) {
        for (int i = 0; i < 4; i++) n.top_right[i] = n.top[3];
    }
    return n;
}

/* topAt/leftAt：i<0 时取 topLeft（上游 lambda 的等价展开） */
static inline int top_at(const uint8_t *top, uint8_t top_left, int i) {
    return i < 0 ? top_left : top[i];
}
static inline int left_at(const uint8_t *left, uint8_t top_left, int i) {
    return i < 0 ? top_left : left[i];
}

static void predict_intra4x4(h264_frame_t *f, int px, int py, int mode,
                             const h264_neighbors4x4_t *n) {
    uint8_t pred[16]; /* raster [y*4+x] */
    const uint8_t *top = n->top, *left = n->left, *top_right = n->top_right;
    uint8_t top_left = n->top_left;
    /* 扩展顶行 t[0..7] = top[0..3] + topRight[0..3] */
    uint8_t t[8] = {top[0], top[1], top[2], top[3],
                    top_right[0], top_right[1], top_right[2], top_right[3]};

    switch (mode) {
        case H264_I4_VERTICAL:
            for (int y = 0; y < 4; y++)
                for (int x = 0; x < 4; x++) pred[y * 4 + x] = top[x];
            break;
        case H264_I4_HORIZONTAL:
            for (int y = 0; y < 4; y++)
                for (int x = 0; x < 4; x++) pred[y * 4 + x] = left[y];
            break;
        case H264_I4_DC: {
            int dc;
            if (n->have_top && n->have_left) {
                int s = top[0] + top[1] + top[2] + top[3] + left[0] +
                        left[1] + left[2] + left[3];
                dc = (s + 4) >> 3;
            } else if (n->have_top) {
                int s = top[0] + top[1] + top[2] + top[3];
                dc = (s + 2) >> 2;
            } else if (n->have_left) {
                int s = left[0] + left[1] + left[2] + left[3];
                dc = (s + 2) >> 2;
            } else {
                dc = 128;
            }
            for (int i = 0; i < 16; i++) pred[i] = (uint8_t)dc;
            break;
        }
        case H264_I4_DIAG_DOWN_LEFT:
            for (int y = 0; y < 4; y++) {
                for (int x = 0; x < 4; x++) {
                    int v;
                    if (x == 3 && y == 3) {
                        v = (t[6] + 3 * t[7] + 2) >> 2;
                    } else {
                        int i = x + y;
                        v = (t[i] + 2 * t[i + 1] + t[i + 2] + 2) >> 2;
                    }
                    pred[y * 4 + x] = (uint8_t)v;
                }
            }
            break;
        case H264_I4_DIAG_DOWN_RIGHT:
            for (int y = 0; y < 4; y++) {
                for (int x = 0; x < 4; x++) {
                    int v;
                    if (x > y) {
                        int i = x - y;
                        v = (top_at(top, top_left, i - 2) +
                             2 * top_at(top, top_left, i - 1) +
                             top_at(top, top_left, i) + 2) >> 2;
                    } else if (x < y) {
                        int i = y - x;
                        v = (left_at(left, top_left, i - 2) +
                             2 * left_at(left, top_left, i - 1) +
                             left_at(left, top_left, i) + 2) >> 2;
                    } else {
                        v = (top[0] + 2 * top_left + left[0] + 2) >> 2;
                    }
                    pred[y * 4 + x] = (uint8_t)v;
                }
            }
            break;
        case H264_I4_VERTICAL_RIGHT:
            for (int y = 0; y < 4; y++) {
                for (int x = 0; x < 4; x++) {
                    int z_vr = 2 * x - y;
                    int v;
                    if (z_vr >= 0 && (z_vr & 1) == 0) {
                        int i = x - (y >> 1) - 1;
                        v = (top_at(top, top_left, i) +
                             top_at(top, top_left, i + 1) + 1) >> 1;
                    } else if (z_vr >= 1) {
                        int i = x - (y >> 1) - 2;
                        v = (top_at(top, top_left, i) +
                             2 * top_at(top, top_left, i + 1) +
                             top_at(top, top_left, i + 2) + 2) >> 2;
                    } else if (z_vr == -1) {
                        v = (left[0] + 2 * top_left + top[0] + 2) >> 2;
                    } else {
                        v = (left_at(left, top_left, y - 1) +
                             2 * left_at(left, top_left, y - 2) +
                             left_at(left, top_left, y - 3) + 2) >> 2;
                    }
                    pred[y * 4 + x] = (uint8_t)v;
                }
            }
            break;
        case H264_I4_HORIZONTAL_DOWN:
            for (int y = 0; y < 4; y++) {
                for (int x = 0; x < 4; x++) {
                    int z_hd = 2 * y - x;
                    int v;
                    if (z_hd >= 0 && (z_hd & 1) == 0) {
                        int i = y - (x >> 1) - 1;
                        v = (left_at(left, top_left, i) +
                             left_at(left, top_left, i + 1) + 1) >> 1;
                    } else if (z_hd >= 1) {
                        int i = y - (x >> 1) - 2;
                        v = (left_at(left, top_left, i) +
                             2 * left_at(left, top_left, i + 1) +
                             left_at(left, top_left, i + 2) + 2) >> 2;
                    } else if (z_hd == -1) {
                        v = (left[0] + 2 * top_left + top[0] + 2) >> 2;
                    } else {
                        v = (top_at(top, top_left, x - 1) +
                             2 * top_at(top, top_left, x - 2) +
                             top_at(top, top_left, x - 3) + 2) >> 2;
                    }
                    pred[y * 4 + x] = (uint8_t)v;
                }
            }
            break;
        case H264_I4_VERTICAL_LEFT:
            for (int y = 0; y < 4; y++) {
                for (int x = 0; x < 4; x++) {
                    int i = x + (y >> 1);
                    int v = (y & 1) == 0 ? (t[i] + t[i + 1] + 1) >> 1
                                         : (t[i] + 2 * t[i + 1] + t[i + 2] + 2) >> 2;
                    pred[y * 4 + x] = (uint8_t)v;
                }
            }
            break;
        case H264_I4_HORIZONTAL_UP:
            for (int y = 0; y < 4; y++) {
                for (int x = 0; x < 4; x++) {
                    int z_hu = x + 2 * y;
                    int v;
                    if (z_hu <= 4 && (z_hu & 1) == 0) {
                        int i = y + (x >> 1);
                        v = (left[i] + left[i + 1] + 1) >> 1;
                    } else if (z_hu <= 4) {
                        int i = y + (x >> 1);
                        v = (left[i] + 2 * left[i + 1] + left[i + 2] + 2) >> 2;
                    } else if (z_hu == 5) {
                        v = (left[2] + 3 * left[3] + 2) >> 2;
                    } else {
                        v = left[3];
                    }
                    pred[y * 4 + x] = (uint8_t)v;
                }
            }
            break;
    }

    for (int y = 0; y < 4; y++) {
        uint8_t *row = frame_y_row(f, py + y);
        for (int x = 0; x < 4; x++) row[px + x] = pred[y * 4 + x];
    }
}

/* Plane 模式 (x,y) 处的值（16x16 亮度 / 8x8 色率共用，size 选舍入常数） */
static int plane_predict(int x, int y, int size, const uint8_t *top,
                         const uint8_t *left, uint8_t top_left) {
    int half = size / 2;
    int H = 0, V = 0;
    for (int i = 0; i < half; i++) {
        H += (i + 1) * (top_at(top, top_left, half + i) -
                        top_at(top, top_left, half - 2 - i));
        V += (i + 1) * (left_at(left, top_left, half + i) -
                        left_at(left, top_left, half - 2 - i));
    }
    int a = 16 * (top[size - 1] + left[size - 1]);
    int b, c;
    if (size == 16) {
        b = (5 * H + 32) >> 6;
        c = (5 * V + 32) >> 6;
    } else { /* size == 8，色率 */
        b = (17 * H + 16) >> 5;
        c = (17 * V + 16) >> 5;
    }
    int v = (a + b * (x - (half - 1)) + c * (y - (half - 1)) + 16) >> 5;
    if (v < 0) v = 0;
    if (v > 255) v = 255;
    return v;
}

static void predict_intra16x16(h264_frame_t *f, int mb_x, int mb_y, int mode,
                               int left_avail, int top_avail,
                               int top_left_avail) {
    int px = mb_x * 16, py = mb_y * 16;
    uint8_t top[16], left[16];
    uint8_t top_left = top_left_avail ? frame_y_row_c(f, py - 1)[px - 1] : 0;
    if (top_avail) memcpy(top, frame_y_row_c(f, py - 1) + px, 16);
    if (left_avail)
        for (int i = 0; i < 16; i++) left[i] = frame_y_row_c(f, py + i)[px - 1];

    for (int y = 0; y < 16; y++) {
        uint8_t *row = frame_y_row(f, py + y);
        for (int x = 0; x < 16; x++) {
            int v;
            switch (mode) {
                case H264_I16_VERTICAL:
                    v = top[x];
                    break;
                case H264_I16_HORIZONTAL:
                    v = left[y];
                    break;
                case H264_I16_PLANE:
                    v = plane_predict(x, y, 16, top, left, top_left);
                    break;
                case H264_I16_DC:
                default:
                    if (top_avail && left_avail) {
                        int s = 0;
                        for (int i = 0; i < 16; i++) s += top[i] + left[i];
                        v = (s + 16) >> 5;
                    } else if (top_avail) {
                        int s = 0;
                        for (int i = 0; i < 16; i++) s += top[i];
                        v = (s + 8) >> 4;
                    } else if (left_avail) {
                        int s = 0;
                        for (int i = 0; i < 16; i++) s += left[i];
                        v = (s + 8) >> 4;
                    } else {
                        v = 128;
                    }
                    break;
            }
            row[px + x] = (uint8_t)v;
        }
    }
}

/* 一个宏块的 8x8 色率平面（Cb 或 Cr）。DC 模式按 4x4 象限分别求均值
 * （与 ffmpeg pred8x8_dc 交叉核对，右下象限取 top[4..7]+left[4..7]） */
static void predict_intra_chroma(uint8_t *plane, int stride, int px, int py,
                                 int mode, int left_avail, int top_avail,
                                 int top_left_avail) {
    uint8_t top[8], left[8];
    uint8_t top_left =
        top_left_avail ? plane[(py - 1) * stride + (px - 1)] : 0;
    if (top_avail) memcpy(top, plane + (py - 1) * stride + px, 8);
    if (left_avail)
        for (int i = 0; i < 8; i++) left[i] = plane[(py + i) * stride + px - 1];

    for (int y = 0; y < 8; y++) {
        uint8_t *row = plane + (py + y) * stride;
        for (int x = 0; x < 8; x++) {
            int v;
            switch (mode) {
                case H264_CHROMA_VERTICAL:
                    v = top[x];
                    break;
                case H264_CHROMA_HORIZONTAL:
                    v = left[y];
                    break;
                case H264_CHROMA_PLANE:
                    v = plane_predict(x, y, 8, top, left, top_left);
                    break;
                case H264_CHROMA_DC:
                default: {
                    int qx = (x >> 2) * 4, qy = (y >> 2) * 4;
                    int top_quad = (qx == 4 && qy == 0);
                    int left_quad = (qx == 0 && qy == 4);
                    int corner_quad = (qx == 0 && qy == 0);
                    if (corner_quad) {
                        if (top_avail && left_avail) {
                            int s = top[0] + top[1] + top[2] + top[3] +
                                    left[0] + left[1] + left[2] + left[3];
                            v = (s + 4) >> 3;
                        } else if (top_avail) {
                            v = (top[0] + top[1] + top[2] + top[3] + 2) >> 2;
                        } else if (left_avail) {
                            v = (left[0] + left[1] + left[2] + left[3] + 2) >> 2;
                        } else {
                            v = 128;
                        }
                    } else if (top_quad) {
                        if (top_avail) {
                            v = (top[4] + top[5] + top[6] + top[7] + 2) >> 2;
                        } else if (left_avail) {
                            v = (left[0] + left[1] + left[2] + left[3] + 2) >> 2;
                        } else {
                            v = 128;
                        }
                    } else if (left_quad) {
                        if (left_avail) {
                            v = (left[4] + left[5] + left[6] + left[7] + 2) >> 2;
                        } else if (top_avail) {
                            v = (top[0] + top[1] + top[2] + top[3] + 2) >> 2;
                        } else {
                            v = 128;
                        }
                    } else { /* 右下象限 */
                        if (top_avail && left_avail) {
                            int s = top[4] + top[5] + top[6] + top[7] +
                                    left[4] + left[5] + left[6] + left[7];
                            v = (s + 4) >> 3;
                        } else if (top_avail) {
                            v = (top[4] + top[5] + top[6] + top[7] + 2) >> 2;
                        } else if (left_avail) {
                            v = (left[4] + left[5] + left[6] + left[7] + 2) >> 2;
                        } else {
                            v = 128;
                        }
                    }
                    break;
                }
            }
            row[px + x] = (uint8_t)v;
        }
    }
}

/* ================================================================== */
/* 运动补偿（上游 h264_motion.h，clause 8.4.2）：亮度 1/4 采样 6tap FIR  */
/* + 双线性，色率 1/8 采样双线性直接插值                                */
/* ================================================================== */

/* 边缘复制的钳位全采样点取值（MV 可指向图像外，clause 8.4.2.2.1） */
static int clamped_sample(const uint8_t *plane, int stride, int width,
                          int height, int x, int y) {
    if (x < 0) x = 0;
    if (x >= width) x = width - 1;
    if (y < 0) y = 0;
    if (y >= height) y = height - 1;
    return plane[(size_t)y * stride + x];
}

/* 6tap FIR [1,-5,20,20,-5,1] */
static int tap6(int a, int b, int c, int d, int e, int f) {
    return a - 5 * b + 20 * c + 20 * d - 5 * e + f;
}

static int raw_half_h(const uint8_t *plane, int stride, int w, int h, int x,
                      int y) {
    return tap6(clamped_sample(plane, stride, w, h, x - 2, y),
                clamped_sample(plane, stride, w, h, x - 1, y),
                clamped_sample(plane, stride, w, h, x, y),
                clamped_sample(plane, stride, w, h, x + 1, y),
                clamped_sample(plane, stride, w, h, x + 2, y),
                clamped_sample(plane, stride, w, h, x + 3, y));
}

static int raw_half_v(const uint8_t *plane, int stride, int w, int h, int x,
                      int y) {
    return tap6(clamped_sample(plane, stride, w, h, x, y - 2),
                clamped_sample(plane, stride, w, h, x, y - 1),
                clamped_sample(plane, stride, w, h, x, y),
                clamped_sample(plane, stride, w, h, x, y + 1),
                clamped_sample(plane, stride, w, h, x, y + 2),
                clamped_sample(plane, stride, w, h, x, y + 3));
}

static int half_h(const uint8_t *plane, int stride, int w, int h, int x, int y) {
    return clip255((raw_half_h(plane, stride, w, h, x, y) + 16) >> 5);
}
static int half_v(const uint8_t *plane, int stride, int w, int h, int x, int y) {
    return clip255((raw_half_v(plane, stride, w, h, x, y) + 16) >> 5);
}

static int avg2(int a, int b) { return (a + b + 1) >> 1; }

/* 中心 "j" 半采样：对 6 行未舍入的水平半和再做一次 6tap */
static int center_j(const uint8_t *plane, int stride, int w, int h, int x,
                    int y) {
    int s0 = raw_half_h(plane, stride, w, h, x, y - 2);
    int s1 = raw_half_h(plane, stride, w, h, x, y - 1);
    int s2 = raw_half_h(plane, stride, w, h, x, y);
    int s3 = raw_half_h(plane, stride, w, h, x, y + 1);
    int s4 = raw_half_h(plane, stride, w, h, x, y + 2);
    int s5 = raw_half_h(plane, stride, w, h, x, y + 3);
    return clip255((tap6(s0, s1, s2, s3, s4, s5) + 512) >> 10);
}

/* 整采样基点 (x0,y0) + 1/4 采样偏移 (fracX,fracY in 0..3) 的亮度插值 */
static uint8_t interp_luma_sample(const uint8_t *plane, int stride, int w,
                                  int h, int x0, int y0, int frac_x,
                                  int frac_y) {
    if (frac_x == 0 && frac_y == 0) {
        return (uint8_t)clamped_sample(plane, stride, w, h, x0, y0);
    }
    int G = clamped_sample(plane, stride, w, h, x0, y0);
    if (frac_y == 0) {
        int b = half_h(plane, stride, w, h, x0, y0);
        if (frac_x == 2) return (uint8_t)b;
        int H = clamped_sample(plane, stride, w, h, x0 + 1, y0);
        return (uint8_t)(frac_x == 1 ? avg2(G, b) : avg2(b, H));
    }
    if (frac_x == 0) {
        int hh = half_v(plane, stride, w, h, x0, y0);
        if (frac_y == 2) return (uint8_t)hh;
        int M = clamped_sample(plane, stride, w, h, x0, y0 + 1);
        return (uint8_t)(frac_y == 1 ? avg2(G, hh) : avg2(hh, M));
    }
    if (frac_x == 2 && frac_y == 2) {
        return (uint8_t)center_j(plane, stride, w, h, x0, y0);
    }
    int b = half_h(plane, stride, w, h, x0, y0);     /* 顶行水平半 */
    int s = half_h(plane, stride, w, h, x0, y0 + 1); /* 底行水平半 */
    int h_col = half_v(plane, stride, w, h, x0, y0); /* 左列垂直半 */
    int m = half_v(plane, stride, w, h, x0 + 1, y0); /* 右列垂直半 */
    if (frac_x == 2) {
        int j = center_j(plane, stride, w, h, x0, y0);
        return (uint8_t)avg2(frac_y == 1 ? b : s, j);
    }
    if (frac_y == 2) {
        int j = center_j(plane, stride, w, h, x0, y0);
        return (uint8_t)avg2(frac_x == 1 ? h_col : m, j);
    }
    /* 对角 1/4 位置 e,g,p,r */
    if (frac_x == 1 && frac_y == 1) return (uint8_t)avg2(h_col, b);
    if (frac_x == 3 && frac_y == 1) return (uint8_t)avg2(b, m);
    if (frac_x == 1 && frac_y == 3) return (uint8_t)avg2(h_col, s);
    return (uint8_t)avg2(m, s);
}

static void motion_comp_luma(uint8_t *dst, int dst_stride, const h264_frame_t *ref,
                             int origin_x, int origin_y, int block_w,
                             int block_h, int mv_x, int mv_y) {
    int full_x = origin_x + (mv_x >> 2);
    int full_y = origin_y + (mv_y >> 2);
    int frac_x = mv_x & 3;
    int frac_y = mv_y & 3;
    for (int y = 0; y < block_h; y++) {
        for (int x = 0; x < block_w; x++) {
            dst[y * dst_stride + x] =
                interp_luma_sample(ref->y, ref->stride_y, ref->width, ref->height,
                                   full_x + x, full_y + y, frac_x, frac_y);
        }
    }
}

/* 色率：1/8 采样双线性。4:2:0 逐行扫描下 mvCL==mvL（clause 8.4.1.4），
 * 亮度 1/4 单位数值直接当色率 1/8 单位用 */
static void motion_comp_chroma(uint8_t *dst, int dst_stride,
                               const uint8_t *ref_plane, int ref_stride,
                               int ref_w, int ref_h, int origin_x, int origin_y,
                               int block_w, int block_h, int mv_x, int mv_y) {
    int full_x = origin_x + (mv_x >> 3);
    int full_y = origin_y + (mv_y >> 3);
    int frac_x = mv_x & 7;
    int frac_y = mv_y & 7;
    for (int y = 0; y < block_h; y++) {
        for (int x = 0; x < block_w; x++) {
            int A = clamped_sample(ref_plane, ref_stride, ref_w, ref_h,
                                   full_x + x, full_y + y);
            int B = clamped_sample(ref_plane, ref_stride, ref_w, ref_h,
                                   full_x + x + 1, full_y + y);
            int C = clamped_sample(ref_plane, ref_stride, ref_w, ref_h,
                                   full_x + x, full_y + y + 1);
            int D = clamped_sample(ref_plane, ref_stride, ref_w, ref_h,
                                   full_x + x + 1, full_y + y + 1);
            int v = ((8 - frac_x) * (8 - frac_y) * A + frac_x * (8 - frac_y) * B +
                     (8 - frac_x) * frac_y * C + frac_x * frac_y * D + 32) >> 6;
            dst[y * dst_stride + x] = (uint8_t)v;
        }
    }
}

/* ================================================================== */
/* MV 预测（上游 h264_mv_predict.h，clause 8.4.1.3）                    */
/* ================================================================== */

/* 4x4 网格邻居查找结果：available=存在（图内且同 slice）；interCoded=
 * 携带真实(非 Intra) MV。Intra/不可用邻居 refIdx=-1（永不等与真实
 * 分区的 refIdx>=0），"邻居 ref_idx 是否匹配"的比较因此天然正确 */
typedef struct {
    int available;
    int inter_coded;
    int8_t ref_idx;
    int16_t mv[2];
} h264_mv_neighbor_t;

static int median3(int a, int b, int c) {
    return a + b + c - (a < b ? (a < c ? a : c) : (b < c ? b : c)) -
           (a > b ? (a > c ? a : c) : (b > c ? b : c));
}

static void predict_mv_median(int have_a, const int16_t *mv_a, int have_b,
                              const int16_t *mv_b, int have_c,
                              const int16_t *mv_c, int16_t *out_mv) {
    /* 不可用/Intra 邻居贡献 (0,0)（clause 8.4.1.3.2） */
    int ax = have_a ? mv_a[0] : 0, ay = have_a ? mv_a[1] : 0;
    int bx = have_b ? mv_b[0] : 0, by = have_b ? mv_b[1] : 0;
    int cx = have_c ? mv_c[0] : 0, cy = have_c ? mv_c[1] : 0;
    out_mv[0] = (int16_t)median3(ax, bx, cx);
    out_mv[1] = (int16_t)median3(ay, by, cy);
}

/* 当前宏块 4x4 网格相对位置 (bx,by) 的邻居 MV 查找。bx>=4 且 by>=0
 * （正右/下方）永不可用——光栅序尚未解码；例外 bx==4,by<0 是右上宏块 */
typedef struct {
    h264_mbtab_t *mb_info;
    int mb_x, mb_y, slice_id;
} h264_mbview_t;

static h264_mv_neighbor_t mv_neighbor_at(const h264_mbview_t *v, int bx, int by) {
    h264_mv_neighbor_t n;
    memset(&n, 0, sizeof(n));
    n.ref_idx = -1;

    if (by >= 4) return n;
    if (bx >= 4 && by >= 0) return n;
    if (bx >= 0 && by >= 0) {
        const h264_mbinfo_t *cur = mbtab_at_c(v->mb_info, v->mb_x, v->mb_y);
        n.available = 1;
        n.inter_coded = 1; /* 宏块内：仅在解码 Inter 宏块时走到 */
        int idx = kInvBlk4x4[by][bx];
        n.ref_idx = cur->ref_idx[idx];
        n.mv[0] = cur->mv[idx][0];
        n.mv[1] = cur->mv[idx][1];
        return n;
    }
    int nmb_x = v->mb_x + (bx < 0 ? -1 : (bx >= 4 ? 1 : 0));
    int nmb_y = v->mb_y + (by < 0 ? -1 : 0);
    if (!mbtab_decoded(v->mb_info, nmb_x, nmb_y, v->slice_id)) return n;
    const h264_mbinfo_t *nmb = mbtab_at_c(v->mb_info, nmb_x, nmb_y);
    n.available = 1;
    if (mbinfo_is_intra(nmb)) {
        n.inter_coded = 0;
        n.mv[0] = n.mv[1] = 0;
        return n;
    }
    n.inter_coded = 1;
    int wbx = (bx < 0) ? 3 : (bx >= 4 ? bx - 4 : bx);
    int wby = (by < 0) ? 3 : by;
    int idx = kInvBlk4x4[wby][wbx];
    n.ref_idx = nmb->ref_idx[idx];
    n.mv[0] = nmb->mv[idx][0];
    n.mv[1] = nmb->mv[idx][1];
    return n;
}

/* 邻居 C：右上 (bx+pw, by-1)，不可用回退 D（左上） */
static h264_mv_neighbor_t mv_neighbor_c(const h264_mbview_t *v, int bx, int by,
                                        int pw, int ph) {
    (void)ph;
    h264_mv_neighbor_t c = mv_neighbor_at(v, bx + pw, by - 1);
    if (c.available) return c;
    return mv_neighbor_at(v, bx - 1, by - 1);
}

/* clause 8.4.1.3 标准中值预测。directional_side：-1 无捷径，
 * 0=A 捷捷(8x16 左/16x8 下)，1=B(16x8 上)，2=C(8x16 右) */
static void predict_mv_general(const h264_mbview_t *v, int bx, int by, int pw,
                               int ph, int cur_ref_idx, int directional_side,
                               int16_t *out_mv) {
    h264_mv_neighbor_t a = mv_neighbor_at(v, bx - 1, by);
    h264_mv_neighbor_t b = mv_neighbor_at(v, bx, by - 1);
    h264_mv_neighbor_t c = mv_neighbor_c(v, bx, by, pw, ph);

    if (directional_side >= 0) {
        /* 16x8/8x16 单侧先验（Table 8-9）：先于通用流程尝试（不是替代
         * ——单侧不匹配时下面的"恰有一个匹配"规则仍可能生效） */
        const h264_mv_neighbor_t *side =
            directional_side == 0 ? &a
            : directional_side == 1 ? &b
                                    : &c;
        if (side->inter_coded && side->ref_idx == cur_ref_idx) {
            out_mv[0] = side->mv[0];
            out_mv[1] = side->mv[1];
            return;
        }
    }
    {
        /* 通用流程：interCoded 且 refIdx 相同才算"匹配"（不可用/Intra 的
         * refIdx=-1 永不匹配）。恰一个匹配则直接取它的 MV */
        int ma = a.inter_coded && a.ref_idx == cur_ref_idx;
        int mb_ = b.inter_coded && b.ref_idx == cur_ref_idx;
        int mc = c.inter_coded && c.ref_idx == cur_ref_idx;
        int matches = (ma ? 1 : 0) + (mb_ ? 1 : 0) + (mc ? 1 : 0);
        if (matches == 1) {
            const h264_mv_neighbor_t *only = ma ? &a : (mb_ ? &b : &c);
            out_mv[0] = only->mv[0];
            out_mv[1] = only->mv[1];
            return;
        }
        /* 零匹配特例（与 ffmpeg pred_motion 交叉核对）：B、C 都真不可用
         * （图/slice 边缘，而不仅是 Intra 或 ref 不匹配）且 A 可用时，
         * 不论 A 的 ref 是否匹配都直接取 A 的 MV。单参考流永远暴露不
         * 了这条规则（A ref 不匹配的情况只出现在多参考流） */
        if (matches == 0 && !b.available && !c.available && a.available) {
            out_mv[0] = a.mv[0];
            out_mv[1] = a.mv[1];
            return;
        }
    }

    predict_mv_median(a.available, a.mv, b.available, b.mv, c.available, c.mv,
                      out_mv);
}

/* 分区 MV/refIdx 填充展开到覆盖的所有 4x4 格（后续邻居查找无需知道
 * 分区形状） */
static void fill_partition_mv(h264_mbinfo_t *mb, int bx, int by, int pw, int ph,
                              int16_t mv_x, int16_t mv_y, int8_t ref_idx) {
    for (int y = by; y < by + ph; y++) {
        for (int x = bx; x < bx + pw; x++) {
            int idx = kInvBlk4x4[y][x];
            mb->mv[idx][0] = mv_x;
            mb->mv[idx][1] = mv_y;
            mb->ref_idx[idx] = ref_idx;
        }
    }
}

/* ================================================================== */
/* 宏块层（上游 h264_macroblock.h / h264_macroblock_inter.h）          */
/* ================================================================== */

/* 一个 slice 内不变的解码上下文；mb_x/mb_y 由调用方在每宏块前更新 */
typedef struct {
    h264_frame_t *frame;
    const h264_frame_t *ref_list[H264_DEC_MAX_REFS]; /* 默认列表，无重排 */
    int num_active_refs;
    h264_mbtab_t *mb_info;
    const h264_sps_t *sps;
    const h264_pps_t *pps;
    int slice_id;
    int mb_x, mb_y;
} h264_mbctx_t;

typedef struct {
    int ok;
    int unsupported;
} h264_mbresult_t;

/* 亮度 4x4 块邻居 nnz 查找（nC 预测用），bx/by 为查询块网格位(0..3)，
 * dx/dy 为 -1/0 邻居方向。不可用返回 -1 */
static int luma_neighbor_nnz(const h264_mbctx_t *ctx, int bx, int by, int dx,
                             int dy) {
    int nbx = bx + dx, nby = by + dy;
    if (nbx >= 0 && nbx < 4 && nby >= 0 && nby < 4) {
        return mbtab_at(ctx->mb_info, ctx->mb_x, ctx->mb_y)
            ->nnz[kInvBlk4x4[nby][nbx]];
    }
    int nmb_x = ctx->mb_x + (nbx < 0 ? -1 : 0);
    int nmb_y = ctx->mb_y + (nby < 0 ? -1 : 0);
    if (!mbtab_decoded(ctx->mb_info, nmb_x, nmb_y, ctx->slice_id)) return -1;
    int wbx = (nbx < 0) ? 3 : nbx;
    int wby = (nby < 0) ? 3 : nby;
    return mbtab_at(ctx->mb_info, nmb_x, nmb_y)->nnz[kInvBlk4x4[wby][wbx]];
}

/* 色率 2x2(每平面) 4x4 块邻居 nnz 查找，cx/cy in 0..1 */
static int chroma_neighbor_nnz(const h264_mbctx_t *ctx, int plane, int cx,
                               int cy, int dx, int dy) {
    int ncx = cx + dx, ncy = cy + dy;
    int base = 16 + plane * 4;
    if (ncx >= 0 && ncx < 2 && ncy >= 0 && ncy < 2) {
        return mbtab_at(ctx->mb_info, ctx->mb_x, ctx->mb_y)
            ->nnz[base + ncy * 2 + ncx];
    }
    int nmb_x = ctx->mb_x + (ncx < 0 ? -1 : 0);
    int nmb_y = ctx->mb_y + (ncy < 0 ? -1 : 0);
    if (!mbtab_decoded(ctx->mb_info, nmb_x, nmb_y, ctx->slice_id)) return -1;
    int wcx = (ncx < 0) ? 1 : ncx;
    int wcy = (ncy < 0) ? 1 : ncy;
    return mbtab_at(ctx->mb_info, nmb_x, nmb_y)->nnz[base + wcy * 2 + wcx];
}

/* Intra_4x4 预测模式的预测（clause 8.3.1.1）。两层规则不可混淆：
 * 1) dcPredModePredictedFlag 是联合条件——任一侧真不可用则两侧都强制
 *    DC(2)，不只是缺的那侧；
 * 2) 该标志为 0 后，每侧再独立因"非 4x4 编码"（如 I_16x16/Inter 邻居）
 *    回退 DC(2)——这不回灌联合标志，也不连累另一侧。
 * （constrained_intra_pred 恒按 0 处理；flag=0 时本实现与 spec 一致） */
static int predict_intra4x4_mode(const h264_mbctx_t *ctx, int bx, int by,
                                 int mb_left_avail, int mb_top_avail) {
    int left_is_mb_boundary = (bx == 0);
    int top_is_mb_boundary = (by == 0);

    int left_available = left_is_mb_boundary ? mb_left_avail : 1;
    int top_available = top_is_mb_boundary ? mb_top_avail : 1;

    if (!left_available || !top_available) return 2;

    const h264_mbinfo_t *left_mb =
        left_is_mb_boundary
            ? mbtab_at_c(ctx->mb_info, ctx->mb_x - 1, ctx->mb_y)
            : NULL;
    const h264_mbinfo_t *top_mb =
        top_is_mb_boundary
            ? mbtab_at_c(ctx->mb_info, ctx->mb_x, ctx->mb_y - 1)
            : NULL;

    int mode_a;
    if (left_is_mb_boundary) {
        /* "非 4x4 编码"同时覆盖 I_16x16 邻居与 P slice 的 Inter 邻居 */
        mode_a = (left_mb->type != H264_MBT_INTRA4X4)
                     ? 2
                     : left_mb->intra4x4_pred_mode[kInvBlk4x4[by][3]];
    } else {
        mode_a = mbtab_at(ctx->mb_info, ctx->mb_x, ctx->mb_y)
                     ->intra4x4_pred_mode[kInvBlk4x4[by][bx - 1]];
    }
    int mode_b;
    if (top_is_mb_boundary) {
        mode_b = (top_mb->type != H264_MBT_INTRA4X4)
                     ? 2
                     : top_mb->intra4x4_pred_mode[kInvBlk4x4[3][bx]];
    } else {
        mode_b = mbtab_at(ctx->mb_info, ctx->mb_x, ctx->mb_y)
                     ->intra4x4_pred_mode[kInvBlk4x4[by - 1][bx]];
    }
    return mode_a < mode_b ? mode_a : mode_b;
}

/* 4x4 亮度块残差（含 DC 全 16 系数，I_4x4/Inter 通用）：CAVLC 解码 ->
 * zigzag 重排 -> 反量化 -> IDCT -> 加到已预测像素上 */
static int decode_luma_block_full(h264_br_t *br, h264_mbctx_t *ctx, int blk_idx,
                                  int qp) {
    int bx = kBlk4x4X[blk_idx], by = kBlk4x4Y[blk_idx];
    int nA = luma_neighbor_nnz(ctx, bx, by, -1, 0);
    int nB = luma_neighbor_nnz(ctx, bx, by, 0, -1);
    int nC = predict_nc(nA, nB);

    int32_t coeff[16];
    uint32_t total_coeff = 0;
    if (!residual_block_cavlc(br, nC, 16, coeff, &total_coeff)) {
        H264_LOG("lumaFull: CAVLC fail at mb(%d,%d) blk=%d\n",
                 ctx->mb_x, ctx->mb_y, blk_idx);
        return 0;
    }
    mbtab_at(ctx->mb_info, ctx->mb_x, ctx->mb_y)->nnz[blk_idx] =
        (uint8_t)total_coeff;

    int32_t block[16] = {0};
    for (int k = 0; k < 16; k++) block[kZigZag4x4[k]] = coeff[k];
    dequant4x4(block, qp, 0);
    idct4x4(block);

    int px = ctx->mb_x * 16 + bx * 4, py = ctx->mb_y * 16 + by * 4;
    add_residual_4x4(frame_y_row(ctx->frame, py) + px, ctx->frame->stride_y,
                     block);
    return 1;
}

/* 4x4 亮度 AC-only 块（I_16x16，扫描位 1..15；位 0 来自宏块级 DC 块） */
static int decode_luma_block_ac(h264_br_t *br, h264_mbctx_t *ctx, int blk_idx,
                                int qp, int32_t dc_value) {
    int bx = kBlk4x4X[blk_idx], by = kBlk4x4Y[blk_idx];
    int nA = luma_neighbor_nnz(ctx, bx, by, -1, 0);
    int nB = luma_neighbor_nnz(ctx, bx, by, 0, -1);
    int nC = predict_nc(nA, nB);

    int32_t coeff[15];
    uint32_t total_coeff = 0;
    if (!residual_block_cavlc(br, nC, 15, coeff, &total_coeff)) {
        H264_LOG("lumaAc: CAVLC fail at mb(%d,%d) blk=%d\n",
                 ctx->mb_x, ctx->mb_y, blk_idx);
        return 0;
    }
    mbtab_at(ctx->mb_info, ctx->mb_x, ctx->mb_y)->nnz[blk_idx] =
        (uint8_t)total_coeff;

    int32_t block[16] = {0};
    for (int k = 0; k < 15; k++) block[kZigZag4x4[k + 1]] = coeff[k];
    dequant4x4(block, qp, 1);
    block[0] = dc_value;
    idct4x4(block);

    int px = ctx->mb_x * 16 + bx * 4, py = ctx->mb_y * 16 + by * 4;
    add_residual_4x4(frame_y_row(ctx->frame, py) + px, ctx->frame->stride_y,
                     block);
    return 1;
}

/* 色率 4x4 AC-only 块（plane 0=Cb 1=Cr，cx/cy in 0..1），DC 已在位 0 */
static int decode_chroma_block_ac(h264_br_t *br, h264_mbctx_t *ctx, int plane,
                                  int cx, int cy, int chroma_qp,
                                  int32_t dc_value) {
    int nA = chroma_neighbor_nnz(ctx, plane, cx, cy, -1, 0);
    int nB = chroma_neighbor_nnz(ctx, plane, cx, cy, 0, -1);
    int nC = predict_nc(nA, nB);

    int32_t coeff[15];
    uint32_t total_coeff = 0;
    if (!residual_block_cavlc(br, nC, 15, coeff, &total_coeff)) {
        H264_LOG("chromaAc: CAVLC fail at mb(%d,%d) plane=%d\n",
                 ctx->mb_x, ctx->mb_y, plane);
        return 0;
    }
    mbtab_at(ctx->mb_info, ctx->mb_x, ctx->mb_y)
        ->nnz[16 + plane * 4 + cy * 2 + cx] = (uint8_t)total_coeff;

    int32_t block[16] = {0};
    for (int k = 0; k < 15; k++) block[kZigZag4x4[k + 1]] = coeff[k];
    dequant4x4(block, chroma_qp, 1);
    block[0] = dc_value;
    idct4x4(block);

    int px = ctx->mb_x * 8 + cx * 4, py = ctx->mb_y * 8 + cy * 4;
    uint8_t *plane_buf = plane == 0 ? ctx->frame->u : ctx->frame->v;
    add_residual_4x4(plane_buf + (size_t)py * ctx->frame->stride_c + px,
                     ctx->frame->stride_c, block);
    return 1;
}

/* 已知 I-slice 编号 mb_type（0=I_NxN，1..24=I_16x16 变体，25=I_PCM）的
 * macroblock_layer() 其余部分。P slice 内 Intra 宏块（mb_type>=5）同用，
 * 编号减 5。qpY 为运行中 QP，原地更新 */
static int decode_mb_intra_with_type(h264_br_t *br, h264_mbctx_t *ctx,
                                     uint32_t mb_type_raw, int *qp_y,
                                     h264_mbresult_t *result) {
    memset(result, 0, sizeof(*result));
    h264_mbinfo_t *mb = mbtab_at(ctx->mb_info, ctx->mb_x, ctx->mb_y);
    memset(mb, 0, sizeof(*mb));

    int left_avail =
        mbtab_left_available(ctx->mb_info, ctx->mb_x, ctx->mb_y, ctx->slice_id);
    int top_avail =
        mbtab_top_available(ctx->mb_info, ctx->mb_x, ctx->mb_y, ctx->slice_id);
    int top_left_avail =
        mbtab_top_left_available(ctx->mb_info, ctx->mb_x, ctx->mb_y, ctx->slice_id);
    int top_right_avail =
        mbtab_top_right_available(ctx->mb_info, ctx->mb_x, ctx->mb_y, ctx->slice_id);

    /* --- I_PCM --- */
    if (mb_type_raw == 25) {
        mb->type = H264_MBT_INTRA_PCM;
        br_byte_align(br);
        int px = ctx->mb_x * 16, py = ctx->mb_y * 16;
        for (int y = 0; y < 16; y++) {
            uint8_t *row = frame_y_row(ctx->frame, py + y) + px;
            for (int x = 0; x < 16; x++) row[x] = (uint8_t)br_u(br, 8);
        }
        int cpx = ctx->mb_x * 8, cpy = ctx->mb_y * 8;
        for (int y = 0; y < 8; y++) {
            uint8_t *row = frame_u_row(ctx->frame, cpy + y) + cpx;
            for (int x = 0; x < 8; x++) row[x] = (uint8_t)br_u(br, 8);
        }
        for (int y = 0; y < 8; y++) {
            uint8_t *row = frame_v_row(ctx->frame, cpy + y) + cpx;
            for (int x = 0; x < 8; x++) row[x] = (uint8_t)br_u(br, 8);
        }
        for (int i = 0; i < 24; i++) mb->nnz[i] = 16;
        mb->qp_y = 0;
        *qp_y = 0;
        if (br->error) {
            H264_LOG("I_PCM truncated at mb(%d,%d)\n", ctx->mb_x, ctx->mb_y);
            return 0;
        }
        result->ok = 1;
        return 1;
    }

    int is16x16 = mb_type_raw >= 1 && mb_type_raw <= 24;
    if (mb_type_raw > 25) {
        H264_LOG("mb_type=%u out of range at mb(%d,%d)\n",
                 mb_type_raw, ctx->mb_x, ctx->mb_y);
        result->unsupported = 1;
        return 1;
    }

    if (!is16x16) {
        /* I_NxN。transform_size_8x8_flag 在 Baseline 不会出现 */
        if (ctx->pps->transform8x8_mode) {
            H264_LOG("I_NxN + 8x8 transform not supported\n");
            result->unsupported = 1;
            return 1;
        }
        mb->type = H264_MBT_INTRA4X4;
        for (int blk = 0; blk < 16; blk++) {
            int bx = kBlk4x4X[blk], by = kBlk4x4Y[blk];
            int predicted =
                predict_intra4x4_mode(ctx, bx, by, left_avail, top_avail);
            int use_predicted = br_flag(br);
            int mode;
            if (use_predicted) {
                mode = predicted;
            } else {
                int rem = (int)br_u(br, 3);
                mode = (rem < predicted) ? rem : rem + 1;
            }
            mb->intra4x4_pred_mode[blk] = (uint8_t)mode;
        }
        mb->chroma_pred_mode = (uint8_t)br_ue(br);
    } else {
        mb->type = H264_MBT_INTRA16X16;
        uint32_t idx = mb_type_raw - 1;
        mb->intra16x16_pred_mode = (uint8_t)(idx % 4);
        mb->cbp_chroma = (uint8_t)((idx / 4) % 3);
        mb->cbp_luma = (idx >= 12) ? 15 : 0;
        mb->chroma_pred_mode = (uint8_t)br_ue(br);
    }

    if (!is16x16) {
        uint32_t cbp_code = br_ue(br);
        if (cbp_code > 47) {
            H264_LOG("invalid cbp %u at mb(%d,%d)\n", cbp_code, ctx->mb_x,
                     ctx->mb_y);
            return 0;
        }
        uint8_t cbp_combined = kCbpIntra4x4[cbp_code];
        mb->cbp_luma = cbp_combined & 0xF;
        mb->cbp_chroma = cbp_combined >> 4;
    }

    int dquant = 0;
    if (mb->cbp_luma != 0 || mb->cbp_chroma != 0 || is16x16) {
        dquant = br_se(br);
    }
    int qp = *qp_y + dquant;
    qp = ((qp + 52) % 52 + 52) % 52; /* 钳位回 0..51（8bit，QpBdOffset=0） */
    *qp_y = qp;
    mb->qp_y = (int8_t)qp;

    if (br->error) {
        H264_LOG("mb_qp_delta error at mb(%d,%d)\n", ctx->mb_x, ctx->mb_y);
        return 0;
    }

    /* --- 亮度残差 --- */
    if (is16x16) {
        predict_intra16x16(ctx->frame, ctx->mb_x, ctx->mb_y,
                           mb->intra16x16_pred_mode, left_avail, top_avail,
                           top_left_avail);

        int bxA = luma_neighbor_nnz(ctx, 0, 0, -1, 0);
        int bxB = luma_neighbor_nnz(ctx, 0, 0, 0, -1);
        int nCdc = predict_nc(bxA, bxB);
        int32_t dc_coeff[16];
        uint32_t dc_total_coeff = 0;
        if (!residual_block_cavlc(br, nCdc, 16, dc_coeff, &dc_total_coeff)) {
            H264_LOG("I16x16 luma DC CAVLC fail at mb(%d,%d)\n", ctx->mb_x,
                     ctx->mb_y);
            return 0;
        }
        int32_t dc_block[16] = {0};
        for (int k = 0; k < 16; k++) dc_block[kZigZag4x4[k]] = dc_coeff[k];
        hadamard4x4(dc_block);
        dequant_luma_dc4x4(dc_block, qp);

        for (int blk = 0; blk < 16; blk++) {
            int bx = kBlk4x4X[blk], by = kBlk4x4Y[blk];
            int32_t dc = dc_block[by * 4 + bx];
            if (mb->cbp_luma != 0) {
                if (!decode_luma_block_ac(br, ctx, blk, qp, dc)) return 0;
            } else {
                /* 无 AC：仍需把（可能非零的）DC 项加到预测上 */
                int32_t block[16] = {0};
                block[0] = dc;
                idct4x4(block);
                int px = ctx->mb_x * 16 + bx * 4, py = ctx->mb_y * 16 + by * 4;
                add_residual_4x4(frame_y_row(ctx->frame, py) + px,
                                 ctx->frame->stride_y, block);
                mb->nnz[blk] = 0;
            }
        }
    } else {
        for (int blk = 0; blk < 16; blk++) {
            int bx = kBlk4x4X[blk], by = kBlk4x4Y[blk];
            h264_neighbors4x4_t nb = gather_neighbors4x4(
                ctx->frame, ctx->mb_x * 16 + bx * 4, ctx->mb_y * 16 + by * 4,
                blk, left_avail, top_avail, top_left_avail, top_right_avail);
            predict_intra4x4(ctx->frame, ctx->mb_x * 16 + bx * 4,
                             ctx->mb_y * 16 + by * 4,
                             mb->intra4x4_pred_mode[blk], &nb);
            int quadrant = blk / 4;
            if (mb->cbp_luma & (1 << quadrant)) {
                if (!decode_luma_block_full(br, ctx, blk, qp)) return 0;
            } else {
                mb->nnz[blk] = 0;
            }
        }
    }

    /* --- 色率 --- */
    int c_qp = chroma_qp(qp, ctx->pps->chroma_qp_index_offset);
    int32_t cb_dc[4] = {0, 0, 0, 0};
    int32_t cr_dc[4] = {0, 0, 0, 0};
    if (mb->cbp_chroma >= 1) {
        for (int plane = 0; plane < 2; plane++) {
            int32_t coeff[4];
            uint32_t total_coeff = 0;
            if (!residual_block_cavlc(br, -1, 4, coeff, &total_coeff)) {
                H264_LOG("chroma DC CAVLC fail at mb(%d,%d) plane=%d\n",
                         ctx->mb_x, ctx->mb_y, plane);
                return 0;
            }
            int32_t *dst = plane == 0 ? cb_dc : cr_dc;
            dst[0] = coeff[0];
            dst[1] = coeff[1];
            dst[2] = coeff[2];
            dst[3] = coeff[3];
            hadamard2x2(dst);
            dequant_chroma_dc2x2(dst, c_qp);
        }
    }

    predict_intra_chroma(ctx->frame->u, ctx->frame->stride_c, ctx->mb_x * 8,
                         ctx->mb_y * 8, mb->chroma_pred_mode, left_avail,
                         top_avail, top_left_avail);
    predict_intra_chroma(ctx->frame->v, ctx->frame->stride_c, ctx->mb_x * 8,
                         ctx->mb_y * 8, mb->chroma_pred_mode, left_avail,
                         top_avail, top_left_avail);

    for (int plane = 0; plane < 2; plane++) {
        const int32_t *dc = plane == 0 ? cb_dc : cr_dc;
        for (int cy = 0; cy < 2; cy++) {
            for (int cx = 0; cx < 2; cx++) {
                int32_t dc_val = dc[cy * 2 + cx];
                if (mb->cbp_chroma >= 2) {
                    if (!decode_chroma_block_ac(br, ctx, plane, cx, cy, c_qp,
                                                dc_val))
                        return 0;
                } else {
                    int32_t block[16] = {0};
                    block[0] = dc_val;
                    idct4x4(block);
                    int px = ctx->mb_x * 8 + cx * 4, py = ctx->mb_y * 8 + cy * 4;
                    uint8_t *plane_buf =
                        plane == 0 ? ctx->frame->u : ctx->frame->v;
                    add_residual_4x4(
                        plane_buf + (size_t)py * ctx->frame->stride_c + px,
                        ctx->frame->stride_c, block);
                    mb->nnz[16 + plane * 4 + cy * 2 + cx] = 0;
                }
            }
        }
    }

    if (br->error) {
        H264_LOG("bitstream error at end of mb(%d,%d)\n", ctx->mb_x, ctx->mb_y);
        return 0;
    }
    result->ok = 1;
    return 1;
}

/* I slice 入口：先读 mb_type 本体 */
static int decode_mb_intra(h264_br_t *br, h264_mbctx_t *ctx, int *qp_y,
                           h264_mbresult_t *result) {
    uint32_t mb_type_raw = br_ue(br);
    if (mb_type_raw > 25 || br->error) {
        H264_LOG("invalid/truncated mb_type at mb(%d,%d)\n", ctx->mb_x,
                 ctx->mb_y);
        memset(result, 0, sizeof(*result));
        result->unsupported = 1;
        return 1;
    }
    return decode_mb_intra_with_type(br, ctx, mb_type_raw, qp_y, result);
}

/* ---- P slice（Inter） ---- */

/* 分区运动补偿（4x4 网格单位换算像素），亮度 + 两色率平面，预测直接
 * 写入当前帧，残差随后由调用方叠加 */
static void motion_compensate_partition(h264_mbctx_t *ctx, int bx, int by,
                                        int pw, int ph, int16_t mv_x,
                                        int16_t mv_y, int8_t ref_idx) {
    const h264_frame_t *ref = ctx->ref_list[ref_idx];
    int px = ctx->mb_x * 16 + bx * 4, py = ctx->mb_y * 16 + by * 4;
    int w = pw * 4, h = ph * 4;
    motion_comp_luma(frame_y_row(ctx->frame, py) + px, ctx->frame->stride_y,
                     ref, px, py, w, h, mv_x, mv_y);

    int cpx = ctx->mb_x * 8 + bx * 2, cpy = ctx->mb_y * 8 + by * 2;
    int cw = pw * 2, ch = ph * 2;
    motion_comp_chroma(frame_u_row(ctx->frame, cpy) + cpx, ctx->frame->stride_c,
                       ref->u, ref->stride_c, ref->width / 2, ref->height / 2,
                       cpx, cpy, cw, ch, mv_x, mv_y);
    motion_comp_chroma(frame_v_row(ctx->frame, cpy) + cpx, ctx->frame->stride_c,
                       ref->v, ref->stride_c, ref->width / 2, ref->height / 2,
                       cpx, cpy, cw, ch, mv_x, mv_y);
}

/* P_Skip 宏块（clause 8.4.1.1）：码流中无 macroblock_layer() 语法。
 * 恒用 refIdx 0；任一邻居不可用或 refIdx==0 且 MV==0 时走零 MV 特例
 * （而非通用中值预测）；整宏块运动补偿，无残差 */
static void decode_p_skip_mb(h264_mbctx_t *ctx, int qp_y) {
    h264_mbinfo_t *mb = mbtab_at(ctx->mb_info, ctx->mb_x, ctx->mb_y);
    memset(mb, 0, sizeof(*mb));
    mb->type = H264_MBT_P_SKIP;
    mb->qp_y = (int8_t)qp_y;

    h264_mbview_t view = {ctx->mb_info, ctx->mb_x, ctx->mb_y, ctx->slice_id};
    h264_mv_neighbor_t a = mv_neighbor_at(&view, -1, 0);
    h264_mv_neighbor_t b = mv_neighbor_at(&view, 0, -1);

    int16_t mv[2] = {0, 0};
    int zero_mv =
        (!a.available || !b.available)
            ? 1
            : ((a.inter_coded && a.ref_idx == 0 && a.mv[0] == 0 && a.mv[1] == 0) ||
               (b.inter_coded && b.ref_idx == 0 && b.mv[0] == 0 && b.mv[1] == 0));
    if (!zero_mv) {
        predict_mv_general(&view, 0, 0, 4, 4, 0, -1, mv);
    }

    fill_partition_mv(mb, 0, 0, 4, 4, mv[0], mv[1], 0);
    motion_compensate_partition(ctx, 0, 0, 4, 4, mv[0], mv[1], 0);
}

/* mvd_l0：两个 se(v)，先 x 后 y */
static int decode_mvd(h264_br_t *br, int16_t *mvd) {
    int32_t x = br_se(br);
    int32_t y = br_se(br);
    if (br->error) return 0;
    mvd[0] = (int16_t)x;
    mvd[1] = (int16_t)y;
    return 1;
}

/* ref_idx_l0 的 te(v)（cMax = numActiveRefs-1），与 ffmpeg h264_cavlc.c
 * 交叉核对：numActiveRefs<=1 时只有一个合法值且完全不读位（所以本函数
 * 可被无条件调用）；==2 时是单个反相比特（cMax==1 特例：值 = !u(1)，
 * 不是普通 u(1)）；>2 才是普通越界检查的 ue(v) */
static int decode_ref_idx(h264_br_t *br, int num_active_refs,
                          int8_t *out_ref_idx) {
    if (num_active_refs <= 1) {
        *out_ref_idx = 0;
        return 1;
    }
    if (num_active_refs == 2) {
        *out_ref_idx = (int8_t)(br_flag(br) ? 0 : 1);
        return !br->error;
    }
    uint32_t v = br_ue(br);
    if (br->error || (int)v >= num_active_refs) return 0;
    *out_ref_idx = (int8_t)v;
    return 1;
}

typedef struct {
    int bx, by, pw, ph, dir_side;
    int8_t ref_idx;
} h264_part_t;

/* 一个 Inter(P) 宏块的完整 macroblock_layer()。语法顺序（与 ffmpeg
 * h264_cavlc.c 交叉核对）：先读全部 ref_idx_l0（每顶层分区一个，或
 * P_8x8 每 8x8 象限一个、象限内子分区共享），后读全部 mvd_l0——两者
 * 不按分区交错 */
static int decode_mb_inter(h264_br_t *br, h264_mbctx_t *ctx, int *qp_y,
                           h264_mbresult_t *result) {
    memset(result, 0, sizeof(*result));
    h264_mbinfo_t *mb = mbtab_at(ctx->mb_info, ctx->mb_x, ctx->mb_y);
    memset(mb, 0, sizeof(*mb));
    mb->type = H264_MBT_INTER;

    uint32_t mb_type_raw = br_ue(br);
    if (mb_type_raw >= 5) {
        /* P slice 内 Intra 宏块：I-slice mb_type 编号 +5 */
        return decode_mb_intra_with_type(br, ctx, mb_type_raw - 5, qp_y,
                                         result);
    }

    h264_part_t parts[16]; /* P_8x8x4x4 最多 16 个 4x4 分区 */
    int num_parts = 0;

    if (mb_type_raw == H264_P_L0_16x16) {
        int8_t r;
        if (!decode_ref_idx(br, ctx->num_active_refs, &r)) return 0;
        parts[num_parts].bx = 0; parts[num_parts].by = 0;
        parts[num_parts].pw = 4; parts[num_parts].ph = 4;
        parts[num_parts].dir_side = -1; parts[num_parts].ref_idx = r;
        num_parts++;
    } else if (mb_type_raw == H264_P_L0_L0_16x8) {
        int8_t r0, r1;
        if (!decode_ref_idx(br, ctx->num_active_refs, &r0) ||
            !decode_ref_idx(br, ctx->num_active_refs, &r1))
            return 0;
        parts[num_parts].bx = 0; parts[num_parts].by = 0;
        parts[num_parts].pw = 4; parts[num_parts].ph = 2;
        parts[num_parts].dir_side = 1; parts[num_parts].ref_idx = r0;
        num_parts++;
        parts[num_parts].bx = 0; parts[num_parts].by = 2;
        parts[num_parts].pw = 4; parts[num_parts].ph = 2;
        parts[num_parts].dir_side = 0; parts[num_parts].ref_idx = r1;
        num_parts++;
    } else if (mb_type_raw == H264_P_L0_L0_8x16) {
        int8_t r0, r1;
        if (!decode_ref_idx(br, ctx->num_active_refs, &r0) ||
            !decode_ref_idx(br, ctx->num_active_refs, &r1))
            return 0;
        parts[num_parts].bx = 0; parts[num_parts].by = 0;
        parts[num_parts].pw = 2; parts[num_parts].ph = 4;
        parts[num_parts].dir_side = 0; parts[num_parts].ref_idx = r0;
        num_parts++;
        parts[num_parts].bx = 2; parts[num_parts].by = 0;
        parts[num_parts].pw = 2; parts[num_parts].ph = 4;
        parts[num_parts].dir_side = 2; parts[num_parts].ref_idx = r1;
        num_parts++;
    } else {
        /* P_8x8 / P_8x8ref0：四个 8x8 象限各带 sub_mb_type。
         * P_8x8ref0 永不编码 ref_idx_l0（恒 0，与 numActiveRefs 无关） */
        uint32_t sub_type[4];
        for (int i = 0; i < 4; i++) sub_type[i] = br_ue(br);
        if (br->error) return 0;

        int8_t quad_ref_idx[4];
        for (int q = 0; q < 4; q++) {
            if (mb_type_raw == H264_P_8x8ref0) {
                quad_ref_idx[q] = 0;
            } else if (!decode_ref_idx(br, ctx->num_active_refs,
                                       &quad_ref_idx[q])) {
                return 0;
            }
        }

        static const int quad_x[4] = {0, 2, 0, 2};
        static const int quad_y[4] = {0, 0, 2, 2};
        for (int q = 0; q < 4; q++) {
            int qx = quad_x[q], qy = quad_y[q];
            int8_t r = quad_ref_idx[q];
            switch (sub_type[q]) {
                case H264_SUB_8x8:
                    parts[num_parts].bx = qx; parts[num_parts].by = qy;
                    parts[num_parts].pw = 2; parts[num_parts].ph = 2;
                    parts[num_parts].dir_side = -1; parts[num_parts].ref_idx = r;
                    num_parts++;
                    break;
                case H264_SUB_8x4:
                    parts[num_parts].bx = qx; parts[num_parts].by = qy;
                    parts[num_parts].pw = 2; parts[num_parts].ph = 1;
                    parts[num_parts].dir_side = -1; parts[num_parts].ref_idx = r;
                    num_parts++;
                    parts[num_parts].bx = qx; parts[num_parts].by = qy + 1;
                    parts[num_parts].pw = 2; parts[num_parts].ph = 1;
                    parts[num_parts].dir_side = -1; parts[num_parts].ref_idx = r;
                    num_parts++;
                    break;
                case H264_SUB_4x8:
                    parts[num_parts].bx = qx; parts[num_parts].by = qy;
                    parts[num_parts].pw = 1; parts[num_parts].ph = 2;
                    parts[num_parts].dir_side = -1; parts[num_parts].ref_idx = r;
                    num_parts++;
                    parts[num_parts].bx = qx + 1; parts[num_parts].by = qy;
                    parts[num_parts].pw = 1; parts[num_parts].ph = 2;
                    parts[num_parts].dir_side = -1; parts[num_parts].ref_idx = r;
                    num_parts++;
                    break;
                case H264_SUB_4x4:
                default:
                    parts[num_parts].bx = qx; parts[num_parts].by = qy;
                    parts[num_parts].pw = 1; parts[num_parts].ph = 1;
                    parts[num_parts].dir_side = -1; parts[num_parts].ref_idx = r;
                    num_parts++;
                    parts[num_parts].bx = qx + 1; parts[num_parts].by = qy;
                    parts[num_parts].pw = 1; parts[num_parts].ph = 1;
                    parts[num_parts].dir_side = -1; parts[num_parts].ref_idx = r;
                    num_parts++;
                    parts[num_parts].bx = qx; parts[num_parts].by = qy + 1;
                    parts[num_parts].pw = 1; parts[num_parts].ph = 1;
                    parts[num_parts].dir_side = -1; parts[num_parts].ref_idx = r;
                    num_parts++;
                    parts[num_parts].bx = qx + 1; parts[num_parts].by = qy + 1;
                    parts[num_parts].pw = 1; parts[num_parts].ph = 1;
                    parts[num_parts].dir_side = -1; parts[num_parts].ref_idx = r;
                    num_parts++;
                    break;
            }
        }
    }

    h264_mbview_t view = {ctx->mb_info, ctx->mb_x, ctx->mb_y, ctx->slice_id};
    for (int i = 0; i < num_parts; i++) {
        int16_t pred[2], mvd[2];
        predict_mv_general(&view, parts[i].bx, parts[i].by, parts[i].pw,
                           parts[i].ph, parts[i].ref_idx, parts[i].dir_side,
                           pred);
        if (!decode_mvd(br, mvd)) return 0;
        int16_t mv[2] = {(int16_t)(pred[0] + mvd[0]),
                         (int16_t)(pred[1] + mvd[1])};
        fill_partition_mv(mb, parts[i].bx, parts[i].by, parts[i].pw,
                          parts[i].ph, mv[0], mv[1], parts[i].ref_idx);
        motion_compensate_partition(ctx, parts[i].bx, parts[i].by,
                                    parts[i].pw, parts[i].ph, mv[0], mv[1],
                                    parts[i].ref_idx);
    }

    /* --- CBP / QP / 残差（镜像 Intra 路径，无 I_16x16 DC 块） --- */
    uint32_t cbp_code = br_ue(br);
    if (cbp_code > 47 || br->error) {
        H264_LOG("invalid cbp %u at mb(%d,%d)\n", cbp_code, ctx->mb_x,
                 ctx->mb_y);
        return 0;
    }
    uint8_t cbp_combined = kCbpInter[cbp_code];
    mb->cbp_luma = cbp_combined & 0xF;
    mb->cbp_chroma = cbp_combined >> 4;

    int dquant = 0;
    if (mb->cbp_luma != 0 || mb->cbp_chroma != 0) {
        dquant = br_se(br);
    }
    int qp = *qp_y + dquant;
    qp = ((qp + 52) % 52 + 52) % 52;
    *qp_y = qp;
    mb->qp_y = (int8_t)qp;
    if (br->error) return 0;

    for (int blk = 0; blk < 16; blk++) {
        int quadrant = blk / 4;
        if (mb->cbp_luma & (1 << quadrant)) {
            if (!decode_luma_block_full(br, ctx, blk, qp)) return 0;
        } else {
            mb->nnz[blk] = 0;
        }
    }

    int c_qp = chroma_qp(qp, ctx->pps->chroma_qp_index_offset);
    int32_t cb_dc[4] = {0, 0, 0, 0};
    int32_t cr_dc[4] = {0, 0, 0, 0};
    if (mb->cbp_chroma >= 1) {
        for (int plane = 0; plane < 2; plane++) {
            int32_t coeff[4];
            uint32_t total_coeff = 0;
            if (!residual_block_cavlc(br, -1, 4, coeff, &total_coeff)) return 0;
            int32_t *dst = plane == 0 ? cb_dc : cr_dc;
            dst[0] = coeff[0];
            dst[1] = coeff[1];
            dst[2] = coeff[2];
            dst[3] = coeff[3];
            hadamard2x2(dst);
            dequant_chroma_dc2x2(dst, c_qp);
        }
    }
    for (int plane = 0; plane < 2; plane++) {
        const int32_t *dc = plane == 0 ? cb_dc : cr_dc;
        for (int cy = 0; cy < 2; cy++) {
            for (int cx = 0; cx < 2; cx++) {
                int32_t dc_val = dc[cy * 2 + cx];
                if (mb->cbp_chroma >= 2) {
                    if (!decode_chroma_block_ac(br, ctx, plane, cx, cy, c_qp,
                                                dc_val))
                        return 0;
                } else {
                    int32_t block[16] = {0};
                    block[0] = dc_val;
                    idct4x4(block);
                    int px = ctx->mb_x * 8 + cx * 4, py = ctx->mb_y * 8 + cy * 4;
                    uint8_t *plane_buf =
                        plane == 0 ? ctx->frame->u : ctx->frame->v;
                    add_residual_4x4(
                        plane_buf + (size_t)py * ctx->frame->stride_c + px,
                        ctx->frame->stride_c, block);
                    mb->nnz[16 + plane * 4 + cy * 2 + cx] = 0;
                }
            }
        }
    }

    if (br->error) return 0;
    result->ok = 1;
    return 1;
}

/* ================================================================== */
/* 环路去块滤波（上游 h264_deblock.h，clause 8.7）                      */
/* ================================================================== */

static int clip3(int lo, int hi, int v) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static int iabs(int v) { return v < 0 ? -v : v; }

/* 一行亮度采样的边滤波（pix[-step]=p0, pix[0]=q0）。bS<4（clause
 * 8.7.2.3）与 bS==4 强滤波（8.7.2.4）；含 p1/p2 的 ap/aq 扩展条件 */
static void filter_edge_luma(uint8_t *pix, int step, int bS, int alpha,
                             int beta, int tc0) {
    int p0 = pix[-1 * step], p1 = pix[-2 * step], p2 = pix[-3 * step];
    int q0 = pix[0], q1 = pix[1 * step], q2 = pix[2 * step];
    if (iabs(p0 - q0) >= alpha || iabs(p1 - p0) >= beta ||
        iabs(q1 - q0) >= beta)
        return;

    if (bS < 4) {
        int ap = iabs(p2 - p0) < beta;
        int aq = iabs(q2 - q0) < beta;
        int tc = tc0 + (ap ? 1 : 0) + (aq ? 1 : 0);
        int delta = clip3(-tc, tc, (((q0 - p0) * 4) + (p1 - q1) + 4) >> 3);
        pix[-1 * step] = clip255(p0 + delta);
        pix[0] = clip255(q0 - delta);
        if (ap) {
            pix[-2 * step] = (uint8_t)(p1 + clip3(-tc0, tc0,
                                                  (p2 + ((p0 + q0 + 1) >> 1) -
                                                   2 * p1) >> 1));
        }
        if (aq) {
            pix[1 * step] = (uint8_t)(q1 + clip3(-tc0, tc0,
                                                 (q2 + ((p0 + q0 + 1) >> 1) -
                                                  2 * q1) >> 1));
        }
    } else {
        int ap = iabs(p2 - p0) < beta;
        int aq = iabs(q2 - q0) < beta;
        int strong_cond = iabs(p0 - q0) < ((alpha >> 2) + 2);
        if (ap && strong_cond) {
            int p3 = pix[-4 * step];
            pix[-1 * step] = (uint8_t)((p2 + 2 * p1 + 2 * p0 + 2 * q0 + q1 + 4) >> 3);
            pix[-2 * step] = (uint8_t)((p2 + p1 + p0 + q0 + 2) >> 2);
            pix[-3 * step] = (uint8_t)((2 * p3 + 3 * p2 + p1 + p0 + q0 + 4) >> 3);
        } else {
            pix[-1 * step] = (uint8_t)((2 * p1 + p0 + q1 + 2) >> 2);
        }
        if (aq && strong_cond) {
            int q3 = pix[3 * step];
            pix[0] = (uint8_t)((q2 + 2 * q1 + 2 * q0 + 2 * p0 + p1 + 4) >> 3);
            pix[1 * step] = (uint8_t)((q2 + q1 + q0 + p0 + 2) >> 2);
            pix[2 * step] = (uint8_t)((2 * q3 + 3 * q2 + q1 + q0 + p0 + 4) >> 3);
        } else {
            pix[0] = (uint8_t)((2 * q1 + q0 + p1 + 2) >> 2);
        }
    }
}

/* 一行色率采样：只改 p0/q0，无 ap/aq 扩展；bS==4 恒用简单二均值 */
static void filter_edge_chroma(uint8_t *pix, int step, int bS, int alpha,
                               int beta, int tc0) {
    int p0 = pix[-1 * step], p1 = pix[-2 * step];
    int q0 = pix[0], q1 = pix[1 * step];
    if (iabs(p0 - q0) >= alpha || iabs(p1 - p0) >= beta ||
        iabs(q1 - q0) >= beta)
        return;

    if (bS < 4) {
        int tc = tc0 + 1;
        int delta = clip3(-tc, tc, (((q0 - p0) * 4) + (p1 - q1) + 4) >> 3);
        pix[-1 * step] = clip255(p0 + delta);
        pix[0] = clip255(q0 - delta);
    } else {
        pix[-1 * step] = (uint8_t)((2 * p1 + p0 + q1 + 2) >> 2);
        pix[0] = (uint8_t)((2 * q1 + q0 + p1 + 2) >> 2);
    }
}

/* 两个 4x4 亮度块间的边界强度 bS（clause 8.7.2.1）。直接比较 refIdx
 * 是精确而非近似：本解码器不支持列表重排，同一幅图内 refIdx N 恒映射
 * 同一参考图。色率边复用同位置亮度块的 bS（8.7.2.1 由亮度属性推导） */
static int boundary_strength(const h264_mbinfo_t *p, int p_blk,
                             const h264_mbinfo_t *q, int q_blk,
                             int is_mb_edge) {
    if (mbinfo_is_intra(p) || mbinfo_is_intra(q)) return is_mb_edge ? 4 : 3;
    if (p->nnz[p_blk] > 0 || q->nnz[q_blk] > 0) return 2;
    if (p->ref_idx[p_blk] != q->ref_idx[q_blk]) return 1;
    int dx = p->mv[p_blk][0] - q->mv[q_blk][0];
    int dy = p->mv[p_blk][1] - q->mv[q_blk][1];
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    return (dx >= 4 || dy >= 4) ? 1 : 0;
}

/* disable_deblocking_filter_idc 的宏块边界例外判定 */
static int mb_edge_filterable(const h264_mbinfo_t *mb, int mb_slice_id,
                              const h264_mbtab_t *mb_info, int neighbor_x,
                              int neighbor_y) {
    if (mb->disable_deblock_idc == 1) return 0;
    if (mb->disable_deblock_idc == 2) {
        return mbtab_slice_id_at(mb_info, neighbor_x, neighbor_y) == mb_slice_id;
    }
    return 1;
}

/* 整幅图像去块（外层循环：宏块光栅序，先左/上边，先垂直后水平边，
 * 先亮度后色率）。必须在图像全部 slice 重建完成后调用 */
static void deblock_picture(h264_frame_t *frame, h264_mbtab_t *mb_info,
                            const h264_pps_t *pps) {
    int mb_w = mb_info->mb_w, mb_h = mb_info->mb_h;

    for (int mb_y = 0; mb_y < mb_h; mb_y++) {
        for (int mb_x = 0; mb_x < mb_w; mb_x++) {
            h264_mbinfo_t *mb = mbtab_at(mb_info, mb_x, mb_y);
            if (mb->disable_deblock_idc == 1) continue;
            int my_slice_id = mbtab_slice_id_at(mb_info, mb_x, mb_y);

            int alpha_off = mb->alpha_c0_off_div2 * 2;
            int beta_off = mb->beta_off_div2 * 2;
            int qp = mb->qp_y;
            int cqp = chroma_qp(qp, pps->chroma_qp_index_offset);

            int left_edge_ok =
                mb_x > 0 &&
                mb_edge_filterable(mb, my_slice_id, mb_info, mb_x - 1, mb_y);
            int top_edge_ok =
                mb_y > 0 &&
                mb_edge_filterable(mb, my_slice_id, mb_info, mb_x, mb_y - 1);

            int px0 = mb_x * 16, py0 = mb_y * 16;

            /* --- 亮度垂直边（step=1）；bS 按每 4 行组独立计算 --- */
            for (int edge = 0; edge < 4; edge++) {
                int is_mb_edge = (edge == 0);
                if (is_mb_edge && !left_edge_ok) continue;
                int ex = px0 + edge * 4;
                const h264_mbinfo_t *p_mb =
                    is_mb_edge ? mbtab_at_c(mb_info, mb_x - 1, mb_y) : mb;
                int qp_neighbor = is_mb_edge ? p_mb->qp_y : qp;
                int qp_avg = (qp + qp_neighbor + 1) >> 1;
                int index_a = clip3(0, 51, qp_avg + alpha_off);
                int index_b = clip3(0, 51, qp_avg + beta_off);
                int alpha = kDeblockAlpha[index_a];
                int beta = kDeblockBeta[index_b];
                if (alpha == 0) continue;
                for (int g = 0; g < 4; g++) {
                    int q_blk = kInvBlk4x4[g][edge];
                    int p_blk =
                        is_mb_edge ? kInvBlk4x4[g][3] : kInvBlk4x4[g][edge - 1];
                    int bS = boundary_strength(p_mb, p_blk, mb, q_blk, is_mb_edge);
                    if (bS == 0) continue;
                    int tc0 = kDeblockTc0[index_a][bS < 4 ? bS : 0];
                    for (int y = g * 4; y < g * 4 + 4; y++) {
                        filter_edge_luma(frame_y_row(frame, py0 + y) + ex, 1,
                                         bS, alpha, beta, tc0);
                    }
                }
            }
            /* --- 亮度水平边（step=stride_y 沿列向下） --- */
            for (int edge = 0; edge < 4; edge++) {
                int is_mb_edge = (edge == 0);
                if (is_mb_edge && !top_edge_ok) continue;
                int ey = py0 + edge * 4;
                const h264_mbinfo_t *p_mb =
                    is_mb_edge ? mbtab_at_c(mb_info, mb_x, mb_y - 1) : mb;
                int qp_neighbor = is_mb_edge ? p_mb->qp_y : qp;
                int qp_avg = (qp + qp_neighbor + 1) >> 1;
                int index_a = clip3(0, 51, qp_avg + alpha_off);
                int index_b = clip3(0, 51, qp_avg + beta_off);
                int alpha = kDeblockAlpha[index_a];
                int beta = kDeblockBeta[index_b];
                if (alpha == 0) continue;
                for (int g = 0; g < 4; g++) {
                    int q_blk = kInvBlk4x4[edge][g];
                    int p_blk =
                        is_mb_edge ? kInvBlk4x4[3][g] : kInvBlk4x4[edge - 1][g];
                    int bS = boundary_strength(p_mb, p_blk, mb, q_blk, is_mb_edge);
                    if (bS == 0) continue;
                    int tc0 = kDeblockTc0[index_a][bS < 4 ? bS : 0];
                    for (int x = g * 4; x < g * 4 + 4; x++) {
                        filter_edge_luma(frame_y_row(frame, ey) + px0 + x,
                                         frame->stride_y, bS, alpha, beta, tc0);
                    }
                }
            }

            /* --- 色率：宏块边 + 每方向一条内部边（4:2:0）。bS 用同位置
             * 亮度块对推导；亮度 4 组 bS 与 2 采样色率段一一对应（全部
             * 4 组都用），QP 用色率 QP --- */
            int cpx0 = mb_x * 8, cpy0 = mb_y * 8;
            for (int edge = 0; edge < 2; edge++) {
                int is_mb_edge = (edge == 0);
                if (is_mb_edge && !left_edge_ok) continue;
                int ex = cpx0 + edge * 4;
                int luma_edge = edge * 2;
                const h264_mbinfo_t *p_mb =
                    is_mb_edge ? mbtab_at_c(mb_info, mb_x - 1, mb_y) : mb;
                int qp_neighbor =
                    is_mb_edge
                        ? chroma_qp(p_mb->qp_y, pps->chroma_qp_index_offset)
                        : cqp;
                int qp_avg = (cqp + qp_neighbor + 1) >> 1;
                int index_a = clip3(0, 51, qp_avg + alpha_off);
                int index_b = clip3(0, 51, qp_avg + beta_off);
                int alpha = kDeblockAlpha[index_a];
                int beta = kDeblockBeta[index_b];
                if (alpha == 0) continue;
                for (int g = 0; g < 4; g++) {
                    int q_blk = kInvBlk4x4[g][luma_edge];
                    int p_blk = is_mb_edge ? kInvBlk4x4[g][3]
                                           : kInvBlk4x4[g][luma_edge - 1];
                    int bS = boundary_strength(p_mb, p_blk, mb, q_blk, is_mb_edge);
                    if (bS == 0) continue;
                    int tc0 = kDeblockTc0[index_a][bS < 4 ? bS : 0];
                    for (int plane = 0; plane < 2; plane++) {
                        uint8_t *base = plane == 0 ? frame->u : frame->v;
                        for (int y = g * 2; y < g * 2 + 2; y++) {
                            filter_edge_chroma(
                                base + (size_t)(cpy0 + y) * frame->stride_c + ex,
                                1, bS, alpha, beta, tc0);
                        }
                    }
                }
            }
            for (int edge = 0; edge < 2; edge++) {
                int is_mb_edge = (edge == 0);
                if (is_mb_edge && !top_edge_ok) continue;
                int ey = cpy0 + edge * 4;
                int luma_edge = edge * 2;
                const h264_mbinfo_t *p_mb =
                    is_mb_edge ? mbtab_at_c(mb_info, mb_x, mb_y - 1) : mb;
                int qp_neighbor =
                    is_mb_edge
                        ? chroma_qp(p_mb->qp_y, pps->chroma_qp_index_offset)
                        : cqp;
                int qp_avg = (cqp + qp_neighbor + 1) >> 1;
                int index_a = clip3(0, 51, qp_avg + alpha_off);
                int index_b = clip3(0, 51, qp_avg + beta_off);
                int alpha = kDeblockAlpha[index_a];
                int beta = kDeblockBeta[index_b];
                if (alpha == 0) continue;
                for (int g = 0; g < 4; g++) {
                    int q_blk = kInvBlk4x4[luma_edge][g];
                    int p_blk = is_mb_edge ? kInvBlk4x4[3][g]
                                           : kInvBlk4x4[luma_edge - 1][g];
                    int bS = boundary_strength(p_mb, p_blk, mb, q_blk, is_mb_edge);
                    if (bS == 0) continue;
                    int tc0 = kDeblockTc0[index_a][bS < 4 ? bS : 0];
                    for (int plane = 0; plane < 2; plane++) {
                        uint8_t *base = plane == 0 ? frame->u : frame->v;
                        for (int x = g * 2; x < g * 2 + 2; x++) {
                            filter_edge_chroma(
                                base + (size_t)ey * frame->stride_c + cpx0 + x,
                                frame->stride_c, bS, alpha, beta, tc0);
                        }
                    }
                }
            }
        }
    }
}

/* ================================================================== */
/* 解码器顶层（上游 h264_decoder.h）：NAL 分发 -> SPS/PPS 缓存 ->       */
/* slice 解码 -> 去块 -> 滑动窗参考帧管理                               */
/* ================================================================== */

struct h264_dec {
    uint8_t *nal_scratch; /* H264_DEC_MAX_NAL，EP 剥离暂存 */
    h264_nal_rd_t nal_rd;

    /* 流式输入（h264_dec_stream_write）：跨 chunk 的不完整 NAL 残余。
     * pend 指向 nal_scratch 尾部另开的 H264_DEC_MAX_NAL 区域，两部分
     * 分开避免与逐 NAL 解析的 scratch 相互干扰。 */
    uint8_t *pend;
    size_t pend_len;

    h264_sps_t sps_tab[H264_MAX_SPS];
    h264_sps_t cur_sps;   /* 最近解码 slice 使用的 SPS */
    h264_pps_t pps_tab[H264_MAX_PPS];
    int have_sps, have_pps;
    int input_exhausted;

    h264_mbtab_t mb_tab;
    h264_frame_t cur;
    h264_frame_t refs[H264_DEC_MAX_REFS]; /* 0 = 最近，滑动窗维护 */
    int ref_cnt;
    int max_refs;
    int slice_cnt;
    int max_w, max_h;
};

/* 本宏块的 slice 级去块参数存进其元数据（去块在全图完成后跨多 slice
 * 运行，需逐宏块参数） */
static void tag_deblock_params(h264_mbctx_t *ctx, const h264_sh_t *sh) {
    h264_mbinfo_t *mb = mbtab_at(ctx->mb_info, ctx->mb_x, ctx->mb_y);
    mb->disable_deblock_idc = (uint8_t)sh->disable_deblocking_filter_idc;
    mb->alpha_c0_off_div2 = (int8_t)sh->slice_alpha_c0_offset_div2;
    mb->beta_off_div2 = (int8_t)sh->slice_beta_offset_div2;
}

/* 一个 VCL slice NAL 的解析与解码（clause 7.3.3/7.3.4）。slice 首块
 * (first_mb_in_slice==0) 时重置逐宏块元数据并按 SPS 设定帧尺寸；
 * 图像最后一个宏块解码完成（pictureComplete）时跑去块并快照进参考帧 */
static h264_dec_status_t dec_decode_slice(struct h264_dec *d,
                                          const h264_nal_t *nal) {
    if (!d->have_sps || !d->have_pps) {
        H264_LOG("slice before SPS/PPS\n");
        return H264_DEC_ERROR;
    }

    h264_br_t br;
    br_init(&br, nal->rbsp, nal->rbsp_size);
    /* 先窥视 pps_id 不提交：slice 头要按 id 查 SPS/PPS（clause 7.3.3
     * 中 first_mb_in_slice/slice_type/pps_id 先于一切依赖项） */
    uint32_t first_mb = br_ue(&br);
    uint32_t slice_type_raw = br_ue(&br);
    uint32_t pps_id = br_ue(&br);
    (void)first_mb;
    (void)slice_type_raw;

    const h264_pps_t *pps = &d->pps_tab[pps_id % H264_MAX_PPS];
    if (!pps->valid || pps->id != pps_id) {
        H264_LOG("PPS id=%u not cached\n", pps_id);
        return H264_DEC_ERROR;
    }
    const h264_sps_t *sps = &d->sps_tab[pps->sps_id % H264_MAX_SPS];
    if (!sps->valid || sps->id != pps->sps_id) {
        H264_LOG("SPS id=%u not cached\n", pps->sps_id);
        return H264_DEC_ERROR;
    }
    d->cur_sps = *sps; /* 供 h264_dec_time_scale() 等查询 */

    /* SPS/PPS 已知后从头重新解析（parse_slice_header 需全新 reader） */
    h264_br_t br2;
    br_init(&br2, nal->rbsp, nal->rbsp_size);
    h264_sh_t sh;
    if (!parse_slice_header(&br2, nal->type, nal->ref_idc, sps, pps, &sh)) {
        H264_LOG("slice header parse failed\n");
        return H264_DEC_ERROR;
    }
    if (sh.unsupported) {
        H264_LOG("slice header uses unsupported feature\n");
        return H264_DEC_UNSUPPORTED;
    }
    int num_active_refs = (int)sh.num_ref_idx_l0_active_minus1 + 1;
    if (sh.slice_type == H264_SLICE_P) {
        if (d->ref_cnt == 0) {
            H264_LOG("P slice with no reference pictures\n");
            return H264_DEC_ERROR;
        }
        /* 检查顺序有意如此：ref_cnt 永不超过 max_refs（滑动窗插入已封
         * 顶），把"超上限"放在"超现有数"之后将永远不可达，超限流会被
         * 误报为损坏而非"不支持" */
        if (num_active_refs > d->max_refs) {
            H264_LOG("stream wants %d refs, exceeds max %d\n",
                     num_active_refs, d->max_refs);
            return H264_DEC_UNSUPPORTED;
        }
        if (num_active_refs > d->ref_cnt) {
            H264_LOG("stream wants %d refs, only %d exist\n",
                     num_active_refs, d->ref_cnt);
            return H264_DEC_ERROR;
        }
    }

    if (sh.first_mb_in_slice == 0) {
        if (!mbtab_reset(&d->mb_tab, (int)sps->pic_width_in_mbs,
                         (int)sps->pic_height_in_mbs)) {
            return H264_DEC_NOMEM;
        }
        if (!frame_set_size(&d->cur, (int)sps->coded_width,
                            (int)sps->coded_height)) {
            return H264_DEC_NOMEM;
        }
        d->slice_cnt = 0;
    }
    int slice_id = d->slice_cnt++;

    int mb_w = (int)sps->pic_width_in_mbs;
    int mb_h = (int)sps->pic_height_in_mbs;
    int total_mbs = mb_w * mb_h;
    int qp_y = (int)sh.slice_qp;

    h264_mbctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.frame = &d->cur;
    if (sh.slice_type == H264_SLICE_P) {
        /* 默认参考列表（clause 8.2.4.2）：refs[] 本就按最新在前维护，
         * 活动列表就是前 num_active_refs 项，无需单独构造/排序 */
        ctx.num_active_refs = num_active_refs;
        for (int i = 0; i < num_active_refs; i++)
            ctx.ref_list[i] = &d->refs[i];
    }
    ctx.mb_info = &d->mb_tab;
    ctx.sps = sps;
    ctx.pps = pps;
    ctx.slice_id = slice_id;

    int mb_addr = (int)sh.first_mb_in_slice;
    while (mb_addr < total_mbs) {
        if (sh.slice_type == H264_SLICE_P) {
            uint32_t skip_run = br_ue(&br2);
            if (br2.error) {
                H264_LOG("mb_skip_run read error near mb(%d,%d)\n",
                         mb_addr % mb_w, mb_addr / mb_w);
                return H264_DEC_ERROR;
            }
            for (uint32_t s = 0; s < skip_run && mb_addr < total_mbs; s++) {
                ctx.mb_x = mb_addr % mb_w;
                ctx.mb_y = mb_addr / mb_w;
                mbtab_begin_mb(&d->mb_tab, ctx.mb_x, ctx.mb_y, slice_id);
                decode_p_skip_mb(&ctx, qp_y);
                tag_deblock_params(&ctx, &sh);
                mb_addr++;
            }
            if (mb_addr >= total_mbs) break;
            if (skip_run > 0 && !br_more_rbsp_data(&br2)) break;
        }

        ctx.mb_x = mb_addr % mb_w;
        ctx.mb_y = mb_addr / mb_w;
        mbtab_begin_mb(&d->mb_tab, ctx.mb_x, ctx.mb_y, slice_id);

        h264_mbresult_t result;
        int ok = (sh.slice_type == H264_SLICE_P)
                     ? decode_mb_inter(&br2, &ctx, &qp_y, &result)
                     : decode_mb_intra(&br2, &ctx, &qp_y, &result);
        if (!ok) {
            H264_LOG("mb decode failed at mb(%d,%d)\n", ctx.mb_x, ctx.mb_y);
            return H264_DEC_ERROR;
        }
        if (result.unsupported) {
            H264_LOG("mb(%d,%d) uses unsupported feature\n", ctx.mb_x, ctx.mb_y);
            return H264_DEC_UNSUPPORTED;
        }
        tag_deblock_params(&ctx, &sh);

        mb_addr++;
        if (mb_addr >= total_mbs) break;
        if (!br_more_rbsp_data(&br2)) break;
    }

    int picture_complete = (mb_addr >= total_mbs);
    if (picture_complete) {
        deblock_picture(&d->cur, &d->mb_tab, pps);
        d->cur.frame_num = sh.frame_num;
        d->cur.is_ref = (nal->ref_idc != 0);
        if (d->cur.is_ref) {
            /* 滑动窗标记（clause 8.2.5.3 默认过程）：插到 index 0，旧帧
             * 后移；已达上限则先逐出最旧。用结构体交换（指针 O(1)），
             * 仅新条目 (refs[0] <- cur) 需要一次真实拷贝 */
            int keep = d->ref_cnt < d->max_refs ? d->ref_cnt
                                                : d->max_refs - 1;
            for (int i = keep; i > 0; i--) {
                frame_swap(&d->refs[i], &d->refs[i - 1]);
            }
            /* copyFrom 只会在某 ref 槽首次填充时失败（此后存储保留），
             * 失败按 NOMEM 上报：静默交出当前帧但未存参考，会让后续
             * P slice 引用一个从未保存的图 */
            if (!frame_copy_from(&d->refs[0], &d->cur)) {
                H264_LOG("ref copy failed - picture decoded but not saved\n");
                return H264_DEC_NOMEM;
            }
            d->ref_cnt = keep + 1;
        }
        return H264_DEC_OK;
    }
    return H264_DEC_NEED_MORE; /* 本图像还有后续 slice */
}

/* ================================================================== */
/* 公开 API                                                            */
/* ================================================================== */

h264_dec_t *h264_dec_create(int max_width, int max_height) {
    if (max_width <= 0 || max_height <= 0) return NULL;
    struct h264_dec *d =
        (struct h264_dec *)H264_DEC_ALLOC(sizeof(struct h264_dec));
    if (!d) return NULL;
    memset(d, 0, sizeof(*d));

    d->nal_scratch = (uint8_t *)H264_DEC_ALLOC(H264_DEC_MAX_NAL);
    if (!d->nal_scratch) {
        H264_DEC_FREE(d);
        return NULL;
    }
    d->pend = (uint8_t *)H264_DEC_ALLOC(H264_DEC_MAX_NAL);
    if (!d->pend) {
        H264_DEC_FREE(d->nal_scratch);
        H264_DEC_FREE(d);
        return NULL;
    }
    d->pend_len = 0;
    d->nal_rd.scratch = d->nal_scratch;
    d->nal_rd.scratch_cap = H264_DEC_MAX_NAL;

    d->max_w = max_width;
    d->max_h = max_height;
    frame_init(&d->cur, max_width, max_height);
    for (int i = 0; i < H264_DEC_MAX_REFS; i++) {
        frame_init(&d->refs[i], max_width, max_height);
    }
    mbtab_init(&d->mb_tab, max_width, max_height);
    d->max_refs = H264_DEC_MAX_REFS;
    return d;
}

void h264_dec_destroy(h264_dec_t *dec) {
    if (!dec) return;
    if (dec->nal_scratch) H264_DEC_FREE(dec->nal_scratch);
    if (dec->pend) H264_DEC_FREE(dec->pend);
    frame_release(&dec->cur);
    for (int i = 0; i < H264_DEC_MAX_REFS; i++) frame_release(&dec->refs[i]);
    mbtab_release(&dec->mb_tab);
    H264_DEC_FREE(dec);
}

void h264_dec_set_max_refs(h264_dec_t *dec, int refs) {
    if (!dec) return;
    if (refs < 1) refs = 1;
    if (refs > H264_DEC_MAX_REFS) refs = H264_DEC_MAX_REFS;
    dec->max_refs = refs;
}

void h264_dec_set_max_dim(h264_dec_t *dec, int max_w, int max_h) {
    if (!dec) return;
    dec->max_w = max_w;
    dec->max_h = max_h;
    frame_set_max_dim(&dec->cur, max_w, max_h);
    for (int i = 0; i < H264_DEC_MAX_REFS; i++) {
        frame_set_max_dim(&dec->refs[i], max_w, max_h);
    }
    mbtab_set_max_dim(&dec->mb_tab, max_w, max_h);
}

int h264_dec_begin(h264_dec_t *dec) {
    if (!dec) return 0;
    int ok = frame_ensure_alloc(&dec->cur);
    for (int i = 0; i < H264_DEC_MAX_REFS; i++) {
        ok = frame_ensure_alloc(&dec->refs[i]) && ok;
    }
    return ok;
}

void h264_dec_end(h264_dec_t *dec) {
    if (!dec) return;
    frame_release(&dec->cur);
    for (int i = 0; i < H264_DEC_MAX_REFS; i++) frame_release(&dec->refs[i]);
    mbtab_release(&dec->mb_tab);
    dec->have_sps = 0;
    dec->have_pps = 0;
    dec->input_exhausted = 0;
    dec->ref_cnt = 0;
    dec->slice_cnt = 0;
    memset(&dec->cur_sps, 0, sizeof(dec->cur_sps));
}

void h264_dec_set_input(h264_dec_t *dec, const uint8_t *data, size_t size) {
    if (!dec) return;
    nal_rd_reset(&dec->nal_rd, data, size);
    dec->input_exhausted = 0;
}

int h264_dec_input_exhausted(const h264_dec_t *dec) {
    return dec ? dec->input_exhausted : 1;
}

h264_dec_status_t h264_dec_next(h264_dec_t *dec) {
    if (!dec) return H264_DEC_ERROR;
    h264_nal_t nal;
    if (!nal_rd_next(&dec->nal_rd, &nal)) {
        dec->input_exhausted = 1;
        return H264_DEC_NEED_MORE;
    }

    if (nal.type == H264_NAL_SPS) {
        h264_br_t br;
        br_init(&br, nal.rbsp, nal.rbsp_size);
        h264_sps_t sps;
        if (!parse_sps(&br, &sps)) {
            H264_LOG("SPS parse failed (corrupt/truncated)\n");
            return H264_DEC_ERROR;
        }
        if (sps.unsupported) {
            H264_LOG("SPS id=%u uses unsupported feature\n", sps.id);
            return H264_DEC_UNSUPPORTED;
        }
        if (sps.coded_width > (uint32_t)dec->max_w ||
            sps.coded_height > (uint32_t)dec->max_h) {
            H264_LOG("SPS %ux%u exceeds ceiling %dx%d\n",
                     sps.coded_width, sps.coded_height, dec->max_w, dec->max_h);
            return H264_DEC_UNSUPPORTED;
        }
        dec->sps_tab[sps.id % H264_MAX_SPS] = sps;
        dec->have_sps = 1;
        return H264_DEC_NEED_MORE;
    }

    if (nal.type == H264_NAL_PPS) {
        h264_br_t br;
        br_init(&br, nal.rbsp, nal.rbsp_size);
        h264_pps_t pps;
        if (!parse_pps(&br, &pps)) {
            H264_LOG("PPS parse failed (corrupt/truncated)\n");
            return H264_DEC_ERROR;
        }
        if (pps.unsupported) {
            H264_LOG("PPS id=%u uses unsupported feature\n", pps.id);
            return H264_DEC_UNSUPPORTED;
        }
        dec->pps_tab[pps.id % H264_MAX_PPS] = pps;
        dec->have_pps = 1;
        return H264_DEC_NEED_MORE;
    }

    if (nal.type == H264_NAL_SLICE_IDR || nal.type == H264_NAL_SLICE_NON_IDR) {
        if (nal.type == H264_NAL_SLICE_IDR) dec->ref_cnt = 0; /* IDR 清 DPB */
        return dec_decode_slice(dec, &nal);
    }

    return H264_DEC_NEED_MORE; /* SEI / AUD / filler 等 */
}

int h264_dec_feed(h264_dec_t *dec, const uint8_t *data, size_t size) {
    h264_dec_set_input(dec, data, size);
    int frames = 0;
    while (!dec->input_exhausted) {
        h264_dec_status_t st = h264_dec_next(dec);
        if (st == H264_DEC_OK) {
            frames++;
        } else if (st != H264_DEC_NEED_MORE) {
            return -(int)st;
        }
    }
    return frames;
}

const uint8_t *h264_dec_y(const h264_dec_t *dec) { return dec->cur.y; }
const uint8_t *h264_dec_u(const h264_dec_t *dec) { return dec->cur.u; }
const uint8_t *h264_dec_v(const h264_dec_t *dec) { return dec->cur.v; }
int h264_dec_width(const h264_dec_t *dec) { return dec->cur.width; }
int h264_dec_height(const h264_dec_t *dec) { return dec->cur.height; }
int h264_dec_stride_y(const h264_dec_t *dec) { return dec->cur.stride_y; }
int h264_dec_stride_c(const h264_dec_t *dec) { return dec->cur.stride_c; }

uint32_t h264_dec_time_scale(const h264_dec_t *dec) {
    return dec->cur_sps.time_scale;
}
uint32_t h264_dec_num_units_in_tick(const h264_dec_t *dec) {
    return dec->cur_sps.num_units_in_tick;
}

void h264_dec_frame_to_i420(const h264_dec_t *dec, uint8_t *dst) {
    const h264_frame_t *f = &dec->cur;
    size_t y_size = (size_t)f->stride_y * f->height;
    size_t c_size = (size_t)f->stride_c * (f->height / 2);
    memcpy(dst, f->y, y_size);
    memcpy(dst + y_size, f->u, c_size);
    memcpy(dst + y_size + c_size, f->v, c_size);
}

/* ================================================================== */
/* 流式输入：任意长度分片喂入，内部按 start code 组包完整 NAL           */
/* ================================================================== */

/* 从尾向前找 00 00 01。返回 start code 首字节下标；未找到返回 (size_t)-1 */
static size_t find_last_start_code(const uint8_t *buf, size_t size) {
    if (size < 3) return (size_t)-1;
    for (size_t i = size - 3; ; i--) {
        if (buf[i] == 0 && buf[i + 1] == 0 && buf[i + 2] == 1) return i;
        if (i == 0) break;
    }
    return (size_t)-1;
}

void h264_dec_stream_reset(h264_dec_t *dec) {
    if (!dec) return;
    dec->pend_len = 0;
}

/* 流结束/断开时收尾：把残余的最后一个 NAL（此后不会再有数据来划界）
 * 当作完整 NAL 解码掉。返回新解出的图像数（>=0）或负错误码。 */
int h264_dec_stream_flush(h264_dec_t *dec) {
    if (!dec) return -(int)H264_DEC_ERROR;
    if (dec->pend_len == 0) return 0;
    int frames = 0;
    h264_dec_set_input(dec, dec->pend, dec->pend_len);
    while (!dec->input_exhausted) {
        h264_dec_status_t st = h264_dec_next(dec);
        if (st == H264_DEC_OK) {
            frames++;
        } else if (st != H264_DEC_NEED_MORE) {
            dec->pend_len = 0;
            return -(int)st;
        }
    }
    dec->pend_len = 0;
    return frames;
}

int h264_dec_stream_write(h264_dec_t *dec, const uint8_t *data, size_t size) {
    if (!dec || (!data && size > 0)) return -(int)H264_DEC_ERROR;

    int frames = 0;

    /* 追加到残余（一个 NAL 装不下即码流异常/上限过小） */
    if (dec->pend_len + size > H264_DEC_MAX_NAL) {
        H264_LOG("stream: NAL overflow (%zu+%zu > %d)\n",
                 dec->pend_len, size, H264_DEC_MAX_NAL);
        dec->pend_len = 0;
        return -(int)H264_DEC_ERROR;
    }
    memcpy(dec->pend + dec->pend_len, data, size);
    dec->pend_len += size;

    /* 以最后一个 start code 为界：之前都是完整 NAL（合法 ES 中
     * 00 00 01 只会出现在 NAL 边界，EP 字节保证了这一点），喂给
     * 解码器；之后的尾部可能不完整，留到下一次 */
    size_t last_sc = find_last_start_code(dec->pend, dec->pend_len);
    if (last_sc == (size_t)-1) {
        /* 尚无完整 NAL 边界：整段都是同一个 NAL 的中段，继续积累。
         * 首个 NAL 之前不存在数据（ AnnexB 流以 start code 开头），
         * 出现在这里只能是码流不以 AnnexB 开始 */
        if (dec->pend_len > 32) {
            H264_LOG("stream: no start code in %zu bytes\n", dec->pend_len);
            dec->pend_len = 0;
            return -(int)H264_DEC_ERROR;
        }
        return 0;
    }

    h264_dec_set_input(dec, dec->pend, last_sc);
    while (!dec->input_exhausted) {
        h264_dec_status_t st = h264_dec_next(dec);
        if (st == H264_DEC_OK) {
            frames++;
        } else if (st != H264_DEC_NEED_MORE) {
            /* 错误/不支持：丢弃残余，让调用方决定是否断流重连；
             * IDR 会重建 DPB，直播流继续喂通常可自行恢复 */
            memmove(dec->pend, dec->pend + last_sc, dec->pend_len - last_sc);
            dec->pend_len -= last_sc;
            return -(int)st;
        }
    }

    memmove(dec->pend, dec->pend + last_sc, dec->pend_len - last_sc);
    dec->pend_len -= last_sc;
    return frames;
}
