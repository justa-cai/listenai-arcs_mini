#ifndef __BOOT_PARTAB_H__
#define __BOOT_PARTAB_H__

#include <stdint.h>
#include <stdbool.h>

#ifndef CONFIG_MEM_FLASH_BASE
#define CONFIG_MEM_FLASH_BASE 0x30000000UL
#endif

#ifndef CONFIG_BOOT_FLASH_SIZE
#define CONFIG_BOOT_FLASH_SIZE 0x2000UL
#endif

#ifndef CONFIG_BOOT_PARTAB_SIZE
#define CONFIG_BOOT_PARTAB_SIZE 0x1000UL
#endif

/*
 * OTA partab occupies the last flash page in the reserved boot area so the
 * application image still starts at CONFIG_BOOT_FLASH_SIZE.
 */
#define BOOT_CONFIG_SIZE        CONFIG_BOOT_PARTAB_SIZE
#define BOOT_CONFIG_BASE        (CONFIG_MEM_FLASH_BASE + CONFIG_BOOT_FLASH_SIZE - BOOT_CONFIG_SIZE)

#if (BOOT_CONFIG_SIZE % 4096) != 0
#error "BOOT_CONFIG_SIZE must be 4KB aligned"
#endif

#if CONFIG_BOOT_FLASH_SIZE < BOOT_CONFIG_SIZE
#error "CONFIG_BOOT_FLASH_SIZE must reserve at least one page for boot partab"
#endif

#if (BOOT_CONFIG_BASE % 4096) != 0
#error "BOOT_CONFIG_BASE must be 4KB aligned"
#endif

#define PARTAB_MAGIC            0x50415254  /* "PART" */
#define PARTAB_VERSION          0x0001

#define MAX_PARTITIONS          16
#define PART_NAME_SIZE          16

/*
 * 分区标志位
 */
#define PART_FLAG_VALID         (1 << 0)    /* 分区有效 */
#define PART_FLAG_BOOTABLE      (1 << 1)    /* 可引导 */
#define PART_FLAG_COMPRESS      (1 << 2)    /* txz压缩格式 */
#define PART_FLAG_ACTIVE        (1 << 3)    /* 当前活动 */
#define PART_FLAG_REMAP         (1 << 4)    /* 需要地址映射 */

/*
 * 启动模式配置
 */
typedef enum {
    BOOT_SCHEME_AB = 0,         /* A/B分区模式: 两套完整固件，地址映射切换 */
    BOOT_SCHEME_OTA = 1,        /* A/OTA模式: 单分区+OTA区，解压覆盖 */
} boot_scheme_t;

/*
 * 分区描述
 */
typedef struct {
    char        name[PART_NAME_SIZE];   /* 分区名称: "APP-A", "APP-B", "OTA", "RES"... */
    uint32_t    base;                   /* 分区起始地址 (物理地址) */
    uint32_t    size;                   /* 分区大小 */
    uint32_t    exec;                   /* 执行地址/虚拟地址 (0xFFFFFFFF表示不可执行) */
    uint32_t    flags;                  /* 标志位 */
    uint32_t    crc32;                  /* 分区数据CRC (可选校验) */
    uint8_t     reserved[7];            /* 预留 */
} partition_t;

/*
 * Boot配置 (替代原来的syscfg+mbr)
 */
typedef struct {
    /* 头部信息 */
    uint32_t    magic;                  /* PARTAB_MAGIC */
    uint32_t    version;                /* 版本号 */
    uint32_t    size;                   /* 结构体大小 */
    uint32_t    crc32;                  /* CRC校验 (计算时此字段置0) */
    
    /* Boot control store 配置 */
    uint32_t    control_store_base;     /* control store 起始地址 */
    uint32_t    control_store_size;     /* control store 大小 */
    
    /* 启动方案配置 */
    uint8_t     boot_scheme;            /* 启动方案: BOOT_SCHEME_AB 或 BOOT_SCHEME_OTA */
    uint8_t     reserved1[3];           /* 预留对齐 */
    
    /* 分区表 */
    uint32_t    part_count;             /* 分区数量 */
    uint32_t    reserved2;              /* 预留对齐 */
    partition_t partitions[MAX_PARTITIONS];
    
} boot_config_t;

/*
 * AB分区启动槽位
 */
typedef enum {
    BOOT_SLOT_A = 0,
    BOOT_SLOT_B = 1,
} boot_slot_t;

/*
 * 启动模式
 */
typedef enum {
    BOOT_MODE_NORMAL = 0,               /* 正常引导 */
    BOOT_MODE_UPDATE = 1,               /* OTA 升级（自动识别 tar/txz 格式） */
} boot_mode_t;

/*
 * API
 */

/* 加载并校验新的boot配置 (partition table 版本) */
boot_config_t *boot_partab_load(void);


/* 查找分区 */
partition_t *boot_partition_find(boot_config_t *cfg, const char *name);

/* 查找APP分区 (根据槽位，用于AB模式) */
partition_t *boot_partition_find_app(boot_config_t *cfg, boot_slot_t slot);

/* 查找OTA分区 */
partition_t *boot_partition_find_ota(boot_config_t *cfg);

/* 校验APP header */
bool boot_app_verify(partition_t *part);

/* 引导APP */
void boot_app_jump(partition_t *part);

/* 获取已加载的boot配置 */
boot_config_t *boot_config_get(void);

/* 打印分区表信息 */
void boot_partab_dump(boot_config_t *cfg);

/* 获取启动方案 */
boot_scheme_t boot_config_get_scheme(boot_config_t *cfg);

/* 获取所有需要地址映射的分区并设置映射 */
int boot_remap_all_partitions(boot_config_t *cfg, boot_slot_t slot);

#endif /* __BOOT_PARTAB_H__ */
