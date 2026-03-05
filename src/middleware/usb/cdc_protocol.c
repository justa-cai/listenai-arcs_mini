/*
 * Copyright (c) 2023, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "cdc_protocol.h"
#include <string.h>

#define TAG "usb_audio_pcl"
#include "lisa_log.h"

/**
 * @brief 计算校验和（XOR）
 */
uint8_t cdc_proto_calc_checksum(const uint8_t *data, uint32_t len)
{
    uint8_t checksum = 0;
    for (uint32_t i = 0; i < len; i++) {
        checksum ^= data[i];
    }
    return checksum;
}

/**
 * @brief 构建帧（用于命令和响应，不用于音频数据）
 */
int cdc_proto_build_frame(cdc_frame_t *frame, cdc_frame_type_t type,
                          const uint8_t *data, uint32_t length)
{
    if (!frame || (length > 0 && !data)) {
        return -1;
    }

    // 命令/响应帧使用较小的限制以节省内存
    if (length > CDC_PROTO_MAX_CMD_DATA_SIZE) {
        LISA_LOGE(TAG,"The frame data is too long(%d), max=%d", length, CDC_PROTO_MAX_CMD_DATA_SIZE);
        return -1;
    }

    // 填充帧头
    frame->header.magic[0] = CDC_PROTO_MAGIC_MSB;
    frame->header.magic[1] = CDC_PROTO_MAGIC_LSB;
    frame->header.type = type;
    frame->header.length = length;  // 小端序，直接赋值
    frame->data = (uint8_t *)data;

    // 计算校验和（帧头 + 数据）
    uint8_t checksum = cdc_proto_calc_checksum((const uint8_t *)&frame->header,
                                                CDC_PROTO_HEADER_SIZE);
    if (data && length > 0) {
        checksum ^= cdc_proto_calc_checksum(data, length);
    }
    frame->checksum = checksum;

    return 0;
}

/**
 * @brief 序列化帧到缓冲区
 */
int cdc_proto_serialize_frame(const cdc_frame_t *frame, uint8_t *buffer, uint32_t buf_size)
{
    if (!frame || !buffer) {
        return -1;
    }

    uint32_t total_size = CDC_PROTO_HEADER_SIZE + frame->header.length + 1;
    if (buf_size < total_size) {
        return -1;
    }

    uint32_t offset = 0;

    // 复制帧头
    memcpy(buffer + offset, &frame->header, CDC_PROTO_HEADER_SIZE);
    offset += CDC_PROTO_HEADER_SIZE;

    // 复制数据
    if (frame->header.length > 0 && frame->data) {
        memcpy(buffer + offset, frame->data, frame->header.length);
        offset += frame->header.length;
    }

    // 添加校验和
    buffer[offset] = frame->checksum;
    offset++;

    return offset;
}

/**
 * @brief 初始化帧解析器
 */
int cdc_proto_parser_init(cdc_parser_t *parser, uint8_t *data_buf, uint32_t buf_size)
{
    if (!parser || !data_buf || buf_size == 0) {
        return -1;
    }

    memset(parser, 0, sizeof(cdc_parser_t));
    parser->state = CDC_PARSE_MAGIC1;
    parser->data_buf = data_buf;
    parser->data_buf_size = buf_size;

    return 0;
}

/**
 * @brief 重置解析器状态
 */
void cdc_proto_parser_reset(cdc_parser_t *parser)
{
    if (!parser) {
        return;
    }

    parser->state = CDC_PARSE_MAGIC1;
    parser->length_idx = 0;
    parser->data_idx = 0;
    memset(&parser->frame, 0, sizeof(cdc_frame_t));
}

/**
 * @brief 解析单个字节
 */
int cdc_proto_parse_byte(cdc_parser_t *parser, uint8_t byte, cdc_frame_t *frame)
{
    if (!parser) {
        return -1;
    }

    switch (parser->state) {
    case CDC_PARSE_MAGIC1:
        if (byte == CDC_PROTO_MAGIC_MSB) {
            parser->frame.header.magic[0] = byte;
            parser->state = CDC_PARSE_MAGIC2;
        }
        break;

    case CDC_PARSE_MAGIC2:
        if (byte == CDC_PROTO_MAGIC_LSB) {
            parser->frame.header.magic[1] = byte;
            parser->state = CDC_PARSE_TYPE;
        } else {
            // 魔数错误，重新开始
            cdc_proto_parser_reset(parser);
            // 检查当前字节是否是魔数第一字节
            if (byte == CDC_PROTO_MAGIC_MSB) {
                parser->frame.header.magic[0] = byte;
                parser->state = CDC_PARSE_MAGIC2;
            }
        }
        break;

    case CDC_PARSE_TYPE:
        parser->frame.header.type = byte;
        parser->state = CDC_PARSE_LENGTH;
        parser->length_idx = 0;
        break;

    case CDC_PARSE_LENGTH:
        parser->length_buf[parser->length_idx++] = byte;
        if (parser->length_idx >= 4) {
            // 小端序解析长度
            parser->frame.header.length = parser->length_buf[0] |
                                         (parser->length_buf[1] << 8) |
                                         (parser->length_buf[2] << 16) |
                                         (parser->length_buf[3] << 24);

            // 检查缓冲区是否足够
            if (parser->frame.header.length > parser->data_buf_size) {
                cdc_proto_parser_reset(parser);
                return -1;
            }

            if (parser->frame.header.length > 0) {
                parser->frame.data = parser->data_buf;
                parser->data_idx = 0;
                parser->state = CDC_PARSE_DATA;
            } else {
                parser->frame.data = NULL;
                parser->state = CDC_PARSE_CHECKSUM;
            }
        }
        break;

    case CDC_PARSE_DATA:
        parser->data_buf[parser->data_idx++] = byte;
        if (parser->data_idx >= parser->frame.header.length) {
            parser->state = CDC_PARSE_CHECKSUM;
        }
        break;

    case CDC_PARSE_CHECKSUM:
        parser->frame.checksum = byte;

        // 验证校验和
        uint8_t calc_checksum = cdc_proto_calc_checksum(
            (const uint8_t *)&parser->frame.header, CDC_PROTO_HEADER_SIZE);
        if (parser->frame.data && parser->frame.header.length > 0) {
            calc_checksum ^= cdc_proto_calc_checksum(parser->frame.data,
                                                      parser->frame.header.length);
        }

        if (calc_checksum != parser->frame.checksum) {
            // 校验失败
            cdc_proto_parser_reset(parser);
            return -1;
        }

        // 帧解析完成，复制到输出
        if (frame) {
            memcpy(frame, &parser->frame, sizeof(cdc_frame_t));
        }

        // 重置解析器以准备下一帧
        cdc_proto_parser_reset(parser);
        return 1;  // 帧解析完成

    default:
        cdc_proto_parser_reset(parser);
        return -1;
    }

    return 0;  // 需要更多数据
}

/**
 * @brief 构建命令响应帧
 */
int cdc_proto_build_cmd_response(uint8_t *buffer, uint32_t buf_size,
                                  cdc_cmd_id_t cmd_id, cdc_status_t status,
                                  const uint8_t *data, uint32_t data_len)
{
    if (!buffer || buf_size < CDC_PROTO_MIN_FRAME_SIZE + 2) {
        return -1;
    }

    // 计算总数据长度（cmd_id + status + data）
    uint32_t total_data_len = 2 + data_len;
    if (total_data_len > CDC_PROTO_MAX_CMD_DATA_SIZE) {
        return -1;
    }

    // 临时缓冲区构建响应数据（使用命令数据限制，节省栈空间）
    uint8_t temp_buf[CDC_PROTO_MAX_CMD_DATA_SIZE];
    temp_buf[0] = cmd_id;
    temp_buf[1] = status;
    if (data && data_len > 0) {
        memcpy(temp_buf + 2, data, data_len);
    }

    // 构建帧
    cdc_frame_t frame;
    if (cdc_proto_build_frame(&frame, CDC_TYPE_CMD_RESPONSE, temp_buf, total_data_len) != 0) {
        return -1;
    }

    // 序列化
    return cdc_proto_serialize_frame(&frame, buffer, buf_size);
}

/**
 * @brief 构建音频数据帧头（优化版：避免拷贝PCM数据，无校验和）
 *
 * 音频帧为性能优化，不包含校验和字节，数据完整性由最终的MD5校验保证
 */
int cdc_proto_build_audio_header(uint8_t *header_buf, uint32_t buf_size,
                                  uint32_t seq_num, uint32_t pcm_len)
{
    if (!header_buf || pcm_len == 0) {
        return -1;
    }

    // 计算总数据长度（seq_num + pcm_data）
    uint32_t total_data_len = 4 + pcm_len;


    // 需要的缓冲区大小：帧头(7) + 序列号(4) = 11字节
    if (buf_size < CDC_PROTO_HEADER_SIZE + 4) {
        return -1;
    }

    // 构建帧头
    header_buf[0] = CDC_PROTO_MAGIC_MSB;
    header_buf[1] = CDC_PROTO_MAGIC_LSB;
    header_buf[2] = CDC_TYPE_AUDIO_DATA;

    // 小端序写入总数据长度（seq_num + pcm）
    header_buf[3] = total_data_len & 0xFF;
    header_buf[4] = (total_data_len >> 8) & 0xFF;
    header_buf[5] = (total_data_len >> 16) & 0xFF;
    header_buf[6] = (total_data_len >> 24) & 0xFF;

    // 小端序写入序列号
    header_buf[7] = seq_num & 0xFF;
    header_buf[8] = (seq_num >> 8) & 0xFF;
    header_buf[9] = (seq_num >> 16) & 0xFF;
    header_buf[10] = (seq_num >> 24) & 0xFF;

    return CDC_PROTO_HEADER_SIZE + 4;  // 返回11字节
}

/**
 * @brief 构建MD5数据帧
 */
int cdc_proto_build_md5_frame(uint8_t *buffer, uint32_t buf_size, const uint8_t *md5)
{
    if (!buffer || !md5) {
        return -1;
    }

    if (buf_size < CDC_PROTO_MIN_FRAME_SIZE + 16) {
        return -1;
    }

    // 构建帧
    cdc_frame_t frame;
    if (cdc_proto_build_frame(&frame, CDC_TYPE_MD5_DATA, md5, 16) != 0) {
        return -1;
    }

    // 序列化
    return cdc_proto_serialize_frame(&frame, buffer, buf_size);
}

/**
 * @brief 构建状态查询响应
 */
int cdc_proto_build_status_response(uint8_t *buffer, uint32_t buf_size,
                                     cdc_state_t state, uint32_t total_bytes, uint32_t seq_num)
{
    if (!buffer || buf_size < CDC_PROTO_MIN_FRAME_SIZE + 2 + 9) {
        return -1;
    }

    // 构建状态数据
    uint8_t status_data[9];
    status_data[0] = state;

    // 小端序写入total_bytes
    status_data[1] = total_bytes & 0xFF;
    status_data[2] = (total_bytes >> 8) & 0xFF;
    status_data[3] = (total_bytes >> 16) & 0xFF;
    status_data[4] = (total_bytes >> 24) & 0xFF;

    // 小端序写入seq_num
    status_data[5] = seq_num & 0xFF;
    status_data[6] = (seq_num >> 8) & 0xFF;
    status_data[7] = (seq_num >> 16) & 0xFF;
    status_data[8] = (seq_num >> 24) & 0xFF;

    // 构建命令响应
    return cdc_proto_build_cmd_response(buffer, buf_size,
                                         CDC_CMD_QUERY_STATUS, CDC_STATUS_OK,
                                         status_data, sizeof(status_data));
}
