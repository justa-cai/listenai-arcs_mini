#ifndef __BOOT_OTA_H__
#define __BOOT_OTA_H__

#include <stdint.h>
#include <stdbool.h>
#include "boot_partab.h"
#include "boot_ota_source.h"

/*
 * OTA包配置
 * 
 * 压缩命令示例:
 * 7z a ota.txz ota.tar -mx9 -m0=LZMA2:d128k:fb273 -mmt-
 */
#define TXZ_DICT_MAX        (128 * 1024)    /* XZ字典最大大小 (与压缩参数d128k匹配) */
#define TXZ_INPUT_BUF_SIZE  (4096 * 4)      /* 输入缓冲区大小 (16KB) */
#define TXZ_OUTPUT_BUF_SIZE (32 * 1024)     /* 输出缓冲区大小 (32KB, 降低boot峰值内存占用) */

/*
 * TAR文件类型
 */
#define TAR_TYPE_REGTYPE    '0'     /* 普通文件 */
#define TAR_TYPE_AREGTYPE   '\0'    /* 普通文件 (另一种表示) */
#define TAR_TYPE_LNKTYPE    '1'     /* 链接 */
#define TAR_TYPE_SYMTYPE    '2'     /* 符号链接 */
#define TAR_TYPE_CHRTYPE    '3'     /* 字符设备 */
#define TAR_TYPE_BLKTYPE    '4'     /* 块设备 */
#define TAR_TYPE_DIRTYPE    '5'     /* 目录 */
#define TAR_TYPE_FIFOTYPE   '6'     /* FIFO */
#define TAR_TYPE_CONTTYPE   '7'     /* 保留 */

#define TAR_BLOCK_SIZE      512
#define TAR_SHORTNAME_SIZE  100

/*
 * TAR块结构
 */
typedef union {
    char dat[TAR_BLOCK_SIZE];
    struct {
        char name[TAR_SHORTNAME_SIZE];      /* 文件名 */
        char mode[8];                       /* 权限 */
        char uid[8];                        /* 用户ID */
        char gid[8];                        /* 组ID */
        char size[12];                      /* 大小 (八进制) */
        char mtime[12];                     /* 修改时间 */
        char chksum[8];                     /* 校验和 */
        char typeflag;                      /* 类型 */
        char linkname[TAR_SHORTNAME_SIZE];  /* 链接名 */
        char magic[6];                      /* magic */
        char version[2];                    /* 版本 */
        char uname[32];                     /* 用户名 */
        char gname[32];                     /* 组名 */
        char devmajor[8];                   /* 主设备号 */
        char devminor[8];                   /* 次设备号 */
        char prefix[155];                   /* 前缀 */
    } hdr;
} tar_blk_t;

/*
 * OTA文件映射信息
 */
typedef struct {
    char        path_match[160]; /* 包内文件路径 */
    uint32_t    part_base;      /* 目标分区基址 */
    uint32_t    part_size;      /* 目标分区大小 */
    uint32_t    file_size;      /* 实际文件大小 (输出) */
    uint8_t     expected_md5[16]; /* manifest 中声明的 MD5 */
    bool        matched;        /* 包内是否找到该文件 */
    bool        verified;       /* 是否已经完成并通过 MD5 校验 */
    bool        written;        /* 是否已经完整写入 */
} ota_file_t;

#define MAX_OTA_FILES   8

/*
 * OTA升级参数
 */
typedef struct {
    uint32_t    src_addr;       /* 源数据地址 (tar/txz包) */
    uint32_t    src_size;       /* 源数据大小 */
    uint32_t    file_count;     /* 需写入文件数量 */
    ota_file_t  files[MAX_OTA_FILES];   /* 文件映射表 */
} ota_param_t;

/*
 * OTA升级
 * @param cfg boot配置
 * @param ota_part OTA分区 (包含tar/txz数据)
 * @param target_slot 目标槽位
 * @return 0成功，其他失败
 */
int boot_ota_txz_update(boot_config_t *cfg, partition_t *ota_part);
int boot_ota_txz_update_from_source(boot_config_t *cfg,
                                    boot_ota_source_t *source,
                                    uboot_ota_failure_info_t *failure);

#endif /* __BOOT_OTA_H__ */
