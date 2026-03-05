/*
 * Copyright (c) 2023, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __CDC_PROTOCOL_H__
#define __CDC_PROTOCOL_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file cdc_protocol.h
 * @brief USB CDC通信协议定义和接口
 *
 * 详细协议规范请参考：docs/usb_cdc_protocol.md
 */

/* 协议常量 */
#define CDC_PROTO_MAGIC_MSB         0xAA    // 魔数高字节
#define CDC_PROTO_MAGIC_LSB         0x55    // 魔数低字节
#define CDC_PROTO_HEADER_SIZE       7       // 帧头大小（Magic+Type+Length）
#define CDC_PROTO_MIN_FRAME_SIZE    8       // 最小帧大小（Header+Check）

/* 数据长度限制 - 区分命令帧和音频帧以节省内存 */
#define CDC_PROTO_MAX_CMD_DATA_SIZE     64      // 命令/响应/MD5最大数据长度


/* 数据类型定义 */
typedef enum {
    CDC_TYPE_CMD_REQUEST    = 0x01,    // 命令请求（PC->设备）
    CDC_TYPE_CMD_RESPONSE   = 0x02,    // 命令响应（设备->PC）
    CDC_TYPE_AUDIO_DATA     = 0x03,    // 音频数据（设备->PC）
    CDC_TYPE_MD5_DATA       = 0x04,    // MD5数据（设备->PC）
} cdc_frame_type_t;

/* 命令码定义 */
typedef enum {
    CDC_CMD_START_RECORD    = 0x01,    // 开始录音
    CDC_CMD_STOP_RECORD     = 0x02,    // 停止录音
    CDC_CMD_QUERY_STATUS    = 0x03,    // 查询状态
} cdc_cmd_id_t;

/* 状态码定义 */
typedef enum {
    CDC_STATUS_OK           = 0x00,    // 成功
    CDC_STATUS_ERROR        = 0x01,    // 失败
    CDC_STATUS_BUSY         = 0x02,    // 忙碌
    CDC_STATUS_UNSUPPORTED  = 0x03,    // 不支持
} cdc_status_t;

/* 设备状态定义 */
typedef enum {
    CDC_STATE_IDLE          = 0x00,    // 空闲
    CDC_STATE_RECORDING     = 0x01,    // 正在录音
} cdc_state_t;

/* 帧头结构 */
typedef struct __attribute__((packed)) {
    uint8_t  magic[2];      // 魔数 0xAA55
    uint8_t  type;          // 数据类型
    uint32_t length;        // 数据长度（小端序）
} cdc_frame_header_t;

/* 完整帧结构 */
typedef struct {
    cdc_frame_header_t header;
    uint8_t *data;          // 指向数据区
    uint8_t checksum;       // 校验和
} cdc_frame_t;

/* 命令请求结构 */
typedef struct __attribute__((packed)) {
    uint8_t cmd_id;         // 命令码
    uint8_t params[];       // 命令参数（可变长度）
} cdc_cmd_request_t;

/* 命令响应结构 */
typedef struct __attribute__((packed)) {
    uint8_t cmd_id;         // 命令码
    uint8_t status;         // 状态码
    uint8_t data[];         // 响应数据（可变长度）
} cdc_cmd_response_t;

/* 音频数据结构 */
typedef struct __attribute__((packed)) {
    uint32_t seq_num;       // 序列号（小端序）
    uint8_t pcm_data[];     // PCM音频数据（可变长度）
} cdc_audio_data_t;

/* 状态查询响应结构 */
typedef struct __attribute__((packed)) {
    uint8_t  state;         // 当前状态
    uint32_t total_bytes;   // 已传输字节数（小端序）
    uint32_t seq_num;       // 当前序列号（小端序）
} cdc_status_query_response_t;

/* 帧解析状态机 */
typedef enum {
    CDC_PARSE_MAGIC1,       // 等待魔数第一字节
    CDC_PARSE_MAGIC2,       // 等待魔数第二字节
    CDC_PARSE_TYPE,         // 等待类型字节
    CDC_PARSE_LENGTH,       // 等待长度字段
    CDC_PARSE_DATA,         // 等待数据
    CDC_PARSE_CHECKSUM,     // 等待校验和
} cdc_parse_state_t;

/* 帧解析器结构 */
typedef struct {
    cdc_parse_state_t state;
    cdc_frame_t frame;
    uint8_t length_buf[4];  // 长度字段缓冲
    uint32_t length_idx;    // 长度字段索引
    uint32_t data_idx;      // 数据索引
    uint8_t *data_buf;      // 数据缓冲区
    uint32_t data_buf_size; // 数据缓冲区大小
} cdc_parser_t;

/**
 * @brief 计算校验和
 *
 * @param data 数据指针
 * @param len 数据长度
 * @return uint8_t 校验和
 */
uint8_t cdc_proto_calc_checksum(const uint8_t *data, uint32_t len);

/**
 * @brief 构建帧
 *
 * @param frame 帧结构指针
 * @param type 数据类型
 * @param data 数据指针
 * @param length 数据长度
 * @return int 0成功，-1失败
 */
int cdc_proto_build_frame(cdc_frame_t *frame, cdc_frame_type_t type,
                          const uint8_t *data, uint32_t length);

/**
 * @brief 序列化帧到缓冲区
 *
 * @param frame 帧结构指针
 * @param buffer 输出缓冲区
 * @param buf_size 缓冲区大小
 * @return int 序列化后的字节数，-1表示失败
 */
int cdc_proto_serialize_frame(const cdc_frame_t *frame, uint8_t *buffer, uint32_t buf_size);

/**
 * @brief 初始化帧解析器
 *
 * @param parser 解析器指针
 * @param data_buf 数据缓冲区
 * @param buf_size 缓冲区大小
 * @return int 0成功，-1失败
 */
int cdc_proto_parser_init(cdc_parser_t *parser, uint8_t *data_buf, uint32_t buf_size);

/**
 * @brief 解析单个字节
 *
 * @param parser 解析器指针
 * @param byte 输入字节
 * @param frame 输出帧指针（解析完成时）
 * @return int 1=帧解析完成，0=需要更多数据，-1=解析错误
 */
int cdc_proto_parse_byte(cdc_parser_t *parser, uint8_t byte, cdc_frame_t *frame);

/**
 * @brief 重置解析器状态
 *
 * @param parser 解析器指针
 */
void cdc_proto_parser_reset(cdc_parser_t *parser);

/**
 * @brief 构建命令响应帧
 *
 * @param buffer 输出缓冲区
 * @param buf_size 缓冲区大小
 * @param cmd_id 命令码
 * @param status 状态码
 * @param data 响应数据（可选，可为NULL）
 * @param data_len 响应数据长度
 * @return int 序列化后的字节数，-1表示失败
 */
int cdc_proto_build_cmd_response(uint8_t *buffer, uint32_t buf_size,
                                  cdc_cmd_id_t cmd_id, cdc_status_t status,
                                  const uint8_t *data, uint32_t data_len);

/**
 * @brief 构建音频数据帧头（优化版：分段发送，无校验和）
 *
 * 此函数只构建帧头和序列号部分，不拷贝PCM数据
 * 音频帧为性能优化，不包含校验和字节，数据完整性由最终的MD5校验保证
 *
 * 调用者需要按以下顺序发送：
 * 1. 调用此函数获取帧头+序列号
 * 2. 发送帧头+序列号
 * 3. 直接发送PCM数据（零拷贝）
 *
 * @param header_buf 输出缓冲区（建议至少11字节：7字节帧头+4字节序列号）
 * @param buf_size 缓冲区大小
 * @param seq_num 序列号
 * @param pcm_len PCM数据长度（仅用于填充帧头长度字段）
 * @return int 帧头+序列号的字节数（通常为11），-1表示失败
 */
int cdc_proto_build_audio_header(uint8_t *header_buf, uint32_t buf_size,
                                  uint32_t seq_num, uint32_t pcm_len);

/**
 * @brief 构建MD5数据帧
 *
 * @param buffer 输出缓冲区
 * @param buf_size 缓冲区大小
 * @param md5 MD5值（16字节）
 * @return int 序列化后的字节数，-1表示失败
 */
int cdc_proto_build_md5_frame(uint8_t *buffer, uint32_t buf_size, const uint8_t *md5);

/**
 * @brief 构建状态查询响应
 *
 * @param buffer 输出缓冲区
 * @param buf_size 缓冲区大小
 * @param state 当前状态
 * @param total_bytes 已传输字节数
 * @param seq_num 当前序列号
 * @return int 序列化后的字节数，-1表示失败
 */
int cdc_proto_build_status_response(uint8_t *buffer, uint32_t buf_size,
                                     cdc_state_t state, uint32_t total_bytes, uint32_t seq_num);

#ifdef __cplusplus
}
#endif

#endif /* __CDC_PROTOCOL_H__ */
