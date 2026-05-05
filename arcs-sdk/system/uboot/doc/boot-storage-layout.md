# Boot 存储布局

## 概述

Boot 使用**混合存储策略**：寄存器用于快速访问的易失性状态，Flash 用于需要持久化的数据。

两块独立的存储区域：
- **寄存器**: REG_AON_DIG_RSVD4 (32-bit) — 启动模式、Recovery 请求等
- **Flash boot_config**: 0x3003d000 (4KB) — Recovery 硬件模式持久化
- **Flash boot_control_store**: CONFIG_BOOT_CONTROL_STORE_BASE_ADDR (4KB) — OTA 请求参数

---

## 寄存器布局 (REG_AON_DIG_RSVD4)

32-bit AON (Always-On) 寄存器，软件复位后保持，掉电丢失。

```
Bit  31  30  29  28 ················ 16  15 ········ 8  7 ········· 0
    ┌───┬───┬───┬───────────────────────┬─────────────┬──────────────┐
    │req│wdt│hst│     reserved (13)     │recover_rsn  │  reboot_cnt  │
    │   │   │   │                       │   (8-bit)   │   (8-bit)    │
    └─┬─┴─┬─┴─┬─┴──────────┬───────────┴─────────────┴──────────────┘
      │   │   │             │
      │   │   │   reserved 13-bit 子字段:
      │   │   │   ┌─────────────────────────────────────────────┐
      │   │   │   │ [0]    boot_mode       (1-bit)              │
      │   │   │   │        0 = BOOT_MODE_NORMAL                 │
      │   │   │   │        1 = BOOT_MODE_UPDATE                 │
      │   │   │   │ [1:4]  upgrade_status  (4-bit)              │
      │   │   │   │        0 = NONE                             │
      │   │   │   │        1 = IN_PROGRESS                      │
      │   │   │   │        2 = SUCCESS                          │
      │   │   │   │        3 = FAILED                           │
      │   │   │   │ [5:12] reserved        (8-bit, 未来扩展)    │
      │   │   │   └─────────────────────────────────────────────┘
      │   │   │
      │   │   └── [29] handshake_timeout  — Boot 握手超时标志
      │   └────── [30] boot_wdt           — Boot 看门狗标志
      └────────── [31] req                — Recovery 软件请求 (1=请求进入)
```

### 字段说明

| 字段 | 位域 | 写入方 | 读取方 | 用途 |
|------|------|--------|--------|------|
| reboot_cnt | [7:0] | Boot | Boot | 重启计数 |
| recover_reason | [15:8] | Boot | Boot/App | Recovery 原因枚举 |
| boot_mode | reserved[0] | App | Boot | OTA 启动模式 |
| upgrade_status | reserved[4:1] | App/Boot | App/Boot | OTA 升级状态跟踪 |
| handshake_timeout | [29] | Boot | Boot | AP/CP 握手超时 |
| boot_wdt | [30] | Boot | Boot | 看门狗超时标志 |
| req | [31] | App | Boot | Recovery 软件请求 |

### recover_reason 枚举

```c
enum {
    RECOVER_REASON_NONE = 0,
    RECOVER_REASON_SOFT_REQ,        // 软件请求
    RECOVER_REASON_HARD_REQ,        // 硬件请求 (Flash 持久化)
    RECOVER_REASON_APP_INVALID,     // APP 镜像无效
    RECOVER_REASON_AP_WDT_TIMEOUT,  // AP 看门狗超时
    RECOVER_REASON_BOOT_WDT_TIMEOUT,// Boot 看门狗超时
};
```

---

## Flash 布局

### boot_config (0x3003d000, 4KB)

Recovery 硬件模式的持久化存储，由 `boot_config.c` 管理。

```
0x3003d000 ┌──────────────────────────────────────┐
           │ boot_config                          │
           │   tag      (4B)  — 配置标识          │
           │   version  (4B)  — 版本号            │
           │   size     (4B)  — 数据长度          │
           │   sum      (4B)  — 校验和            │
           │   recover  (1B)  — Recovery 标志     │
           │   ...                                │
0x3003dfff └──────────────────────────────────────┘
```

### boot_control_store (CONFIG_BOOT_CONTROL_STORE_BASE_ADDR, 4KB)

OTA 请求参数的持久化存储，由 `boot_control_store_flash.c` /
`boot_control_store_lisa_flash.c` 管理。

默认地址: Kconfig `CONFIG_BOOT_CONTROL_STORE_BASE_ADDR` (示例值 0x30700000)

```
+0x0000 ┌──────────────────────────────────────┐
        │ magic       (4B)  = 0x42435453       │ "BCTS"
        │ version     (4B)  = 1                │
        ├──────────────────────────────────────┤
        │ ota_request (144B)                   │
        │   magic     (4B)  = 0x5154414F       │ "OTAQ"
        │   version   (4B)  = 1                │
        │   source    (4B)  FLASH(0) / TF(1)   │
        │   pkg_size  (4B)  OTA 包大小          │
        │   union     (128B)                   │
        │     flash_offset (4B)  ← Flash OTA   │
        │     path[128]         ← TF 卡 OTA    │
        ├──────────────────────────────────────┤
        │ crc32       (4B)  数据完整性校验       │
        ├──────────────────────────────────────┤
        │ reserved    (3940B) 未来扩展          │
+0x0FFF └──────────────────────────────────────┘
```

### 校验机制

- **magic**: 0x42435453 ("BCTS")，用于判断 Flash 是否已初始化
- **version**: 1，用于未来格式升级时的兼容判断
- **crc32**: 覆盖 magic + version + ota_request，检测数据损坏
- Flash 全擦后为 0xFF，magic 校验失败，自动 fallback 到 BOOT_MODE_NORMAL

---

## 功能-存储对照表

| 功能 | 存储位置 | 大小 | 持久化 | 说明 |
|------|---------|------|--------|------|
| OTA 启动模式 | 寄存器 reserved[0] | 1-bit | ❌ | Boot 快速判断是否需要 OTA |
| OTA 升级状态 | 寄存器 reserved[1:4] | 4-bit | ❌ | 跟踪升级进度 |
| OTA 请求参数 | Flash boot_control_store | 4KB | ✅ | 包来源、地址、大小 |
| Recovery 软件请求 | 寄存器 req | 1-bit | ❌ | 临时进入，重启后自动清除 |
| Recovery 硬件请求 | Flash boot_config | 4KB | ✅ | 持久保存，需手动退出 |

---

## Boot 启动时的读取顺序

```
1. 读寄存器 REG_AON_DIG_RSVD4
   ├── req == 1 ?  ──────────→ 进入 Recovery (软件模式)
   └── boot_mode == UPDATE ? ─→ 读 Flash boot_control_store
                                  ├── magic/crc 有效 → 执行 OTA 升级
                                  └── 无效 → 降级为 NORMAL

2. 读 Flash boot_config
   └── recover == true ? ─────→ 进入 Recovery (硬件模式)

3. 以上都不满足 → BOOT_MODE_NORMAL，跳转到 APP
```

---

## 实现文件

| 文件 | 作用 | Flash API |
|------|------|-----------|
| `src/boot_control_store_flash.c` | Boot 内部实现 | boot_flash.h (HAL 直接访问) |
| `api/boot_control_store_lisa_flash.c` | 对外 API 实现 | lisa_flash.h (设备框架) |
| `src/boot_control_store_none.c` | 无存储桩实现 | — |
| `src/boot_control_store_test.c` | 内存测试实现 | — |
| `src/boot_config.c` | Recovery 持久化 | boot_flash.h |
