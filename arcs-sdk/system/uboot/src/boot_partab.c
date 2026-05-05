/*
 * Boot分区表操作
 * 
 * 实现分区表加载、校验、查找和APP引导
 */

#include "boot_partab.h"
#include "boot_nvs.h"
#include "boot_crc32.h"
#include "boot_remap.h"
#include "syslog.h"
#include <string.h>
#include <stdio.h>

/* 全局boot配置指针 */
static boot_config_t *g_boot_config = NULL;

/*
 * 加载并校验新的boot配置 (partition table 版本)
 */
boot_config_t *boot_partab_load(void)
{
    boot_config_t *cfg = (boot_config_t *)BOOT_CONFIG_BASE;
    boot_config_t tmp;
    uint32_t calc_crc;
    
    /* 读取配置到临时变量 */
    boot_nvs_read(&tmp, cfg, sizeof(boot_config_t));
    
    /* 检查magic */
    if (tmp.magic != PARTAB_MAGIC) {
        printk("boot_config: invalid magic 0x%08x\n", tmp.magic);
        return NULL;
    }
    
    /* 检查size */
    if (tmp.size > sizeof(boot_config_t) || tmp.size == 0) {
        printk("boot_config: invalid size %d\n", tmp.size);
        return NULL;
    }

    if (tmp.part_count == 0U || tmp.part_count > MAX_PARTITIONS) {
        printk("boot_config: invalid part count %d\n", tmp.part_count);
        return NULL;
    }
    
    /* CRC校验 (计算时crc32字段置0) */
    uint32_t saved_crc = tmp.crc32;
    tmp.crc32 = 0;
    calc_crc = crc32_calc(&tmp, tmp.size, 0);
    
    if (calc_crc != saved_crc) {
        printk("boot_config: crc mismatch, calc=0x%08x saved=0x%08x\n", calc_crc, saved_crc);
        return NULL;
    }
    
    printk("boot_config: valid, version=%d, parts=%d\n", tmp.version, tmp.part_count);
    printk("boot_config: control_store_base=0x%08x, size=0x%x\n",
           tmp.control_store_base, tmp.control_store_size);
    
    g_boot_config = cfg;
    return cfg;
}

/*
 * 获取已加载的boot配置
 */
boot_config_t *boot_config_get(void)
{
    return g_boot_config;
}

/*
 * 查找分区 (按名称)
 */
partition_t *boot_partition_find(boot_config_t *cfg, const char *name)
{
    if (cfg == NULL || name == NULL) {
        return NULL;
    }
    
    for (uint32_t i = 0; i < cfg->part_count && i < MAX_PARTITIONS; i++) {
        if (strncmp(cfg->partitions[i].name, name, PART_NAME_SIZE) == 0) {
            return &cfg->partitions[i];
        }
    }
    return NULL;
}

/*
 * 查找APP分区 (根据槽位)
 */
partition_t *boot_partition_find_app(boot_config_t *cfg, boot_slot_t slot)
{
    const char *name = (slot == BOOT_SLOT_A) ? "AP-A" : "AP-B";
    return boot_partition_find(cfg, name);
}

/*
 * 查找OTA分区
 */
partition_t *boot_partition_find_ota(boot_config_t *cfg)
{
    partition_t *part = boot_partition_find(cfg, "OTA_TXZ");

    if (part != NULL) {
        return part;
    }

    return boot_partition_find(cfg, "OTA");
}

/*
 * 校验APP header
 * 复用新芯片的"Hr"魔数校验方式
 */
bool boot_app_verify(partition_t *part)
{
    if (part == NULL) {
        return false;
    }
    
    /* 检查分区有效标志 */
    if (!(part->flags & PART_FLAG_VALID)) {
        printk("boot_app_verify: partition not valid\n");
        return false;
    }
    
    /* 检查APP header (位于base+0x140) */
    uint8_t *app_hd = (uint8_t *)(part->base + 0x140);
    
    /* 检查magic "Hr" 和地址匹配 */
    if ((app_hd[0] == 'H') && (app_hd[1] == 'r') && 
        (*(uint32_t *)(app_hd + 8) == part->base)) {
        printk("boot_app_verify: ok, base=0x%08x\n", part->base);
        return true;
    }
    
    printk("boot_app_verify: header invalid at 0x%08x\n", part->base);
    return false;
}

/*
 * 引导APP
 */
void boot_app_jump(partition_t *part)
{
    if (part == NULL) {
        printk("boot_app_jump: null partition\n");
        return;
    }
    
    uint32_t entry = part->exec;
    if (entry == 0xFFFFFFFF) {
        entry = part->base;
    }
    
    printk("boot_app_jump: entry=0x%08x\n", entry);
    
    /* 跳转执行 */
    void (*app_entry)(void) = (void (*)(void))entry;
    app_entry();
    
    /* 不应该到达这里 */
    while (1) {
        __asm__ volatile("wfi");
    }
}

/*
 * 打印分区表信息
 */
void boot_partab_dump(boot_config_t *cfg)
{
    if (cfg == NULL) {
        printk("boot_partab_dump: null config\n");
        return;
    }
    
    printk("=== Boot Config ===\n");
    printk("magic: 0x%08x\n", cfg->magic);
    printk("version: %d\n", cfg->version);
    printk("size: %d\n", cfg->size);
    printk("crc32: 0x%08x\n", cfg->crc32);
    printk("scheme: %s\n", (cfg->boot_scheme == BOOT_SCHEME_AB) ? "A/B" : "A/OTA");
    printk("control_store: base=0x%08x size=0x%x\n",
           cfg->control_store_base, cfg->control_store_size);
    printk("partitions: %d\n", cfg->part_count);
    
    for (uint32_t i = 0; i < cfg->part_count && i < MAX_PARTITIONS; i++) {
        partition_t *p = &cfg->partitions[i];
        printk("  [%d] %s: base=0x%08x size=0x%x exec=0x%08x flags=0x%x remap=%s\n",
               i, p->name, p->base, p->size, p->exec, p->flags, 
               (p->flags & PART_FLAG_REMAP) ? "yes" : "no");
    }
    printk("===================\n");
}

/*
 * 获取启动方案
 */
boot_scheme_t boot_config_get_scheme(boot_config_t *cfg)
{
    if (cfg == NULL) {
        return BOOT_SCHEME_OTA;  /* 默认A/OTA模式 */
    }
    return (boot_scheme_t)cfg->boot_scheme;
}

/*
 * 根据槽位映射 APP 分区到 Region B
 * 只映射 APP 分区的起始地址，其他分区基于 APP 分区偏移，映射后都在 Region B 上
 */
int boot_remap_all_partitions(boot_config_t *cfg, boot_slot_t slot)
{
    char slot_suffix = (slot == BOOT_SLOT_A) ? 'A' : 'B';
    char app_name[16];
    
    printk("remap: setting up slot %c APP partition\n", slot_suffix);
    
    /* 先禁用映射 */
    boot_remap_disable();
    
    /* 查找 APP 分区 */
    snprintf(app_name, sizeof(app_name), "AP-%c", slot_suffix);
    partition_t *app_part = boot_partition_find(cfg, app_name);
    
    if (app_part == NULL) {
        printk("remap: APP-%c partition not found\n", slot_suffix);
        return -1;
    }
    
    /* 检查分区有效标志 */
    if (!(app_part->flags & PART_FLAG_VALID)) {
        printk("remap: APP-%c partition not valid\n", slot_suffix);
        return -1;
    }
    
    /* 检查 base 是否有效 */
    if (app_part->base == 0) {
        printk("remap: APP-%c partition base=0x%08x invalid\n", slot_suffix, app_part->base);
        return -1;
    }
    
    /* 映射 APP 分区到 Region B */
    if (boot_remap_app(app_part->base) != 0) {
        printk("remap: APP-%c mapping failed\n", slot_suffix);
        return -1;
    }
    
    printk("remap: APP-%c mapped successfully (base=0x%08x -> Region B 0x%08x)\n",
           slot_suffix, app_part->base, REMAP_REGION_B_BASE);
    
    return 0;
}
