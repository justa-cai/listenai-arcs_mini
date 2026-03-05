# LSFS 文件系统操作示例

## 功能说明

演示 LSFS (LiSa File System) 文件系统的基本操作，包括挂载 SD 卡、文件读写、目录遍历等核心功能。

LSFS 是 ARCS SDK 提供的轻量级文件系统抽象层，支持多种文件系统类型（FatFS 等）和存储介质（SD 卡、Flash、RAM Disk）。本示例展示如何在 SD 卡上使用 FatFS 文件系统进行基本的文件操作。

## 硬件连接

- **SDMMC0 接口**：连接 SD 卡模块
  - 具体引脚定义参见板级配置文件

## 示例步骤

1. 初始化 SD 卡和磁盘子系统
2. 初始化 LSFS 框架
3. 挂载 SD 卡文件系统（挂载失败时自动格式化）
4. 写入测试文件（`/SD:/sdmmc.txt`）
5. 读取文件内容并验证
6. 遍历根目录，列出所有文件和目录
7. 卸载文件系统

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 预期输出

**终端输出：**
```
=== LSFS Sample ===

Mounted /SD: successfully

Write 12 bytes to /SD:/test.txt
Read 12 bytes: Hello, LSFS!

Listing directory: /SD:/
          12  test.txt

Unmounted /SD:
```

## 核心 API

| API | 说明 |
|-----|------|
| `lisa_sdmmc_probe()` | 初始化 SD 卡设备 |
| `disk_init()` | 初始化磁盘子系统 |
| `lsfs_init()` | 初始化 LSFS 框架 |
| `lsfs_mount()` | 挂载文件系统到指定挂载点 |
| `lsfs_mkfs()` | 格式化文件系统 |
| `lsfs_open()` | 打开或创建文件 |
| `lsfs_write()` | 写入数据到文件 |
| `lsfs_read()` | 从文件读取数据 |
| `lsfs_close()` | 关闭文件 |
| `lsfs_opendir()` | 打开目录 |
| `lsfs_readdir()` | 读取目录项 |
| `lsfs_closedir()` | 关闭目录 |
| `lsfs_unmount()` | 卸载文件系统 |

## 文件操作说明

### 打开模式标志

```c
LSFS_O_READ      // 只读模式
LSFS_O_WRITE     // 只写模式
LSFS_O_RDWR      // 读写模式
LSFS_O_CREATE    // 文件不存在则创建
LSFS_O_APPEND    // 追加模式
LSFS_O_TRUNC     // 打开时清空文件
```

### 错误处理

- **挂载失败处理**：示例中实现了自动格式化机制，当挂载失败时尝试格式化后重新挂载
- **返回值规则**：
  - 成功：返回 `0` 或正数（read/write 返回字节数）
  - 失败：返回负的 errno 值（如 `-EINVAL`, `-ENOENT`）

## 关键代码

### 挂载点定义

```c
#define SDMMC_DEVICE      "SD:"
#define SDMMC_MOUNT_POINT "/SD:"

static struct lsfs_mount_t sdmmc_lsfs_mnt = {
    .type = LSFS_FATFS,           // 使用 FatFS 文件系统
    .mnt_point = "/SD:",          // 挂载点路径
    .fs_data = NULL,              // 文件系统私有数据
};
```

### 挂载流程（含自动格式化）

```c
int ret = lsfs_mount(&sdmmc_lsfs_mnt);
if (ret != 0) {
    // 挂载失败，尝试格式化
    ret = lsfs_mkfs(LSFS_FATFS, "SD:", NULL, 0);
    if (ret == 0) {
        ret = lsfs_mount(&sdmmc_lsfs_mnt);  // 重新挂载
    }
}
```

### 文件读写操作

```c
struct lsfs_file_t file;

// 初始化文件对象
lsfs_file_t_init(&file);

// 写入文件
lsfs_open(&file, "/SD:/sdmmc.txt", LSFS_O_CREATE | LSFS_O_TRUNC | LSFS_O_WRITE);
lsfs_write(&file, data, strlen(data));
lsfs_close(&file);

// 读取文件
lsfs_file_t_init(&file);
lsfs_open(&file, "/SD:/sdmmc.txt", LSFS_O_READ);
lsfs_read(&file, buffer, sizeof(buffer) - 1);
lsfs_close(&file);
```

### 目录遍历

```c
struct lsfs_dir_t dir;
struct lsfs_dirent entry;

lsfs_dir_t_init(&dir);
lsfs_opendir(&dir, "/SD:/");

while (1) {
    ret = lsfs_readdir(&dir, &entry);
    if (ret != 0 || entry.name[0] == 0)
        break;  // 遍历结束

    if (entry.type == LSFS_DIR_ENTRY_DIR) {
        // 目录
    } else {
        // 文件
        printf("%10lu %s\n", entry.size, entry.name);
    }
}
lsfs_closedir(&dir);
```

## 配置说明

### 必需的配置项（prj.conf）

```
CONFIG_FILE_SYSTEM=y          # 使能文件系统支持
CONFIG_FATFS_FILESYSTEM=y     # 使能 FatFS 文件系统
CONFIG_DISK_DRIVER=y          # 使能磁盘驱动
CONFIG_DISK_DRIVER_SDMMC=y    # 使能 SD/MMC 磁盘驱动
CONFIG_LSFS=y                 # 使能 LSFS 抽象层
CONFIG_LSFS_FAT=y             # 使能 LSFS 的 FatFS 支持
```

## 注意事项

1. **初始化顺序**：必须按照以下顺序初始化
   - 先调用 `lisa_sdmmc_probe()` 初始化 SD 卡设备
   - 再调用 `disk_init()` 初始化磁盘子系统
   - 然后调用 `lsfs_init()` 初始化 LSFS 框架
   - 最后调用 `lsfs_mount()` 挂载文件系统

2. **文件对象初始化**：每次使用前必须调用 `lsfs_file_t_init()` 或 `lsfs_dir_t_init()` 初始化对象

3. **挂载点命名规则**：
   - 必须以 `/` 开头
   - 通常以 `:` 结尾（如 `/SD:`）
   - 完整文件路径格式：`/SD:/dir/file.txt`

4. **及时关闭文件**：使用完文件或目录后立即调用 `lsfs_close()` 或 `lsfs_closedir()` 释放资源

5. **线程安全**：LSFS API 是线程安全的，但同一个文件对象不应在多线程间共享

## 扩展功能

LSFS 支持更多高级功能，详见组件文档 `modules/fs/README.md`：

- **多存储介质**：同时挂载 RAM Disk、Flash Disk、SD Card
- **POSIX 兼容接口**：通过 LVFS 层使用标准 POSIX API（`open`, `read`, `write` 等）
- **目录操作**：创建目录（`lsfs_mkdir`）、删除（`lsfs_unlink`）、重命名（`lsfs_rename`）
- **文件定位**：`lsfs_seek`, `lsfs_tell`, `lsfs_truncate`
- **文件系统信息**：查询文件信息（`lsfs_stat`）和磁盘空间（`lsfs_statvfs`）
- **系统掉电保护**：LSFS 冻结功能（`lsfs_freeze_freeze`）
