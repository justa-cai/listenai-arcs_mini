# 文件系统

## 概述

该组件是ARCS SDK提供的文件系统框架，支持多种文件系统类型和存储介质。该组件采用分层架构设计，提供了从底层磁盘访问到高层POSIX兼容接口的完整解决方案。

## 架构设计

FS组件采用分层架构设计，数据流向：应用层 → POSIX API/LSFS API → LVFS → LSFS → SubFS → Disk

```
┌──────────────────────────────────────────────────────────┐
│            应用层 (Application Layer)                     │
│  支持两种调用方式：                                        │
│  1. POSIX API (open/read/write/close)                    │
│  2. LSFS API (lsfs_open/lsfs_read/lsfs_write)            │
└──────────────────────────────────────────────────────────┘
                 ↓                           ↓
         ┌───────────────┐          ┌───────────────┐
         │  POSIX API    │          │   直接调用     │
         └───────────────┘          └───────────────┘
                 ↓                           ↓
┌──────────────────────────────────┐        │
│  LVFS (LiSa Virtual File System) │        │
│  - 提供POSIX接口实现              │        │
│  - 文件描述符管理                 │        │
│  - 内部调用LSFS接口               │        │
└──────────────────────────────────┘        │
                 ↓                           ↓
┌──────────────────────────────────────────────────────────┐
│          LSFS (LiSa File System)                         │
│  - 文件系统抽象层，提供统一接口                           │
│  - 多挂载点管理                                           │
│  - 静态注册SubFS中的文件系统实例                          │
│  - 冻结/恢复功能（用于系统掉电保护）                      │
└──────────────────────────────────────────────────────────┘
                 ↓
┌──────────────────────────────────────────────────────────┐
│          SubFS (子文件系统实现层)                         │
│  ┌──────────────────────────────────────────────┐        │
│  │   FatFS - FAT文件系统实现                     │        │
│  │   (通过lsfs_register注册到LSFS)              │        │
│  └──────────────────────────────────────────────┘        │
└──────────────────────────────────────────────────────────┘
                 ↓
┌──────────────────────────────────────────────────────────┐
│         Disk 驱动层 (Disk Driver Layer)                  │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐               │
│  │ RAM Disk │  │Flash Disk│  │ SD/MMC   │               │
│  └──────────┘  └──────────┘  └──────────┘               │
└──────────────────────────────────────────────────────────┘
```

### 组件说明

#### 1. 应用层调用方式
应用层支持两种文件系统操作方式：

**方式一：使用POSIX API**
```c
int fd = open("/RAM:/test.txt", O_CREAT | O_RDWR, 0666);
write(fd, data, size);
read(fd, buffer, size);
close(fd);
```
- POSIX API调用会经过LVFS层，LVFS内部转发给LSFS处理
- 适合需要标准POSIX兼容的应用

**方式二：直接使用LSFS API**
```c
struct lsfs_file_t file;
lsfs_file_t_init(&file);
lsfs_open(&file, "/RAM:/test.txt", LSFS_O_CREATE | LSFS_O_RDWR);
lsfs_write(&file, data, size);
lsfs_read(&file, buffer, size);
lsfs_close(&file);
```
- 直接调用LSFS接口，不经过LVFS层
- 更轻量级，适合资源受限的嵌入式应用

#### 2. LSFS (LiSa File System)
LSFS是轻量级文件系统抽象层，提供统一的文件系统接口。SubFS中的文件系统实例（如FatFS）在初始化时通过`lsfs_register()`静态注册到LSFS。

**特性：**
- 支持多文件系统类型（FatFS等）
- 多挂载点管理
- 统一的错误处理
- 文件系统格式化支持
- 冻结/恢复功能（用于系统掉电保护）
- SubFS静态注册机制

**主要API：**
- 文件操作：`lsfs_open()`, `lsfs_close()`, `lsfs_read()`, `lsfs_write()`
- 目录操作：`lsfs_mkdir()`, `lsfs_opendir()`, `lsfs_readdir()`
- 文件管理：`lsfs_unlink()`, `lsfs_rename()`, `lsfs_stat()`
- 挂载管理：`lsfs_mount()`, `lsfs_unmount()`, `lsfs_mkfs()`
- 文件系统注册：`lsfs_register()`, `lsfs_unregister()`

#### 3. LVFS (LiSa Virtual File System)
LVFS提供POSIX兼容接口，内部调用LSFS接口实现功能。应用层通过POSIX API（如`open()`, `read()`等）操作文件时，实际由LVFS转发给LSFS处理。

**特性：**
- 提供POSIX API实现
- 文件描述符管理
- 内部转发调用到LSFS
- 支持大文件（64位偏移）

**主要API：**
- `lvfs_open()`, `lvfs_close()`, `lvfs_read()`, `lvfs_write()`
- `lvfs_lseek()`, `lvfs_truncate()`, `lvfs_sync()`
- `lvfs_register()` - 注册文件系统到LVFS层

#### 4. SubFS (子文件系统)
SubFS包含具体的文件系统实现，通过`lsfs_register()`注册到LSFS。

**支持的文件系统：**
- **FatFS** - FAT文件系统实现，在`lsfs_fat_init()`中注册

**注册机制：**
```c
// FatFS初始化时静态注册到LSFS
int lsfs_fat_init(void) {
    return lsfs_register(LSFS_FATFS, &fatfs_fs);
}
```

#### 5. Disk 驱动层
提供对各种存储介质的抽象访问，被SubFS中的文件系统实现所依赖。

**支持的存储类型：**
- **RAM Disk** - 内存模拟磁盘
- **Flash Disk** - Flash存储
- **SD/MMC** - SD卡存储

**主要API：**
- `disk_init()` - 初始化磁盘子系统
- `disk_access_init()` - 初始化特定磁盘
- `disk_access_read()` - 读取扇区
- `disk_access_write()` - 写入扇区
- `disk_access_ioctl()` - 磁盘控制操作

## 配置选项

在 `menuconfig` 中配置文件系统：

```
File Systems --->
    [*] File system support
        [*] Support __LARGE64_FILES
        SubFS --->
            [*] FatFS file system support
        LSFS --->
            [*] Enable LSFS
            [*] Support freeze functionality
        LVFS --->
            [*] Enable LVFS
            [*] Enable POSIX API alias
    Disk Driver --->
        [*] RAM Disk support
        [*] Flash Disk support
        [*] SD/MMC support
```

## 使用指南

### 1. 使用LSFS接口

#### 基本流程

```c
#include "lsfs.h"
#include "disk/disk_access.h"

// 1. 初始化磁盘和文件系统
disk_init(NULL);
lsfs_init();

// 2. 定义挂载点
static struct lsfs_mount_t ram_mnt = {
    .type = LSFS_FATFS,
    .mnt_point = "/RAM:",
    .fs_data = NULL,
};

// 3. 挂载文件系统
int ret = lsfs_mount(&ram_mnt);
if (ret != 0) {
    // 挂载失败，尝试格式化
    ret = lsfs_mkfs(LSFS_FATFS, "RAM:", NULL, 0);
    if (ret == 0) {
        ret = lsfs_mount(&ram_mnt);
    }
}

// 4. 文件操作
struct lsfs_file_t file;
lsfs_file_t_init(&file);

// 写入文件
ret = lsfs_open(&file, "/RAM:/test.txt", LSFS_O_CREATE | LSFS_O_WRITE);
if (ret == 0) {
    const char *data = "Hello, LSFS!";
    lsfs_write(&file, data, strlen(data));
    lsfs_close(&file);
}

// 读取文件
lsfs_file_t_init(&file);
ret = lsfs_open(&file, "/RAM:/test.txt", LSFS_O_READ);
if (ret == 0) {
    char buffer[64];
    ssize_t bytes = lsfs_read(&file, buffer, sizeof(buffer) - 1);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        printf("Read: %s\n", buffer);
    }
    lsfs_close(&file);
}

// 5. 卸载文件系统
lsfs_unmount(&ram_mnt);
```

#### 打开模式标志

```c
// 访问模式
LSFS_O_READ      // 只读模式
LSFS_O_WRITE     // 只写模式
LSFS_O_RDWR      // 读写模式

// 创建标志
LSFS_O_CREATE    // 文件不存在则创建
LSFS_O_APPEND    // 追加模式
LSFS_O_TRUNC     // 打开时清空文件
```

#### 目录操作

```c
#include "lsfs.h"

void list_directory(const char *path) {
    struct lsfs_dir_t dir;
    struct lsfs_dirent entry;
    int ret;

    // 初始化目录对象
    lsfs_dir_t_init(&dir);

    // 打开目录
    ret = lsfs_opendir(&dir, path);
    if (ret == 0) {
        // 遍历目录
        while (1) {
            ret = lsfs_readdir(&dir, &entry);
            if (ret != 0 || entry.name[0] == 0)
                break;

            if (entry.type == LSFS_DIR_ENTRY_DIR) {
                printf("   <DIR>   %s\n", entry.name);
            } else {
                printf("%10lu %s\n", entry.size, entry.name);
            }
        }
        lsfs_closedir(&dir);
    }
}

// 创建目录
ret = lsfs_mkdir("/RAM:/mydir");

// 删除文件或目录
ret = lsfs_unlink("/RAM:/test.txt");

// 重命名
ret = lsfs_rename("/RAM:/old.txt", "/RAM:/new.txt");
```

#### 文件定位操作

```c
struct lsfs_file_t file;
lsfs_file_t_init(&file);

lsfs_open(&file, "/RAM:/data.bin", LSFS_O_RDWR);

// 移动文件指针
lsfs_seek(&file, 100, LSFS_SEEK_SET);  // 从文件开头偏移100字节
lsfs_seek(&file, 10, LSFS_SEEK_CUR);   // 从当前位置前进10字节
lsfs_seek(&file, -20, LSFS_SEEK_END);  // 从文件末尾倒退20字节

// 获取当前位置
off_t pos = lsfs_tell(&file);

// 获取文件大小
off_t size = lsfs_lsize(&file);

// 截断文件
lsfs_truncate(&file, 1024);  // 截断或扩展到1024字节

// 同步到磁盘
lsfs_sync(&file);

lsfs_close(&file);
```

#### 快速查找优化

对于大文件的频繁seek操作，可以使用快速查找功能：

```c
struct lsfs_file_t file;
lsfs_file_t_init(&file);

lsfs_open(&file, "/RAM:/bigfile.bin", LSFS_O_READ);

// 建立簇映射表（提升后续seek性能）
lsfs_fastseek_link(&file, 1024);  // 分配1024字节映射表

// 执行快速seek操作
lsfs_seek(&file, 1000000, LSFS_SEEK_SET);

// 重建映射表（文件内容变化后）
lsfs_fastseek_relinkmap(&file);

// 释放映射表
lsfs_fastseek_unlink(&file);

lsfs_close(&file);
```

### 2. 使用POSIX兼容接口

通过LVFS层，可以使用标准POSIX API：

```c
#include <fcntl.h>
#include <unistd.h>
#include "lvfs.h"
#include "lsfs.h"
#include "disk/disk_access.h"

// 1. 初始化
disk_init(NULL);
lvfs_init();
lsfs_init();

// 2. 挂载文件系统
static struct lsfs_mount_t ram_mnt = {
    .type = LSFS_FATFS,
    .mnt_point = "/RAM:",
    .fs_data = NULL,
};
lsfs_mount(&ram_mnt);

// 3. 使用POSIX API
int fd = open("/RAM:/test.txt", O_CREAT | O_RDWR, 0666);
if (fd >= 0) {
    const char *data = "Hello, POSIX!";
    write(fd, data, strlen(data));

    lseek(fd, 0, SEEK_SET);

    char buffer[64];
    ssize_t bytes = read(fd, buffer, sizeof(buffer) - 1);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        printf("Read: %s\n", buffer);
    }

    // 截断文件
    ftruncate(fd, 100);

    // 同步到磁盘
    fsync(fd);

    close(fd);
}
```

### 3. 工作目录管理

```c
#include "lsfs.h"

// 改变当前工作目录
lsfs_chdir("/RAM:/mydir");

// 获取当前工作目录
char cwd[256];
lsfs_getcwd("/RAM:", cwd, sizeof(cwd));
printf("Current dir: %s\n", cwd);

// 改变当前驱动器
lsfs_chdrive("/RAM:");
```

### 4. 文件系统信息查询

```c
#include "lsfs.h"

// 获取文件或目录信息
struct lsfs_dirent entry;
int ret = lsfs_stat("/RAM:/test.txt", &entry);
if (ret == 0) {
    printf("Name: %s\n", entry.name);
    printf("Size: %lu bytes\n", entry.size);
    printf("Type: %s\n",
           entry.type == LSFS_DIR_ENTRY_FILE ? "File" : "Directory");
}

// 获取文件系统容量信息
struct lsfs_statvfs stat;
ret = lsfs_statvfs("/RAM:", &stat);
if (ret == 0) {
    printf("Block size: %lu\n", stat.f_bsize);
    printf("Total blocks: %lu\n", stat.f_blocks);
    printf("Free blocks: %lu\n", stat.f_bfree);
    printf("Total size: %lu bytes\n",
           stat.f_blocks * stat.f_frsize);
    printf("Free size: %lu bytes\n",
           stat.f_bfree * stat.f_frsize);
}
```

### 5. 多存储介质支持

```c
#include "lsfs.h"
#include "disk/disk_access.h"
#include "lisa_sdmmc.h"

// 初始化
disk_init(NULL);
lsfs_init();

// RAM Disk
static struct lsfs_mount_t ram_mnt = {
    .type = LSFS_FATFS,
    .mnt_point = "/RAM:",
};

// Flash Disk
static struct lsfs_mount_t flash_mnt = {
    .type = LSFS_FATFS,
    .mnt_point = "/NAND:",
};

// SD Card
static struct lsfs_mount_t sd_mnt = {
    .type = LSFS_FATFS,
    .mnt_point = "/SD:",
};

// 初始化SD卡
lisa_sdmmc_probe(lisa_device_get("sdmmc0"));

// 挂载各个存储设备
lsfs_mount(&ram_mnt);
lsfs_mount(&flash_mnt);
lsfs_mount(&sd_mnt);

// 使用不同存储
struct lsfs_file_t file;

// 在RAM上操作
lsfs_file_t_init(&file);
lsfs_open(&file, "/RAM:/temp.txt", LSFS_O_CREATE | LSFS_O_WRITE);
lsfs_write(&file, "RAM data", 8);
lsfs_close(&file);

// 在Flash上操作
lsfs_file_t_init(&file);
lsfs_open(&file, "/NAND:/config.txt", LSFS_O_CREATE | LSFS_O_WRITE);
lsfs_write(&file, "Flash data", 10);
lsfs_close(&file);

// 在SD卡上操作
lsfs_file_t_init(&file);
lsfs_open(&file, "/SD:/media.bin", LSFS_O_CREATE | LSFS_O_WRITE);
lsfs_write(&file, "SD data", 7);
lsfs_close(&file);
```

### 6. 系统掉电保护

LSFS提供冻结功能，在系统即将掉电时保护文件系统：

```c
#if CONFIG_LSFS_SUPPORT_FREEZE

// 系统掉电前调用
// 会自动关闭所有打开的文件，不修改文件系统结构
int ret = lsfs_freeze_freeze(0);
if (ret < 0) {
    // 某些文件关闭失败
    printf("Warning: Some files failed to close\n");
}

#endif
```

## 错误处理

所有API返回值遵循以下规则：

- **成功**：返回 `0` 或正数（对于read/write返回字节数）
- **失败**：返回负的errno值

常见错误码：

```c
-EINVAL   // 无效参数
-ENOENT   // 文件或目录不存在
-EEXIST   // 文件或目录已存在
-EROFS    // 只读文件系统
-ENOSPC   // 磁盘空间不足
-EBADF    // 无效的文件描述符
-EBUSY    // 资源忙
-ENOTSUP  // 操作不支持
```

错误处理示例：

```c
struct lsfs_file_t file;
lsfs_file_t_init(&file);

int ret = lsfs_open(&file, "/RAM:/test.txt", LSFS_O_READ);
if (ret != 0) {
    switch (ret) {
    case -ENOENT:
        printf("File not found\n");
        break;
    case -EINVAL:
        printf("Invalid path\n");
        break;
    default:
        printf("Open failed: %d\n", ret);
        break;
    }
    return ret;
}
```

## 性能优化建议

### 1. 文件操作优化

- **批量读写**：使用较大的缓冲区减少系统调用次数
  ```c
  // 推荐：一次读取大块数据
  char buffer[4096];
  lsfs_read(&file, buffer, sizeof(buffer));

  // 不推荐：频繁小块读取
  char byte;
  for (int i = 0; i < 4096; i++) {
      lsfs_read(&file, &byte, 1);
  }
  ```

- **顺序访问**：避免频繁的随机seek，尽量顺序读写

- **及时关闭**：使用完文件后立即关闭，释放资源

### 2. 大文件处理

- 使用64位文件接口处理大于2GB的文件：
  ```c
  #if CONFIG_FS_LARGE64_FILES
  _off64_t offset = lsfs_lseek64(&file, large_offset, LSFS_SEEK_SET);
  int64_t size = lsfs_lsize64(&file);
  lsfs_truncate64(&file, new_size);
  #endif
  ```

- 对大文件使用快速查找功能减少seek时间

### 3. 磁盘缓存

某些磁盘驱动支持IO缓存，可提升性能（见 `iocache.c`）

### 4. 文件系统选择

- **RAM Disk**：速度最快，但掉电丢失，适合临时数据
- **Flash Disk**：持久化存储，适合配置文件
- **SD Card**：大容量，适合媒体文件

## 示例程序

完整示例代码位于：

- **LSFS示例**：`samples/modules/fs/lsfs/`
- **POSIX示例**：`samples/modules/fs/posix/fs/`
- **FatFS示例**：`samples/modules/fs/fatfs/`

## API参考

### LSFS核心API

| 函数 | 说明 |
|------|------|
| `lsfs_init()` | 初始化LSFS框架 |
| `lsfs_mount()` | 挂载文件系统 |
| `lsfs_unmount()` | 卸载文件系统 |
| `lsfs_mkfs()` | 格式化文件系统 |
| `lsfs_open()` | 打开或创建文件 |
| `lsfs_close()` | 关闭文件 |
| `lsfs_read()` | 读取文件 |
| `lsfs_write()` | 写入文件 |
| `lsfs_seek()` | 设置文件位置 |
| `lsfs_tell()` | 获取文件位置 |
| `lsfs_lsize()` | 获取文件大小 |
| `lsfs_truncate()` | 截断文件 |
| `lsfs_sync()` | 同步文件到磁盘 |
| `lsfs_unlink()` | 删除文件或目录 |
| `lsfs_rename()` | 重命名文件或目录 |
| `lsfs_mkdir()` | 创建目录 |
| `lsfs_opendir()` | 打开目录 |
| `lsfs_readdir()` | 读取目录项 |
| `lsfs_closedir()` | 关闭目录 |
| `lsfs_stat()` | 获取文件信息 |
| `lsfs_statvfs()` | 获取文件系统信息 |
| `lsfs_getcwd()` | 获取当前目录 |
| `lsfs_chdir()` | 改变当前目录 |
| `lsfs_chdrive()` | 改变当前驱动器 |

### LVFS核心API

| 函数 | 说明 |
|------|------|
| `lvfs_init()` | 初始化LVFS框架 |
| `lvfs_register()` | 注册文件系统 |
| `lvfs_unregister()` | 取消注册文件系统 |
| `lvfs_open()` | 打开文件（返回fd） |
| `lvfs_close()` | 关闭文件 |
| `lvfs_read()` | 读取文件 |
| `lvfs_write()` | 写入文件 |
| `lvfs_lseek()` | 设置文件位置 |
| `lvfs_truncate()` | 截断文件 |
| `lvfs_sync()` | 同步文件 |

### Disk Access API

| 函数 | 说明 |
|------|------|
| `disk_init()` | 初始化磁盘子系统 |
| `disk_access_init()` | 初始化指定磁盘 |
| `disk_access_status()` | 获取磁盘状态 |
| `disk_access_read()` | 读取磁盘扇区 |
| `disk_access_write()` | 写入磁盘扇区 |
| `disk_access_ioctl()` | 磁盘控制操作 |
| `disk_access_register()` | 注册磁盘驱动 |
| `disk_access_unregister()` | 取消注册磁盘驱动 |

## 注意事项

1. **初始化顺序**
   - 先调用 `disk_init()`
   - 再调用 `lsfs_init()` 或 `lvfs_init()`
   - 最后调用 `lsfs_mount()`

2. **文件对象初始化**
   - 每次使用前必须调用 `lsfs_file_t_init()` 或 `lsfs_dir_t_init()`
   - 关闭后可以重用，但需重新初始化

3. **挂载点命名**
   - 必须以 `/` 开头
   - 通常以 `:` 结尾（如 `/RAM:`）
   - 文件路径格式：`/RAM:/dir/file.txt`

4. **只读文件系统**
   - 挂载时可设置 `LSFS_MOUNT_FLAG_READ_ONLY`
   - 只读模式下写操作返回 `-EROFS`

5. **线程安全**
   - LSFS/LVFS API 是线程安全的
   - 但同一个文件对象不应在多线程间共享

6. **资源限制**
   - 最大打开文件数由配置决定
   - 文件名长度限制：`MAX_FILE_NAME` (默认256)

## 常见问题

### Q1: 挂载失败怎么办？

**A:** 挂载失败通常是因为文件系统未格式化，解决方法：

```c
int ret = lsfs_mount(&mnt);
if (ret != 0) {
    // 尝试格式化
    ret = lsfs_mkfs(mnt.type, &mnt.mnt_point[1], NULL, 0);
    if (ret == 0) {
        ret = lsfs_mount(&mnt);  // 重新挂载
    }
}
```

### Q2: 如何在不同存储间复制文件？

**A:** 打开源文件和目标文件，循环读写：

```c
struct lsfs_file_t src, dst;
char buffer[512];
ssize_t bytes;

lsfs_file_t_init(&src);
lsfs_file_t_init(&dst);

lsfs_open(&src, "/RAM:/source.txt", LSFS_O_READ);
lsfs_open(&dst, "/SD:/dest.txt", LSFS_O_CREATE | LSFS_O_WRITE);

while ((bytes = lsfs_read(&src, buffer, sizeof(buffer))) > 0) {
    lsfs_write(&dst, buffer, bytes);
}

lsfs_close(&src);
lsfs_close(&dst);
```

### Q3: 如何判断路径是文件还是目录？

**A:** 使用 `lsfs_stat()` 查询：

```c
struct lsfs_dirent entry;
if (lsfs_stat(path, &entry) == 0) {
    if (entry.type == LSFS_DIR_ENTRY_DIR) {
        printf("It's a directory\n");
    } else {
        printf("It's a file\n");
    }
}
```

### Q4: 如何检查磁盘剩余空间？

**A:** 使用 `lsfs_statvfs()`：

```c
struct lsfs_statvfs stat;
if (lsfs_statvfs("/RAM:", &stat) == 0) {
    unsigned long free_bytes = stat.f_bfree * stat.f_frsize;
    printf("Free space: %lu bytes\n", free_bytes);
}
```

### Q5: 为什么POSIX API无法使用？

**A:** 确保：
1. 在 `menuconfig` 中启用 `CONFIG_LVFS_POSIX_API_ALIAS`
2. 调用了 `lvfs_init()`
3. 文件系统已成功挂载
