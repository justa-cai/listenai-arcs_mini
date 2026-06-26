/**
 * @file fatfs_custom_config.h
 * @brief FATFS 中文文件名支持配置（可选）
 *
 * 【功能说明】
 * 此配置文件启用 FATFS 的 Unicode 支持，使 SD 卡中的中文文件名可以正确显示。
 *
 * 【使用方法】
 * 1. 此文件已启用，直接编译即可支持中文文件名
 * 2. 如果不需要中文文件名支持，可以删除此文件，同时删除 prj.conf 中以下两行：
 *    CONFIG_FATFS_FFCONF_OVERRIDE=y
 *    CONFIG_FATFS_FFCONF_NAME="fatfs_custom_config.h"
 *
 * 【技术说明】
 * FATFS 默认使用 ANSI/OEM 编码（FF_LFN_UNICODE=0）和美国英语代码页（FF_CODE_PAGE=437），
 * 无法正确处理中文字符。此配置覆盖默认设置，使用 UTF-8 编码和简体中文代码页。
 */

/* ============================================================================
 * 长文件名（LFN）Unicode 配置
 * ============================================================================ */

/**
 * FF_LFN_UNICODE: 切换 API 的字符编码（当启用 LFN 时）
 *
 * 0: ANSI/OEM 在当前代码页 (TCHAR = char)
 * 1: Unicode UTF-16 (TCHAR = WCHAR)
 * 2: Unicode UTF-8 (TCHAR = char)   <-- 推荐：兼容标准 C 字符串
 * 3: Unicode UTF-32 (TCHAR = DWORD)
 */
#undef FF_LFN_UNICODE
#define FF_LFN_UNICODE    2    /* 使用 UTF-8 编码，最兼容 */

/* ============================================================================
 * 代码页配置
 * ============================================================================ */

/**
 * FF_CODE_PAGE: 指定目标系统使用的 OEM 代码页
 *
 * 437 - U.S.
 * 936 - 简体中文 (DBCS)
 * 950 - 繁体中文 (DBCS)
 *   0 - 包含上述所有代码页，并通过 f_setcp() 配置
 *
 * 注意：当 FF_LFN_UNICODE >= 1 时，代码页主要用于短文件名（8.3 格式）
 */
#undef FF_CODE_PAGE
#define FF_CODE_PAGE      936  /* 简体中文 */

/* ============================================================================
 * 字符串 I/O 编码（当 FF_USE_STRFUNC 启用时有效）
 * ============================================================================ */

/**
 * FF_STRF_ENCODE: 字符串 I/O 函数的字符编码假设
 *
 * 0: ANSI/OEM 在当前代码页
 * 1: Unicode UTF-16LE
 * 2: Unicode UTF-16BE
 * 3: Unicode UTF-8
 */
#undef FF_STRF_ENCODE
#define FF_STRF_ENCODE    3    /* UTF-8 */