/**
 * @brief ico 编解码，用于 ico 与 pcm 数据之间进行语音组包、编码、解码
 *
 *
 */
#ifndef __ICO_CODEC_H__
#define __ICO_CODEC_H__

#if defined(_WIN32) || defined(_WIN64)
#define DllExport __declspec(dllexport)
#define DllImport __declspec(dllimport)
#else
#define DllExport
#define DllImport
#endif

#include "ivErrorCode.h"

/**
 * @brief ico 编码初始化
 *
 * @return ivStatus
 */
DllExport ivStatus ico_encode_init(void);

/**
 * @brief ico 解码初始化
 *
 * @return ivStatus
 */
DllExport ivStatus ico_decode_init(void);

/**
 * @brief ico 编码器重置
 *
 */
DllExport void ico_codec_reset(void);

/**
 * @brief ico 编码，编码后的数据存放在 enc_data 中
 *
 * @note 编码前的数据长度需为 640 字节，编码后的数据长度由 outLen 返回（实际输出 outLen 时，需要将 outLen 乘以 2;即最大
 * 20*2=40 字节）
 *
 * @param in 原始 pcm 数据
 * @param enc_data 编码后的数据
 * @param outLen 编码后的数据长度
 * @return ivStatus
 */
DllExport ivStatus ico_codec_encode(short *in, void *enc_data, short *outLen);

/**
 * @brief ico 解码，解码后的数据存放在 pcm_data 中
 *
 * @param in
 * @param pcm_data
 * @param outLen
 * @return ivStatus
 */
DllExport ivStatus ico_codec_decode(void *in, short *pcm_data, short *outLen);

#endif // _ICO_CODEC_H_