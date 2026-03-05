/**
 * @file lisa_sntp.h
 * @brief LISA SNTP 客户端 API
 *
 * 此文件提供 LISA SNTP（简单网络时间协议）客户端接口，支持通过 NTP 服务器获取精确的网络时间，
 * 为 ARCS 平台提供统一的时间同步功能，基于 core_sntp_client 库实现。
 */

#ifndef __LISA_SNTP_H__
#define __LISA_SNTP_H__

#include <stdint.h>

/**
 * @brief SNTP 时间结构
 *
 * 用于表示从 SNTP 服务器获取的精确时间信息，包含秒数和纳秒数。
 */
struct lisa_sntp_time {
	uint64_t sec;   /**< 自 1970-01-01 00:00:00 UTC 以来的秒数 */
	uint32_t nsec;  /**< 纳秒部分，范围 0-999999999 */
};

/**
 * @brief 查询 SNTP 服务器获取时间
 *
 * 向指定的 SNTP 服务器列表发送时间查询请求，获取精确的网络时间。
 * 函数会按顺序尝试服务器列表中的服务器，直到成功获取时间或所有服务器都失败。
 *
 * @param servers SNTP 服务器地址字符串数组，例如 {"ntp1.aliyun.com", "ntp2.aliyun.com"}
 * @param servers_cnt 服务器数组中的服务器数量
 * @param timeout 查询超时时间（毫秒）
 * @param time 输出参数，用于存储获取到的时间信息
 * @return 0 表示成功，非 0 表示失败（负值表示错误，正值表示警告）
 */
int lisa_sntp_query(const char *servers[], int servers_cnt, uint32_t timeout,
		    struct lisa_sntp_time *time);

#endif
