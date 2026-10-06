/*
 * h264dec.h - H.264 Baseline Profile (CAVLC, I/P slice) 纯 C 软件解码器
 *
 * 从 TinyH264 (pschatzmann/TinyH264, C++ header-only) 移植重构为 C，
 * 逐模块保留原始语义：CAVLC 熵解码 / 反变换反量化 / 帧内预测 /
 * 运动补偿(6tap FIR + 1/4 亮度 + 1/8 色度) / MV 中值预测 / 环路去块滤波 /
 * 滑动窗参考帧管理。输出 YUV420P 三平面。
 *
 * 支持范围（与上游一致，不支持的一律显式返回 H264_DEC_UNSUPPORTED）：
 *   - Baseline/Main 中仅用 CAVLC 的 I/P slice（B/SP/SI 不支持）
 *   - 4:2:0 8bit、frame_mbs_only（无隔行）、无 FMO、无加权预测
 *   - 无显式参考帧重排(ref_pic_list_modification)、无 MMCO 自适应标记
 *   - 默认滑动窗标记，最多 H264_DEC_MAX_REFS 参考帧（运行时可下调）
 *
 * 内存：全部经 H264_DEC_ALLOC/H264_DEC_FREE 宏分配（默认 malloc/free，
 * 端侧可编译期重定义到 PSRAM 分配器）。帧缓冲在首次解码时按
 * max_width x max_height 一次性分配，之后不再进堆。
 */
#ifndef H264DEC_H
#define H264DEC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 一次 h264_dec_next() 的结果（处理了一个 NAL 单元） */
typedef enum {
    H264_DEC_OK = 0,       /* 解出一幅新图像，可读取 h264_dec_y/u/v() */
    H264_DEC_NEED_MORE,    /* 本 NAL 不是图像(SPS/PPS/SEI)或图像未完成，继续 */
    H264_DEC_UNSUPPORTED,  /* 码流用了未实现的特性 */
    H264_DEC_ERROR,        /* 码流损坏/截断 */
    H264_DEC_NOMEM,        /* 帧缓冲分配失败（非码流问题，同样按终止处理） */
} h264_dec_status_t;

typedef struct h264_dec h264_dec_t;

/*
 * 创建解码器。max_width/max_height 为分配上限（必须是本解码器要遇到的
 * 最大分辨率，不要求 16 对齐，内部按宏块对齐处理）；超过该分辨率的 SPS
 * 返回 H264_DEC_UNSUPPORTED。失败返回 NULL。
 */
h264_dec_t *h264_dec_create(int max_width, int max_height);
void h264_dec_destroy(h264_dec_t *dec);

/*
 * 设置运行时最大参考帧数，钳位到 [1, H264_DEC_MAX_REFS]。默认
 * H264_DEC_MAX_REFS。立即对下一幅存入的参考帧生效。内存占用：
 * (1 + refs) * 1.5 * max_w * max_h 字节。
 */
void h264_dec_set_max_refs(h264_dec_t *dec, int refs);

/*
 * 运行时改分配上限（对应上游 setMaxDimension）：已分配的帧缓冲立即释放，
 * 下次解码按新尺寸重分配。
 */
void h264_dec_set_max_dim(h264_dec_t *dec, int max_w, int max_h);

/* 预分配全部帧缓冲（可选；否则首帧解码时懒分配）。全部成功返回 1。 */
int h264_dec_begin(h264_dec_t *dec);
/* 释放全部帧缓冲并复位码流状态（SPS/PPS 缓存、参考帧计数等）。 */
void h264_dec_end(h264_dec_t *dec);

/*
 * 喂入一段 Annex-B 缓冲（可含多个 NAL，必须包含完整 NAL 单元——
 * 网络分片需调用方先按 start code 组好包），随后用 h264_dec_next()
 * 逐 NAL 消费。buf 在消费期间必须保持有效。
 */
void h264_dec_set_input(h264_dec_t *dec, const uint8_t *data, size_t size);

/* 处理一个 NAL 单元；输入耗尽后返回 H264_DEC_NEED_MORE 且
 * h264_dec_input_exhausted() 为真。 */
h264_dec_status_t h264_dec_next(h264_dec_t *dec);
int h264_dec_input_exhausted(const h264_dec_t *dec);

/*
 * 便捷接口：set_input + 循环 next 直到耗尽。
 * 返回本次喂入解出的图像数（>=0），或负的 h264_dec_status_t 错误码
 * （H264_DEC_NEED_MORE 不算错误）。
 */
int h264_dec_feed(h264_dec_t *dec, const uint8_t *data, size_t size);

/*
 * 流式接口（推荐网络/文件源使用）：任意长度分片喂入，内部按 Annex-B
 * start code 组包——只把"到最后一个完整 start code 边界"的数据交给
 * 解码，跨分片的 NAL 尾部留在内部残余缓冲区等下一片补齐，调用方无
 * 需关心 NAL 边界。返回值同 h264_dec_feed()。
 */
int h264_dec_stream_write(h264_dec_t *dec, const uint8_t *data, size_t size);

/* 丢弃流式残余缓冲（换源/重连时用；不改解码器参数集状态） */
void h264_dec_stream_reset(h264_dec_t *dec);

/* 流结束/断开时收尾：残余的最后一个 NAL 当作完整 NAL 解码掉。
 * 返回值同 h264_dec_feed()。 */
int h264_dec_stream_flush(h264_dec_t *dec);

/* ---- 最近一幅完成图像的访问（h264_dec_next 返回 H264_DEC_OK 后有效） ---- */
const uint8_t *h264_dec_y(const h264_dec_t *dec);
const uint8_t *h264_dec_u(const h264_dec_t *dec);
const uint8_t *h264_dec_v(const h264_dec_t *dec);
int h264_dec_width(const h264_dec_t *dec);   /* 编码尺寸(宏块对齐) */
int h264_dec_height(const h264_dec_t *dec);
int h264_dec_stride_y(const h264_dec_t *dec);
int h264_dec_stride_c(const h264_dec_t *dec);

/* SPS 里 VUI timing_info 声明的帧率（fps = time_scale / (2*num_units_in_tick)）。
 * 无 VUI 时 num_units_in_tick == 0。 */
uint32_t h264_dec_time_scale(const h264_dec_t *dec);
uint32_t h264_dec_num_units_in_tick(const h264_dec_t *dec);

/* 把当前帧打包成紧凑 I420（Y 全平面 + U + V，无行填充）。
 * dst 需 width*height*3/2 字节。 */
void h264_dec_frame_to_i420(const h264_dec_t *dec, uint8_t *dst);

#ifdef __cplusplus
}
#endif

#endif /* H264DEC_H */
