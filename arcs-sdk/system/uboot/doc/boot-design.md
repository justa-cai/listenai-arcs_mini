# Boot 启动方案设计文档

## 版本信息

| 版本 | 日期 | 说明 |
|------|------|------|
| 1.0 | 2024-12 | 初始版本 |
| 1.1 | 2024-12 | 支持A/B和A/OTA两种模式，删除recovery |
| 1.2 | 2024-12 | 简化A/B回滚机制，更新配置工具，优化地址映射 |
| 1.3 | 2025-03 | 启动环境存储从 EasyFlash 改为 lisa_kv 抽象接口 |

---

## 1. 概述

### 1.1 背景

本方案为新芯片设计的Boot启动方案，从老项目 `lsboot` 迁移并优化，主要特性：

- **双启动方案**：支持 A/B分区 和 A/OTA 两种模式二选一
- **硬件地址映射**：利用CIPHER控制器实现地址重映射（A/B模式）
- **TXZ压缩升级**：支持XZ压缩的OTA包
- **自动回滚机制**：A/B模式下启动失败自动回滚

### 1.2 两种启动模式对比

| 特性 | A/B分区模式 | A/OTA模式 |
|------|-------------|-----------|
| 固件数量 | 两套完整固件 | 一套固件 |
| 升级方式 | 写入非活动槽位 | 解压覆盖原分区 |
| 地址映射 | 需要（CIPHER Region B） | 不需要 |
| 回滚支持 | ✓ 简单回滚（活动槽失败切备用槽） | ✗ 不支持 |
| Flash占用 | 大（约2倍） | 小 |
| 适用场景 | Flash充足，需要高可靠性 | Flash有限 |

### 1.3 与老方案对比

| 特性 | 老方案 (lsboot) | 新方案 (boot) |
|------|----------------|---------------|
| AB分区 | 假AB (LMA!=VMA拷贝) | 真AB (硬件地址映射) |
| 配置存储 | syscfg + mbr + easyflash | bootconfig + lisa_kv (KV 存储抽象) |
| 分区表 | JSON格式 | 二进制结构体 |
| 配置工具 | 无 | partab_gen.py |
| 模式选择 | 无 | A/B 或 A/OTA 可选 |

---

## 2. 系统架构

### 2.1 A/B分区模式 Flash布局

```
┌─────────────────────────────────────────────────────────────────────┐
│                    A/B分区模式 Flash布局                             │
├────────────┬────────────────────────────────────────────────────────┤
│ 地址       │ 内容                                                    │
├────────────┼────────────────────────────────────────────────────────┤
│ 0x30000000 │ Boot (64KB)                                            │
├────────────┼────────────────────────────────────────────────────────┤
│ 0x30010000 │ BootConfig (4KB) - 分区表配置                           │
├────────────┼────────────────────────────────────────────────────────┤
│ 0x30011000 │ KV Store (8KB) - 启动环境变量 (lisa_kv)                 │
├────────────┼────────────────────────────────────────────────────────┤
│ 0x30013000 │ APP-A (1.5MB) - A槽位AP镜像        ─┐                  │
├────────────┼────────────────────────────────────  │                  │
│ 0x30193000 │ CP-A (512KB) - A槽位CP镜像          │ A槽位            │
├────────────┼────────────────────────────────────  │                  │
│ 0x30213000 │ RES-A (1MB) - A槽位资源            ─┘                  │
├────────────┼────────────────────────────────────────────────────────┤
│ 0x30313000 │ APP-B (1.5MB) - B槽位AP镜像        ─┐                  │
├────────────┼────────────────────────────────────  │                  │
│ 0x30493000 │ CP-B (512KB) - B槽位CP镜像          │ B槽位            │
├────────────┼────────────────────────────────────  │                  │
│ 0x30513000 │ RES-B (1MB) - B槽位资源            ─┘                  │
├────────────┼────────────────────────────────────────────────────────┤
│ 0x30613000 │ OTA (2MB) - OTA升级包存放区                             │
├────────────┼────────────────────────────────────────────────────────┤
│ 0x30813000 │ USER - 用户数据区                                       │
└────────────┴────────────────────────────────────────────────────────┘
```

### 2.2 A/OTA模式 Flash布局

```
┌─────────────────────────────────────────────────────────────────────┐
│                    A/OTA模式 Flash布局                               │
├────────────┬────────────────────────────────────────────────────────┤
│ 地址       │ 内容                                                    │
├────────────┼────────────────────────────────────────────────────────┤
│ 0x30000000 │ Boot (64KB)                                            │
├────────────┼────────────────────────────────────────────────────────┤
│ 0x30010000 │ BootConfig (4KB) - 分区表配置                           │
├────────────┼────────────────────────────────────────────────────────┤
│ 0x30011000 │ KV Store (8KB) - 启动环境变量 (lisa_kv)                 │
├────────────┼────────────────────────────────────────────────────────┤
│ 0x30013000 │ APP (2MB) - AP镜像 (唯一)                               │
├────────────┼────────────────────────────────────────────────────────┤
│ 0x30213000 │ CP (1MB) - CP镜像                                       │
├────────────┼────────────────────────────────────────────────────────┤
│ 0x30313000 │ RES (2MB) - 资源                                        │
├────────────┼────────────────────────────────────────────────────────┤
│ 0x30513000 │ OTA (3MB) - OTA升级包存放区 (较大)                      │
├────────────┼────────────────────────────────────────────────────────┤
│ 0x30813000 │ USER - 用户数据区                                       │
└────────────┴────────────────────────────────────────────────────────┘
```

### 2.3 地址映射原理（A/B模式）

```
      物理地址空间                    虚拟地址空间 (映射后)
  ┌─────────────────┐              ┌─────────────────┐
  │ 0x30013000 APP-A│◄──┐          │                 │
  ├─────────────────┤   │          │                 │
  │ 0x30193000 CP-A │   │  映射    │ 0x10000000 APP  │ ← Region B
  ├─────────────────┤   │  ════►   │ (基于APP偏移)   │
  │ 0x30213000 RES-A│   │          │ CP/RES/TONE等   │
  ├─────────────────┤   │(A槽位)   │ 都在Region B上  │
  │ 0x30313000 APP-B│   │          │                 │
  ├─────────────────┤   │          └─────────────────┘
  │ 0x30493000 CP-B │   │
  ├─────────────────┤   │
  │ 0x30513000 RES-B│   │
  └─────────────────┘   │
                         │
  只映射 APP 分区起始地址到 Region B (0x10000000)
  其他分区（CP、RES、TONE等）基于 APP 分区偏移访问
  所有分区映射后都在 Region B 地址空间内
```

---

## 3. 数据结构

### 3.1 BootConfig 结构 (boot_partab.h)

```c
/* 启动方案 */
typedef enum {
    BOOT_SCHEME_AB = 0,         /* A/B分区模式 */
    BOOT_SCHEME_OTA = 1,        /* A/OTA模式 */
} boot_scheme_t;

/* 分区标志位 */
#define PART_FLAG_VALID     (1 << 0)    /* 分区有效 */
#define PART_FLAG_BOOTABLE  (1 << 1)    /* 可引导 */
#define PART_FLAG_COMPRESS  (1 << 2)    /* txz压缩格式 */
#define PART_FLAG_ACTIVE    (1 << 3)    /* 当前活动 */
#define PART_FLAG_REMAP     (1 << 4)    /* 需要地址映射 */

/* 分区描述 */
typedef struct {
    char        name[16];       /* 分区名称 */
    uint32_t    base;           /* 物理地址 */
    uint32_t    size;           /* 分区大小 */
    uint32_t    exec;           /* 虚拟地址/执行地址 */
    uint32_t    flags;          /* 标志位 */
    uint32_t    crc32;          /* 数据CRC */
    uint8_t     reserved[7];    /* 预留 */
} partition_t;

/* Boot配置 */
typedef struct {
    uint32_t    magic;              /* 0x50415254 "PART" */
    uint32_t    version;            /* 版本号 */
    uint32_t    size;               /* 结构体大小 */
    uint32_t    crc32;              /* CRC校验 */

    uint32_t    kv_store_base;      /* KV存储地址 */
    uint32_t    kv_store_size;      /* KV存储大小 */

    uint8_t     boot_scheme;        /* 启动方案: BOOT_SCHEME_AB 或 BOOT_SCHEME_OTA */
    uint8_t     reserved1[3];

    uint32_t    part_count;         /* 分区数量 */
    uint32_t    reserved2;
    partition_t partitions[16];     /* 分区表 */
} boot_config_t;
```

### 3.2 启动环境变量 (lisa_kv)

通过 `lisa_kv` 接口访问，后端由 Kconfig 配置决定（EasyFlash 或 LSFS）。

| 键名 | 值 | 说明 |
|------|-----|------|
| `boot.mode` | "normal" / "update" | 启动模式 |
| `boot.active` | "A" / "B" | 当前活动槽位（A/B模式） |

---

## 4. 启动流程

### 4.1 总体启动流程图

```
                        ┌─────────────┐
                        │   上电复位   │
                        └──────┬──────┘
                               │
                        ┌──────▼──────┐
                        │  halt_cp()  │
                        └──────┬──────┘
                               │
                        ┌──────▼──────┐
                        │ syslog_init │
                        └──────┬──────┘
                               │
                        ┌──────▼──────┐
                        │boot_nvs_init│
                        └──────┬──────┘
                               │
                        ┌──────▼──────┐
                        │boot_config  │
                        │   _load()   │
                        └──────┬──────┘
                               │
                    ┌──────────┴──────────┐
                    │                     │
                  失败                   成功
                    │                     │
             ┌──────▼──────┐       ┌──────▼──────┐
             │ error_reboot│       │lisa_kv_init  │
             └─────────────┘       └──────┬──────┘
                                          │
                                   ┌──────▼──────┐
                                   │ get_scheme()│
                                   └──────┬──────┘
                                          │
                    ┌─────────────────────┼─────────────────────┐
                    │                                           │
              scheme=AB                                   scheme=OTA
                    │                                           │
             ┌──────▼──────┐                             ┌──────▼──────┐
             │boot_ab_     │                             │boot_ota_    │
             │  scheme()   │                             │  scheme()   │
             └─────────────┘                             └─────────────┘
```

### 4.2 A/B模式启动流程

```
                        ┌─────────────┐
                        │boot_ab_     │
                        │  scheme()   │
                        └──────┬──────┘
                               │
                        ┌──────▼──────┐
                        │ get_mode()  │
                        └──────┬──────┘
                               │
              ┌────────────────┴────────────────┐
              │                                 │
         mode=update                       mode=normal
              │                                 │
       ┌──────▼──────┐                          │
       │do_ab_txz_   │                          │
       │  update()   │                          │
       └──────┬──────┘                          │
              │                                 │
       ┌──────▼──────┐                          │
       │  成功? ────────► reboot()              │
       └──────┬──────┘                          │
              │ 失败                            │
              └─────────────┬───────────────────┘
                            │
                     ┌──────▼──────┐
                     │get_active   │
                     │   _slot()   │
                     └──────┬──────┘
                            │
                     ┌──────▼──────┐
                     │remap_app()  │  映射APP分区到Region B
                     └──────┬──────┘
                            │
                     ┌──────▼──────┐
                     │verify_app() │
                     └──────┬──────┘
                            │
              ┌─────────────┴─────────────┐
            失败                         成功
              │                           │
       ┌──────▼──────┐             ┌──────▼──────┐
       │try_backup   │             │jump_to_ap() │
       │  _slot()    │             │ (虚拟地址)  │
       └──────┬──────┘             └─────────────┘
              │
       ┌──────▼──────┐
       │两槽都失败?  │
       └──────┬──────┘
              │
       ┌──────▼──────┐
       │error_reboot │
       └─────────────┘
```

### 4.3 A/OTA模式启动流程

```
                        ┌─────────────┐
                        │boot_ota_    │
                        │  scheme()   │
                        └──────┬──────┘
                               │
                        ┌──────▼──────┐
                        │ get_mode()  │
                        └──────┬──────┘
                               │
              ┌────────────────┴────────────────┐
              │                                 │
         mode=update                       mode=normal
              │                                 │
       ┌──────▼──────┐                          │
       │do_ota_      │                          │
       │ decompress_ │  解压OTA覆盖所有分区     │
       │  update()   │                          │
       └──────┬──────┘                          │
              │                                 │
       ┌──────▼──────┐                          │
       │  成功? ────────► reboot()              │
       └──────┬──────┘                          │
              │ 失败                            │
              └─────────────┬───────────────────┘
                            │
                     ┌──────▼──────┐
                     │ find_app()  │  查找 "APP" 或 "APP-A"
                     └──────┬──────┘
                            │
                     ┌──────▼──────┐
                     │verify_app() │
                     └──────┬──────┘
                            │
              ┌─────────────┴─────────────┐
            失败                         成功
              │                           │
       ┌──────▼──────┐             ┌──────▼──────┐
       │error_reboot │             │jump_to_app()│
       └─────────────┘             │ (物理地址)  │
                                   └─────────────┘
```

---

## 5. TXZ升级流程

### 5.1 升级流程

**A/B模式**：升级到非活动槽位
```
OTA分区(txz) ──解压──► 非活动槽位分区 ──► 切换active ──► 重启
```

**A/OTA模式**：解压覆盖原分区
```
OTA分区(txz) ──解压──► 所有分区（根据文件名匹配） ──► 重启
```

### 5.2 文件映射规则

TAR 包中的文件名必须匹配分区表中的分区名称（不区分大小写，忽略 `.bin` 后缀）：

**示例**:
- 分区表中有 `AP` 分区 → TAR 包中需包含 `AP` 或 `ap.bin` 文件
- 分区表中有 `CP` 分区 → TAR 包中需包含 `CP` 或 `cp.bin` 文件
- 分区表中有 `RES` 分区 → TAR 包中需包含 `RES` 或 `res.bin` 文件

**实现逻辑** (`boot_ota_set_file_map`):
```c
/* 遍历所有有效分区（排除 OTA_TXZ） */
for (每个分区) {
    if (分区有效 && 分区名 != "OTA_TXZ") {
        文件映射[分区名] = 分区地址;
    }
}
```

**TAR 包结构示例**:
```
ota.tar:
  AP      (匹配分区表中的 "AP" 分区)
  CP      (匹配分区表中的 "CP" 分区)
  RES     (匹配分区表中的 "RES" 分区)
```

### 5.3 7z压缩命令

```bash
# 创建OTA包
tar -cvf ota.tar AP.bin CP.bin RES.bin

# XZ压缩 (字典128KB，与TXZ_DICT_MAX匹配)
7z a ota.txz ota.tar -mx9 -m0=LZMA2:d128k:fb273 -mmt-

# 参数说明:
# -mx9      : 最大压缩等级
# d128k     : 字典大小128KB (必须与 TXZ_DICT_MAX 匹配)
# fb273     : fast bytes最大值，提高压缩率
# -mmt-     : 单线程
```

---

## 6. A/B模式回滚机制

### 6.1 原理

A/B模式采用简单的双槽位回滚机制：

```
┌─────────────────────────────────────────────────────────────────────┐
│                    A/B模式回滚机制（简化版）                          │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   1. 尝试启动活动槽位（active slot）                                 │
│      - 映射 APP 分区到 Region B                                     │
│      - 校验镜像有效性                                               │
│      - 跳转到虚拟地址执行                                           │
│                                                                     │
│   2. 如果活动槽位启动失败                                           │
│      - 自动切换到备用槽位（backup slot）                            │
│      - 更新 KV 存储中的 boot.active                                 │
│      - 重新尝试启动                                                 │
│                                                                     │
│   3. 如果两个槽位都失败                                             │
│      - 打印错误信息                                                 │
│      - 重启系统                                                     │
│                                                                     │
│   注意：已删除3次重试确认机制，采用简单的双槽位切换                 │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### 6.2 实现逻辑

```c
/* boot/src/main.c */
static void boot_ab_scheme(boot_config_t *cfg)
{
    boot_slot_t active_slot = boot_env_get_active_slot();
    boot_slot_t backup_slot = (active_slot == BOOT_SLOT_A) ? BOOT_SLOT_B : BOOT_SLOT_A;

    /* 尝试启动活动槽位 */
    if (try_boot_slot(cfg, active_slot) == 0) {
        return; /* 成功，不会返回 */
    }

    /* 活动槽位失败，尝试备用槽位 */
    if (try_boot_slot(cfg, backup_slot) == 0) {
        boot_env_set_active_slot(backup_slot);  /* 更新活动槽位 */
        return; /* 成功，不会返回 */
    }

    /* 两个槽位都失败 */
    boot_error_reboot("Both slots failed");
}
```

---

## 7. 配置工具

### 7.1 配置工具

**工具位置**: `boottools/`

**功能**:
- 从 JSON 配置文件生成 `boot_cfg.bin`（4KB，分区表配置）
- 生成 `kv.bin`（8KB，KV 存储区域，固定大小）

**使用方法**:
```bash
cd boottools

# 使用默认 config.json
make all

# 或直接运行
./boottools config.json -m 0 --scheme ota

# 参数说明:
# -m <0|1>       启动模式: 0=normal, 1=update
# --scheme <ab|ota>  启动方案: ab 或 ota（覆盖 config.json）
```

**输出文件**:
- `boot_cfg.bin`: Boot配置（4KB，包含分区表和 KV 存储地址）
- `kv.bin`: KV 存储区域（8KB，固定大小）

### 7.2 JSON配置格式

**A/B模式示例 (config_ab.json)**:
```json
{
    "scheme": "ab",
    "kv_store_base": "0x30011000",
    "kv_store_size": "0x2000",

    "partitions": [
        {
            "name": "AP-A",
            "base": "0x30013000",
            "size": "0x180000",
            "exec": "0x10000000",
            "flags": ["valid", "bootable", "remap"],
            "crc32": "0x0"
        },
        {
            "name": "AP-B",
            "base": "0x30313000",
            "size": "0x180000",
            "exec": "0x10000000",
            "flags": ["valid", "bootable", "remap"],
            "crc32": "0x0"
        },
        {
            "name": "CP-A",
            "base": "0x30193000",
            "size": "0x80000",
            "exec": "0xFFFFFFFF",
            "flags": ["valid", "remap"],
            "crc32": "0x0"
        },
        {
            "name": "RES-A",
            "base": "0x30213000",
            "size": "0x100000",
            "exec": "0xFFFFFFFF",
            "flags": ["valid", "remap"],
            "crc32": "0x0"
        }
    ]
}
```

**注意**:
- `exec` 字段：APP 分区设置为 Region B 虚拟地址（0x10000000），其他分区设为 0xFFFFFFFF
- 所有带 `remap` 标志的分区都会基于 APP 分区偏移映射到 Region B
- `kv_store_size` 在配置中指定，但实际生成的 `kv.bin` 固定为 8KB

**A/OTA模式示例 (config_ota.json)**:
```json
{
    "scheme": "ota",
    "kv_store_base": "0x30011000",
    "kv_store_size": "0x2000",

    "partitions": [
        {
            "name": "AP",
            "base": "0x30013000",
            "size": "0x200000",
            "exec": "0x30013000",
            "flags": ["valid", "bootable"],
            "crc32": "0x0"
        },
        {
            "name": "CP",
            "base": "0x30213000",
            "size": "0x100000",
            "exec": "0xFFFFFFFF",
            "flags": ["valid"],
            "crc32": "0x0"
        },
        {
            "name": "RES",
            "base": "0x30313000",
            "size": "0x200000",
            "exec": "0xFFFFFFFF",
            "flags": ["valid"],
            "crc32": "0x0"
        },
        {
            "name": "OTA_TXZ",
            "base": "0x30513000",
            "size": "0x300000",
            "exec": "0xFFFFFFFF",
            "flags": ["valid"],
            "crc32": "0x0"
        }
    ]
}
```

**注意**:
- OTA 分区名称必须为 `OTA_TXZ`（用于识别和排除）
- 其他分区名称（如 `AP`、`CP`、`RES`）将用于 TAR 包文件匹配

### 7.3 配置字段说明

| 字段 | 类型 | 说明 |
|------|------|------|
| `scheme` | string | "ab" 或 "ota" |
| `kv_store_base` | hex | KV 存储起始地址 |
| `kv_store_size` | hex | KV 存储大小 |
| `partitions` | array | 分区列表 |

**分区字段**：

| 字段 | 类型 | 说明 |
|------|------|------|
| `name` | string | 分区名，最长15字符。TAR包中的文件名需匹配此名称 |
| `base` | hex | 物理地址 |
| `size` | hex | 分区大小 |
| `exec` | hex | 执行地址/虚拟地址。A/B模式APP设为0x10000000（Region B），其他为0xFFFFFFFF |
| `flags` | array | 标志: valid, bootable, compress, remap |
| `crc32` | hex | 分区数据CRC（可选，默认0） |

---

## 8. Kconfig 配置

```kconfig
# 启动方案 (运行时由分区表决定，这里是默认值)
CONFIG_BOOT_AB_PARTITION=y

# TXZ升级
CONFIG_BOOT_OTA_PACKAGE=y
CONFIG_TXZ_DICT_MAX=128

# 地址映射 (A/B模式需要，只使用 Region B)
CONFIG_BOOT_REMAP=y
CONFIG_AP_VIRTUAL_ADDRESS=0x10000000  /* Region B 基址 */

# Boot配置区地址
CONFIG_BOOT_CONFIG_BASE=0x30010000

# KV 存储抽象层
CONFIG_LISA_KV=y
CONFIG_LISA_KV_TYPE_EF=y        # 默认后端: EasyFlash
# CONFIG_LISA_KV_TYPE_LSFS=y    # 可选后端: LSFS（文件系统）
```

**注意**:
- 已删除 `CONFIG_BOOT_MAX_RETRY`（不再使用重试计数机制）
- 地址映射只使用 Region B（0x10000000），其他分区基于 APP 偏移访问

---

## 9. AP/CP Linker配置

### A/B模式（使用虚拟地址）

**AP Linker (ap.ld)**:
```c
MEMORY {
    FLASH(rxa!w) : ORIGIN = 0x10000000, LENGTH = 1536K  /* Region B 虚拟地址 */
    RAM(rwxa)    : ORIGIN = 0x20000000, LENGTH = 704K
}
```

**CP Linker (cp.ld)**:
```c
MEMORY {
    FLASH(rxa!w) : ORIGIN = 0x10000000, LENGTH = 512K   /* 基于 APP 偏移，也在 Region B */
    RAM(rwxa)    : ORIGIN = 0x20100000, LENGTH = 256K
}
```

**注意**:
- 所有分区都映射到 Region B（0x10000000）地址空间
- CP、RES、TONE 等分区通过相对于 APP 的偏移访问

### A/OTA模式（使用物理地址）

**AP Linker (ap.ld)**:
```c
MEMORY {
    FLASH(rxa!w) : ORIGIN = 0x30013000, LENGTH = 2048K  /* 物理地址 */
    RAM(rwxa)    : ORIGIN = 0x20000000, LENGTH = 704K
}
```

---

## 10. 文件清单

### 10.1 源文件

| 文件 | 说明 |
|------|------|
| `main.c` | 主程序，启动流程控制 |
| `boot_partab.h/c` | 分区表结构和操作 |
| `boot_nvs.h/c` | Flash NVS操作封装 |
| `boot_env.h/c` | KV 启动环境管理（基于 lisa_kv） |
| `boot_ota.h/c` | TXZ升级实现 |
| `boot_remap.h/c` | 地址映射实现 |
| `boot_crc32.h` | CRC32适配层 |
| `lisa_kv` | KV 存储抽象层（由 arcs-sdk 组件管理） |
| `xzdec/*` | XZ解压库 |

### 10.2 工具

| 文件 | 说明 |
|------|------|
| `tools/partab_gen.py` | JSON转二进制分区表 |
| `tools/config_ab.json` | A/B模式配置示例 |
| `tools/config_ota.json` | A/OTA模式配置示例 |

---

## 11. API参考

### 11.1 启动方案

```c
/* 获取启动方案 */
boot_scheme_t boot_config_get_scheme(boot_config_t *cfg);
// 返回 BOOT_SCHEME_AB 或 BOOT_SCHEME_OTA
```

### 11.2 分区操作

```c
/* 加载分区表 */
boot_config_t *boot_config_load(void);

/* 查找分区 */
partition_t *boot_partition_find(boot_config_t *cfg, const char *name);
partition_t *boot_partition_find_app(boot_config_t *cfg, boot_slot_t slot);
partition_t *boot_partition_find_ota(boot_config_t *cfg);

/* 校验APP镜像 */
bool boot_app_verify(partition_t *part);

/* 引导APP */
void boot_app_jump(partition_t *part);
```

### 11.3 环境变量

内部通过 `lisa_kv_get_string` / `lisa_kv_set_string` 实现，不再直接依赖具体 KV 后端。

```c
/* 启动模式 (内部使用 lisa_kv_get_string/set_string("boot.mode", ...)) */
boot_mode_t boot_env_get_mode(void);
int boot_env_set_mode(boot_mode_t mode);

/* 槽位管理 (内部使用 lisa_kv_get_string/set_string("boot.active", ...)) */
boot_slot_t boot_env_get_active_slot(void);
int boot_env_set_active_slot(boot_slot_t slot);
```

### 11.4 地址映射 (A/B模式)

```c
/* 映射 APP 分区到 Region B */
int boot_remap_app(uint32_t app_physical_addr);

/* 根据槽位映射 APP 分区 */
int boot_remap_all_partitions(boot_config_t *cfg, boot_slot_t slot);

/* 禁用 Region B 映射 */
void boot_remap_disable(void);
```

**注意**:
- 只映射 APP 分区的起始地址到 Region B（0x10000000）
- 其他分区（CP、RES、TONE等）基于 APP 分区偏移访问，都在 Region B 地址空间内

---

## 12. 错误处理

所有错误统一调用 `boot_error_reboot()` 打印后重启：

```c
static void boot_error_reboot(const char *msg)
{
    printk("BOOT ERROR: %s\n", msg);
    printk("System will reboot...\n");
    chip_reboot();
}
```

**可能的错误**：
- NVS初始化失败
- 分区表无效
- KV 存储初始化失败
- APP分区未找到
- APP校验失败
- 地址映射失败
- 两个槽位都启动失败（A/B模式）

---

## 附录A：完整示例

### A.1 AP端正常启动

```c
void app_main(void)
{
    // 基础初始化
    hal_init();

    // 关键功能自检
    if (wifi_init() != 0 || audio_init() != 0) {
        system_reboot();  // 失败，重启（A/B模式会自动切换到备用槽位）
        return;
    }

    // 正常业务
    main_loop();
}
```

**注意**: A/B模式已删除启动确认机制，如果AP启动失败（看门狗复位或崩溃），bootloader会在下次启动时自动切换到备用槽位。

### A.2 AP端触发OTA

```c
void handle_ota_request(const char *url)
{
    uint32_t ota_offset = OTA_STAGING_OFFSET;
    uint32_t ota_size;

    // 下载完整 OTA 包到 FLASH staging 区
    ota_size = download_to_flash(url, ota_offset);

    // 只写 OTA 请求，不直接操作 boot.mode 或内部 KV key
    uboot_ota_start_from_flash(ota_offset, ota_size);
    sys_reboot(SYS_REBOOT_SOFT);
}
```

**说明**:

- APP 只负责放置完整 OTA 包并发起升级请求
- BOOT 下次启动后读取 OTA 请求，再根据静态分区表按文件名匹配 OTA 包中的镜像
- 目标分区由 boot 分区表决定，APP 不需要传入目标地址或分区列表
- 如果需要变更升级内容，应重新生成并替换整个 OTA 包

### A.3 生成配置和分区表

```bash
# 1. 编辑配置文件
cd boottools
vim config.json

# 2. 生成二进制文件
make all
# 或
./boottools

# 3. 输出文件
# - boot_cfg.bin: 烧录到 BootConfig 地址（如 0x30010000）
# - kv.bin: 烧录到 KV 存储地址（如 0x30011000）
```
