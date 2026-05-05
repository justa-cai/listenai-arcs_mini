# U-Boot 引导加载程序

## 简介

U-Boot 是 ARCS SDK 的系统级引导加载程序，负责设备启动与应用加载。boot_f 是引导的第一阶段，完成硬件初始化和应用镜像校验后跳转到应用；当检测到 Recovery 请求或需要执行 OTA 升级时，boot_f 进入 boot_s（基于 FreeRTOS 的第二阶段），由 boot_s 完成 ADB Recovery 或 OTA 流程。

## 功能特性

- **应用引导**：boot_f 校验应用镜像 header，确认有效后直接跳转；校验失败则进入 boot_s recovery
- **OTA 升级**：在 boot_s 中执行，基于 TXZ (LZMA2) 压缩的 TAR 包，两遍处理（校验 + 写入），支持从 Flash 或 TF 卡读取 OTA 包
- **ADB Recovery**：在 boot_s 中通过 ADB 协议更新 Flash / TF 卡上的固件和资源，支持 TinyUSB / CherryUSB USB 后端，可选 Shell 交互与文件推送/拉取
- **看门狗**：使用 CP 核 WDT，boot_f 阶段可启用看门狗检测启动超时；boot_s 中持续喂狗，超时触发全芯片复位并在下次启动进入 recovery
- **地址重映射**：通过硬件虚地址映射将 APP 分区偏移映射到固定地址空间（Region B）

## 启动流程

```
boot_f (第一阶段，Flash XIP)
  ├─ 硬件初始化（时钟、ECLIC、可选 PSRAM）
  ├─ 校验应用镜像 header
  ├─ 检查 recovery 条件（AON 寄存器 + Flash 持久标志）
  │   ├─ 无 recovery 请求 → 跳转到应用
  │   └─ 有 recovery 请求 → 进入 boot_s
  └─ boot_s (FreeRTOS, 需要 CONFIG_BOOT_SECOND_STAGE)
      ├─ recovery 原因为 ADB？ → 启动 ADB Runtime（USB 枚举，等待主机连接）
      └─ boot mode 为 OTA？ → 加载 OTA 请求，执行 TXZ 升级流程，完成后重启
```

**触发 boot_s 的条件**（来自 `boot_stage_gate.c`）：

- `boot_info.req == 1`（应用侧软件 recovery 请求）
- `boot_info.boot_wdt != 0`（boot 看门狗超时）
- `boot_info.handshake_timeout != 0`（AP 看门狗超时）
- `boot_config.recover != 0`（Flash 持久化 recovery 标志）
- `boot_info.reserved != 0`（OTA 升级挂起）

## 目录结构

```
uboot/
├── src/              # 引导核心实现（boot、OTA、ADB、配置存储等）
├── api/              # 对外 API 实现（OTA、Recovery、WDT）
├── include/          # 公共头文件
├── cmake/            # 构建脚本
├── linker/           # 链接脚本
├── tools/            # 分区表生成、OTA 打包等工具脚本
├── test/             # 单元测试与集成测试
├── doc/              # 设计文档（架构、存储布局、OTA 源选择）
├── Kconfig           # 根 Kconfig
├── Kconfig.boot      # 引导阶段专用配置
└── Kconfig.public    # 对应用可见的公共配置
```

## 配置选项

通过 `menuconfig` 配置以下关键选项：

### OTA

| 选项 | 说明 | 默认值 |
|------|------|--------|
| `CONFIG_BOOT_OTA_PACKAGE` | 启用 OTA 升级功能（TAR/TXZ 解包写分区，自动启用二阶段） | n |
| `CONFIG_BOOT_OTA_SOURCE_TF` | 允许从 TF 卡读取 OTA 包 | n |
| `CONFIG_BOOT_CONTROL_STORE_FLASH` | 使用 Flash 存储 OTA 控制信息 | n |

### ADB Recovery

| 选项 | 说明 | 默认值 |
|------|------|--------|
| `CONFIG_BOOT_ADB` | 启用 ADB Recovery（自动启用二阶段） | n |
| `CONFIG_BOOT_ADB_BACKEND_TINYUSB` | 使用 TinyUSB 作为 USB 后端 | y (默认) |
| `CONFIG_BOOT_ADB_BACKEND_CHERRYUSB` | 使用 CherryUSB 作为 USB 后端 | n |
| `CONFIG_BOOT_ADB_SHELL` | 启用 ADB Shell（依赖 Letter Shell） | n |
| `CONFIG_BOOT_ADB_SYNC` | 启用 ADB 文件推送/拉取 | n |
| `CONFIG_BOOT_ADB_SDMMC_RAW` | 启用 RAW SDMMC 访问（`/RAW/SDRAW/...`） | n |
| `CONFIG_BOOT_ADB_SDMMC_FS` | 挂载 TF 卡 FAT 文件系统（默认根 `/SD:/adb/`） | n |

### 看门狗

| 选项 | 说明 | 默认值 |
|------|------|--------|
| `CONFIG_BOOT_WDT` | 启用 boot 阶段看门狗（CP 核 WDT） | n |
| `CONFIG_BOOT_WDT_BEFORE_JUMP` | 跳转应用前启用看门狗 | n |

### 日志

| 选项 | 说明 | 默认值 |
|------|------|--------|
| `CONFIG_BOOT_LOG_LEVEL` | 日志级别（NON/ERR/WRN/INF/DBG） | INF |
| `CONFIG_BOOT_EARLY_LOG` | 启用 boot_f 阶段早期日志 | n |

## API 参考

以下 API 供应用侧调用，无需构建完整 boot 镜像即可链接。

### OTA API (`uboot_ota_api.h`)

```c
int uboot_ota_start_from_flash(uint32_t offset, uint32_t size);
int uboot_ota_start_from_tf(const char *path);
int uboot_ota_start_from_ota_partition(void);
int uboot_ota_start(const struct uboot_ota_request *req);
int uboot_ota_get_last_failure(uboot_ota_failure_info_t *info);
```

### Recovery API (`uboot_recovery_api.h`)

```c
int uboot_recovery_request(uboot_recovery_mode_t mode);
```

### Watchdog API (`uboot_wdt_api.h`)

```c
int uboot_wdt_feed(void);
int uboot_wdt_enable(void);
int uboot_wdt_disable(void);
```

## 构建依赖

- **boot_f（始终需要）**：arcs-hal、libc、libm
- **boot_s（二阶段）**：+ freertos、heap
- **ADB Sync / TF OTA**：+ lsfs、disk
- **ADB Shell**：+ letter-shell
- **日志**：+ easylogger

## 设计文档

```{toctree}
:maxdepth: 1

doc/boot-design.md
doc/boot-storage-layout.md
doc/BOOT_OTA_SOURCE_SELECTION_DESIGN.md
```
