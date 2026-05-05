# Boot OTA Source Selection Design

## 版本信息

| 版本 | 日期 | 说明 |
|------|------|------|
| 0.1 | 2026-03-30 | 初版，补充 APP 触发的 OTA 源选择设计 |
| 0.2 | 2026-03-30 | request 存储改为抽象后端，具体实现编译期选择，默认 EasyFlash |
| 0.3 | 2026-04-01 | 明确 APP 只负责放置 OTA 包并写升级请求 |
| 0.4 | 2026-04-01 | 调整为固定分区方案：BOOT 按静态分区表匹配包内文件名，不使用包内 config 决定写入目标 |
| 0.5 | 2026-04-01 | 固化当前共识：OTA 包内不放 config/manifest，包内只包含固件文件，目标分区完全由 partab 决定 |

---

## 1. 背景

现有 `system/uboot` 已具备以下基础能力：

- 分区表和启动模式抽象：`boot_partab.[ch]`
- 启动环境：`boot_env.[ch]`
- `A/B` 与 `A/OTA` 启动流程设计：`doc/boot-design.md`
- `txz/xz + tar` 解包写分区：`boot_ota.[ch]`

当前缺口是：

- 只有 `boot.mode=update` 这一层升级模式开关，没有“固件来源”协议
- `A/OTA` 更新默认从固定 OTA 分区读取，不支持 APP 动态指定升级包位置
- `boot_ota.c` 当前输入模型是 `src_addr + src_size` 的 flash 裸地址流，不能直接读取 TF 文件

本设计在不推翻现有 `boot-design.md` 的前提下，扩展一套“APP 调 uboot API 发起升级”的补充方案。

---

## 2. 目标

### 2.1 功能目标

- 上层 APP 调用 uboot 提供的公共 API 发起升级
- API 支持两种 OTA 源：
  - `FLASH`：通过 flash 偏移和包大小指定
  - `TF`：通过文件系统路径指定
- OTA 包只负责提供待升级镜像文件，目标分区由静态分区表决定
- APP 发起升级后，持久化升级请求，置 boot 为升级模式，然后重启
- uboot 重启进入升级模式后，根据请求信息读取指定 OTA 包并完成升级
- 升级包格式支持：
  - `txz`：`xz` 解压后得到 `tar`
  - `tar`：直接解析，不走 `xz`
- 升级失败后保留升级请求，后续重启继续尝试

### 2.2 非目标

- 本轮不设计网络下载协议
- 本轮不修改分区打包规则和分区表格式
- APP 不通过 API 向 BOOT 传入分区写入计划
- 本轮不实现复杂重试退避或失败计数上报

### 2.3 当前确认的包语义

本设计最终采用“固定分区 + 文件名匹配”模型，明确如下：

- `ota.tar` / `ota.txz` 内不包含 `config`、`manifest` 或 `copy_to` 描述
- OTA 包内只包含待升级的固件文件本体
- 目标分区、目标地址、分区大小完全由 boot 加载的静态分区表决定
- APP 不修改 OTA 包内部语义，也不向 BOOT 传递“写到哪”的运行时计划
- BOOT 仅根据“分区表中的有效分区名”与“包内文件名”建立写入映射

因此，若分区表里只有 `AP` 一个可升级分区，则 OTA 包内只放 `AP.bin` 即可；若分区表里还有
`CP`、`RES` 等可升级分区，则包内可继续加入 `CP.bin`、`RES.bin` 等对应镜像。

---

## 3. 设计原则

1. 保留现有 `boot.mode=update` 作为“是否进入升级路径”的总开关
2. 新增“boot-owned request”作为“去哪里取 OTA 包”的唯一事实来源
3. 对 APP 暴露公共 API，但不让 APP 直接操作 uboot 内部细节
4. 尽量复用当前 `boot_ota.c` 的 tar/xz 处理逻辑，只替换输入源
5. TF 文件读取优先复用 SDK 现有 `fs/disk/sdmmc` 能力，不复制旧仓库 `src/fs` 整棵代码
6. request 存储层只依赖抽象接口，具体 backend 由编译配置选择，确保 APP 与 BOOT 使用同一实现
7. APP 只负责“把 OTA 包放到哪里”和“下次启动要不要升级”，BOOT 负责根据静态分区表决定写入计划

---

## 4. 总体方案

### 4.1 分层

新增三层能力：

1. **APP 侧公共 API**
   - 供应用调用
   - 负责校验参数、写入升级请求、设置升级模式、触发重启

2. **升级请求持久化层**
   - 负责定义请求字段、保存、加载、校验、清理
   - 通过 store backend 抽象隔离底层存储实现
   - 为 boot 和 APP 提供统一协议

3. **升级源读取层**
   - 为 `boot_ota.c` 提供统一 reader
   - 屏蔽 `FLASH` 和 `TF` 两种读取方式差异

### 4.2 APP 与 BOOT 的职责边界

- APP 负责：
  - 下载、生成或替换完整的 `ota.tar` / `ota.txz`
  - 将 OTA 包放到指定来源介质
    - `FLASH`：写入 staging 区并记录 offset/size
    - `TF`：写入文件系统并记录文件路径
  - 不修改 OTA 包内部目标分区语义，包内只保留固件文件
  - 调用 `uboot_ota_start*()` 写入升级请求
  - 触发重启
- BOOT 负责：
  - 读取 `boot.mode` 和 OTA request
  - 根据 request 打开 OTA 包
  - 加载静态分区表，遍历所有有效分区并排除 `OTA_TXZ`
  - 用分区名匹配包内文件名，确定目标分区
  - 校验包内文件大小不超过对应分区大小
  - 执行升级，并维护 request 与 `boot.mode` 生命周期
- 明确不推荐的做法：
  - APP 通过 API 直接传入“目标分区列表、目标地址列表”
  - APP 直接操作 BOOT 内部 key，而不是走公共 API

如果业务侧需要变更升级内容，应重新生成并替换整个 OTA 包。

### 4.3 启动时序

```text
APP
  -> 下载/生成新的 ota.tar 或 ota.txz
  -> 写入 FLASH staging 区或 TF 文件
  -> uboot_start_ota_from_flash()/uboot_start_ota_from_tf()
  -> 保存 ota request
  -> 设置 boot.mode=update
  -> reboot

uboot
  -> boot_env_get_mode()
  -> mode == update
  -> boot_ota_request_load()
  -> boot_ota_source_open()
  -> 检测 txz 或 tar
  -> 加载 boot 分区表
  -> 建立 “分区名 -> 目标分区” 映射（排除 OTA_TXZ）
  -> 逐个匹配 tar 内文件名与分区名
  -> 执行分区写入
  -> success: clear request + set mode normal + reboot
  -> fail: keep request + keep update mode + reboot
```

---

## 5. 公共 API 设计

### 5.1 API 语义

这里的“uboot API”不是 APP 直接调用 boot 镜像里的运行时符号，而是：

- 由 `system/uboot` 定义一套公共头文件和协议
- 在 APP 构建侧编译一个小型 client API
- API 内部写入 uboot 消费的持久化请求

这样既满足“由 uboot 提供 API 给 APP 调用”，又避免 APP 与 boot 内部 key/字段硬编码耦合。

同时，公共 API 的边界固定为“写升级请求”，不暴露“升级哪些镜像、各镜像写到哪里”这类运行时参数；这些信息由静态分区表和包内文件名匹配规则共同决定。

### 5.2 建议接口

```c
typedef enum {
    UBOOT_OTA_SOURCE_FLASH = 0,
    UBOOT_OTA_SOURCE_TF = 1,
} uboot_ota_source_t;

#define UBOOT_OTA_PATH_MAX 128

typedef struct {
    uboot_ota_source_t source;
    uint32_t package_size;
    union {
        struct {
            uint32_t flash_offset;
        } flash;
        struct {
            char path[UBOOT_OTA_PATH_MAX];
        } tf;
    };
} uboot_ota_request_t;

int uboot_ota_start(const uboot_ota_request_t *req);
int uboot_ota_start_from_flash(uint32_t flash_offset, uint32_t package_size);
int uboot_ota_start_from_tf(const char *path);
int uboot_ota_start_from_ota_partition(void);
```

其中 `uboot_ota_start_from_ota_partition()` 是固定分区 A/OTA 场景的便捷接口：

- APP 不需要关心 `OTA_TXZ` 分区地址和大小
- API 内部从静态分区表查找 `OTA_TXZ`
- 然后转换成 flash request，再复用 `uboot_ota_start_from_flash()`

### 5.3 参数约束

- `FLASH`
  - `flash_offset` 为相对 `CONFIG_MEM_FLASH_BASE` 的偏移
  - `package_size` 必须大于 0
- `TF`
  - `path` 必须是绝对挂载路径，默认使用 `/SD:/...`
  - `package_size` 可选
  - 若 APP 无法提前获知文件大小，可由 boot 打开文件后自行获取

---

## 6. 升级请求持久化协议

### 6.1 现状

当前 `boot_env.[ch]` 仅定义了：

- `boot.mode`
- `boot.active`

参考：

- [boot_env.h](/home/openclaw/Work/ARCS/arcs-sdk/system/uboot/src/boot_env.h)
- [boot_env.c](/home/openclaw/Work/ARCS/arcs-sdk/system/uboot/src/boot_env.c)

### 6.2 存储抽象

request 层不直接暴露 EasyFlash key 细节，而是定义 boot-owned store 抽象：

- `boot_control_store_save()`
- `boot_control_store_load()`
- `boot_control_store_clear()`
- `boot_control_store_set_mode()`

APP API 和 BOOT request loader 都只依赖这层抽象，具体 backend 在编译期由 boot Kconfig 选定。

request record 只保存以下最小信息：

- OTA 来源类型
- OTA 包位置
  - `FLASH`：offset + size
  - `TF`：path

request record 不保存分区列表，也不保存写入计划。

### 6.3 默认后端

默认实现使用 EasyFlash。

当前建议实现方式：

- request 本体以单条 blob 形式保存
- `boot.mode` 仍保持 `boot.mode=update`
- APP 侧 API 先保存 request，再设置 `boot.mode`

这样可以把 boot 内部存储格式集中在 backend 实现里，避免 APP 侧硬编码内部 key 组合。

### 6.4 有效性与清理规则

boot 端只在以下条件同时满足时视为有效请求：

- `boot.mode == update`
- request record 能成功加载
- request version 可识别
- 源字段完整且可解析

清理规则：

- 升级成功：
  - 清 request record
  - `boot.mode <- normal`
- 升级失败：
  - 保留 request record
  - 保留 `boot.mode=update`

---

## 7. uboot 启动状态机修改

### 7.1 A/OTA 模式修改点

当前 `boot_ota_scheme()` 在升级模式下直接从固定 OTA 分区取包：

- [main.c](/home/openclaw/Work/ARCS/arcs-sdk/system/uboot/src/main.c)

目标改为：

1. `boot_env_get_mode()`
2. `boot_ota_request_load()`
3. 请求合法则按 `source` 打开 reader
4. 加载静态分区表并建立文件映射
5. 执行 `tar/txz` 升级
5. 成功则清请求，失败则保留请求

### 7.2 A/B 模式修改点

本轮重点是 `A/OTA` 源选择，但同一套 request 协议不应限制未来在 `A/B` 模式复用。

因此设计时应保证：

- `boot_ota_request_t` 与 `boot_scheme` 解耦
- `FLASH/TF` reader 可被 `A/B` 和 `A/OTA` 共用

---

## 8. 升级源读取抽象

### 8.1 Reader 接口

`boot_ota.c` 当前假设输入是 flash 地址范围。建议新增顺序 reader 抽象：

```c
typedef struct boot_ota_reader boot_ota_reader_t;

typedef struct {
    int (*open)(boot_ota_reader_t *reader, const boot_ota_request_t *req);
    int (*read)(boot_ota_reader_t *reader, void *buf, uint32_t len, uint32_t *out_len);
    int (*close)(boot_ota_reader_t *reader);
    uint32_t total_size;
    uint32_t position;
} boot_ota_reader_ops_t;
```

对 `boot_ota.c` 的要求只有：

- 顺序读取
- 获取总大小
- 关闭 reader

因此无需引入随机 seek 抽象。

### 8.2 FLASH reader

- 输入：`flash_offset + package_size`
- 物理地址：`CONFIG_MEM_FLASH_BASE + flash_offset`
- 读取方式：复用 `boot_nvs_read()`

### 8.3 TF reader

输入路径默认采用 `/SD:/path/to/ota.txz`。

启动时需要完成：

1. `sdmmc_hard_init()`
2. `disk_init(NULL)`
3. 初始化并挂载 FS
4. 打开 TF 文件
5. 顺序 `f_read()`

实现上优先复用：

- `modules/fs/disk/sdmmc.c`
- `modules/fs/subfs/fatfs`
- `modules/fs/lsfs` / `lvfs` 或直接最小化使用 FatFS API

不建议把旧仓库 `/home/openclaw/Work/ARCS/boot/src/fs` 直接整包复制到 `system/uboot`。

---

## 9. OTA 包内容与匹配规则

### 9.1 固定分区映射

`A/OTA` 模式下，BOOT 不从 OTA 包中读取“目标地址”或“目标分区列表”。目标写入位置完全由 boot 分区表决定。

这里再明确一次：

- OTA 包内没有 `config`
- OTA 包内没有 `manifest`
- OTA 包内没有 `copy_to.addr` / `copy_to.size`
- BOOT 不信任包内提供的目标地址
- 唯一合法的写入目标来源是 `partab`

以如下分区表示例为例：

```json
{
    "scheme": "ota",
    "partitions": [
        { "name": "AP", "base": "0x30013000", "size": "0x200000" },
        { "name": "CP", "base": "0x30213000", "size": "0x100000" },
        { "name": "RES", "base": "0x30313000", "size": "0x200000" },
        { "name": "OTA_TXZ", "base": "0x30513000", "size": "0x300000" }
    ]
}
```

则 BOOT 在升级时只做以下事情：

- 遍历所有有效分区
- 排除 `OTA_TXZ`
- 建立 `AP`、`CP`、`RES` 到各自物理地址与大小的映射
- 在 OTA 包中查找与这些分区名匹配的文件

### 9.2 文件名匹配规则

- 匹配基于分区名
- 不区分大小写
- 忽略可选的 `.bin` 后缀
- 不要求 APP 额外传入 manifest

示例：

- 分区表中有 `AP` 分区，则包内文件可命名为 `AP` 或 `ap.bin`
- 分区表中有 `CP` 分区，则包内文件可命名为 `CP` 或 `cp.bin`
- 分区表中有 `RES` 分区，则包内文件可命名为 `RES` 或 `res.bin`

### 9.3 OTA 包结构示例

未压缩包：

```text
ota.tar
  AP.bin
  CP.bin
  RES.bin
```

压缩包：

```text
ota.txz
  -> xz 解压后得到 ota.tar
```

包内也可以只包含分区子集；BOOT 仅处理能匹配到有效分区名的文件，其余文件跳过。

单镜像场景示例：

```text
partab:
  AP
  OTA_TXZ

ota.tar:
  AP.bin
```

这不是“静态写死 `AP.bin`”的特殊协议，而只是因为该场景下分区表里只有 `AP` 一个升级目标分区。

---

## 10. 包格式识别

### 10.1 现状

当前 `boot_ota_txz_update()` 只处理 `xz` 输入流。

### 10.2 目标

新增自动识别：

- 若包头是 `XZ` magic：走现有 `xz -> tar` 路径
- 若包头不是 `XZ`，但可识别为 tar：直接 tar 解析

### 10.3 建议规则

1. 读取首个 block
2. 若前 6 字节匹配 `FD 37 7A 58 5A 00`，判为 `txz`
3. 否则检查 tar header 特征：
   - block 大小 512
   - `ustar` 签名，或 header checksum 合法
4. 识别失败直接报错并保留 request

---

## 11. 构建与配置

### 11.1 新增 boot 配置项

建议新增：

- `CONFIG_BOOT_CONTROL_STORE_EASYFLASH`
  - 选择 EasyFlash 作为 request 默认后端
- `CONFIG_BOOT_CONTROL_STORE_TEST`
  - 选择内存 test backend，仅用于 Unity 测试构建
- `CONFIG_BOOT_OTA_SOURCE_TF`
  - 启用 TF 文件 reader
  - 依赖 `CONFIG_BOOT_SECOND_STAGE`

### 11.2 TF 功能依赖

若开启 `CONFIG_BOOT_OTA_SOURCE_TF`，boot 构建需引入：

- `CONFIG_DISK_DRIVER`
- `CONFIG_DISK_DRIVER_SDMMC`
- `CONFIG_FILE_SYSTEM`
- `CONFIG_FAT_FILESYSTEM_ELM` 或等价 FatFS 配置
- `CONFIG_LISA_DEVICE`
- `CONFIG_LISA_SDMMC_DEVICE`

### 11.3 代码复用来源

可直接参考但不整包复制：

- `/home/openclaw/Work/ARCS/boot/src/CMakeLists.txt`
- `/home/openclaw/Work/ARCS/boot/src/Kconfig`
- `/home/openclaw/Work/ARCS/boot/src/fs/disk/sdmmc.c`
- `/home/openclaw/Work/ARCS/boot/src/sdmmc_init.c`

---

## 12. 失败语义

根据当前需求，失败语义固定为：

- 失败后保留升级请求
- 失败后保留 `boot.mode=update`
- 下次重启继续尝试升级

这意味着：

- 参数无效
- TF 卡未插入
- TF 文件不存在
- `xz` 解压失败
- tar 内容不合法
- 分区写入失败

以上都属于“可重试失败”，不会自动回退到普通启动。

---

## 13. 与现有设计文档的关系

本文件是 [boot-design.md](/home/openclaw/Work/ARCS/arcs-sdk/system/uboot/doc/boot-design.md) 的增量补充，新增内容仅覆盖：

- APP 触发的升级请求协议
- `FLASH/TF` 动态源选择
- `tar/txz` 自动识别入口
- TF 文件 reader 的构建依赖

未替代原有以下设计：

- A/B 与 A/OTA 的分区布局
- 分区表结构
- TAR 文件名到分区名的固定映射语义
- A/B 启动与回滚逻辑

---

## 14. 后续实施建议

实施顺序建议为：

1. 先补 request 协议和 APP API
2. 先把 request 存储收敛成抽象后端，默认实现走 EasyFlash
3. 再接 boot 端状态机
4. 先打通 `FLASH` reader
5. 再接 `TF` reader 和 FS 依赖
6. 最后补板级验证和文档回填
