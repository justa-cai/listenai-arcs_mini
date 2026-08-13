/*
 * Copyright (c) 2025, LISTENAI
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LISA_SYS_CHIP_ID_H_
#define LISA_SYS_CHIP_ID_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 获取芯片硬件唯一 ID 的十六进制字符串。
 *
 * 长度固定 16 字符，字符集 [0-9A-F] 或 [0-9a-f]（由芯片 arch 选择，默认大写；
 * ARCS 为小写以匹配 u-boot device serial），'\0' 结尾，共占 17 字节。
 * 多次调用返回的内容逐字节相同。
 * 芯片 ID 区未烧入时返回全 0 字符串。
 *
 * @return 指向 SDK 内部静态 buffer 的指针，调用方不要修改或释放。
 */
const char *sys_chip_id_get(void);

#ifdef __cplusplus
}
#endif

#endif /* LISA_SYS_CHIP_ID_H_ */
