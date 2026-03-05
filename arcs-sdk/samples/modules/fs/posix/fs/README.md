# LVFS POSIX 文件操作示例

## 功能说明

演示通过 LVFS (LiSa Virtual File System) 层使用标准 POSIX API 进行文件操作，包括文件描述符操作和标准 I/O 流操作两种方式。

LVFS 提供 POSIX 兼容接口，应用层通过标准的 `open()`, `read()`, `write()`, `fopen()`, `fread()`, `fwrite()` 等 API 操作文件，LVFS 内部转发调用到 LSFS 处理。本示例在 SD 卡上演示两种 POSIX 接口的使用。

### 新特性

- **标准 POSIX 兼容**：使用标准 C 库文件操作 API，便于移植现有应用
- **双接口支持**：支持文件描述符接口（`open`/`read`/`write`）和流接口（`fopen`/`fread`/`fwrite`）
- **透明调用**：应用层无需关心底层实现，LVFS 自动转发到 LSFS
- **完整功能集**：支持文件定位（`lseek`/`fseek`）、截断（`ftruncate`）、同步（`fsync`/`fflush`）

## 硬件连接

- **SDMMC0 接口**：连接 SD 卡模块
  - 具体引脚定义参见板级配置文件

## 使用场景

适用于需要标准 POSIX 兼容的应用场景，特别是从其他操作系统（如 Linux）移植应用程序时，可以直接使用熟悉的文件操作 API，降低移植成本。

## 示例步骤

1. 初始化 SD 卡和磁盘子系统
2. 初始化 LVFS 和 LSFS 框架
3. 挂载 SD 卡文件系统（挂载失败时自动格式化）
4. **测试文件描述符接口**：
   - 使用 `open()` 创建文件
   - 使用 `write()` 写入数据
   - 使用 `lseek()` 定位文件指针
   - 使用 `read()` 读取数据并验证
   - 使用 `fsync()` 同步到磁盘
   - 使用 `ftruncate()` 截断文件
   - 使用 `close()` 关闭文件
5. **测试标准 I/O 流接口**（如果启用 `CONFIG_LVFS_POSIX_API_ALIAS`）：
   - 使用 `fopen()` 创建文件流
   - 使用 `fwrite()` 写入数据
   - 使用 `fseek()` 定位文件指针
   - 使用 `fread()` 读取数据并验证
   - 使用 `fflush()` 刷新缓冲区
   - 使用 `fclose()` 关闭文件流
6. 卸载文件系统

## 编译

```{eval-rst}
.. include:: /sample_build.rst
```

## 预期输出

**终端输出：**
```
=== LVFS POSIX Sample ===

Mounted /SD: successfully

--- File Descriptor API Demo ---
write() wrote 13 bytes
read() read 13 bytes: Hello, POSIX!
fsync() completed
ftruncate() to 5 bytes
close() completed

--- Standard I/O Stream API Demo ---
fwrite() wrote 13 items
fflush() completed
fread() read 13 items: Hello, POSIX!
fclose() completed

Unmounted /SD:
```

## 核心 API

### 文件系统初始化 API

| API | 说明 |
|-----|------|
| `lisa_sdmmc_probe()` | 初始化 SD 卡设备 |
| `disk_init()` | 初始化磁盘子系统 |
| `lvfs_init()` | 初始化 LVFS 框架 |
| `lsfs_init()` | 初始化 LSFS 框架 |
| `lsfs_mount()` | 挂载文件系统 |
| `lsfs_unmount()` | 卸载文件系统 |

### POSIX 文件描述符接口

| API | 说明 |
|-----|------|
| `open()` | 打开或创建文件（返回文件描述符） |
| `read()` | 从文件描述符读取数据 |
| `write()` | 向文件描述符写入数据 |
| `lseek()` | 设置文件读写位置 |
| `fsync()` | 同步文件数据到磁盘 |
| `ftruncate()` | 截断文件到指定长度 |
| `close()` | 关闭文件描述符 |

### POSIX 标准 I/O 流接口（需启用 `CONFIG_LVFS_POSIX_API_ALIAS`）

| API | 说明 |
|-----|------|
| `fopen()` | 打开文件流（返回 FILE 指针） |
| `fread()` | 从文件流读取数据 |
| `fwrite()` | 向文件流写入数据 |
| `fseek()` | 设置文件流读写位置 |
| `fflush()` | 刷新文件流缓冲区 |
| `fclose()` | 关闭文件流 |

## POSIX 接口说明

### 文件描述符接口

文件描述符接口是底层的 POSIX 文件操作方式，直接使用整数文件描述符：

**特点：**
- 返回整数文件描述符（`int fd`）
- 适合系统级编程
- 性能略高于流接口
- 使用 `open()`, `read()`, `write()`, `close()`

**典型使用流程：**
```c
int fd = open("/SD:/test.txt", O_CREAT | O_RDWR, 0666);
write(fd, data, size);
lseek(fd, 0, SEEK_SET);
read(fd, buffer, size);
fsync(fd);
close(fd);
```

### 标准 I/O 流接口

标准 I/O 流接口是高层的 C 标准库接口，使用 FILE 指针：

**特点：**
- 返回 FILE 指针（`FILE *fp`）
- 符合 C 标准库规范
- 提供缓冲机制
- 适合应用级编程
- 使用 `fopen()`, `fread()`, `fwrite()`, `fclose()`

**典型使用流程：**
```c
FILE *fp = fopen("/SD:/test.txt", "w+");
fwrite(data, 1, size, fp);
fseek(fp, 0, SEEK_SET);
fread(buffer, 1, size, fp);
fflush(fp);
fclose(fp);
```

## 关键代码

### 挂载点定义

```c
#define SDMMC_DEVICE      "SD:"
#define SDMMC_MOUNT_POINT "/SD:"

static struct lsfs_mount_t ram_lsfs_mnt = {
    .type = LSFS_FATFS,           // 使用 FatFS 文件系统
    .mnt_point = "/SD:",          // 挂载点路径
    .fs_data = NULL,              // 文件系统私有数据
};
```

### 初始化流程

```c
// 1. 初始化 SD 卡设备
lisa_sdmmc_probe(lisa_device_get("sdmmc0"));

// 2. 初始化磁盘子系统
disk_init(NULL);

// 3. 初始化 LVFS 和 LSFS
lvfs_init();  // 必须先初始化 LVFS
lsfs_init();

// 4. 挂载文件系统
lsfs_mount(&ram_lsfs_mnt);
```

### 文件描述符操作示例

```c
#include <fcntl.h>
#include <unistd.h>

// 打开文件
int fd = open("/SD:/sdmmc.txt", O_CREAT | O_RDWR, 0660);

// 写入数据
write(fd, "hello world!", 12);

// 定位到文件开头
lseek(fd, 0, SEEK_SET);

// 读取数据
char buffer[80];
ssize_t bytes = read(fd, buffer, sizeof(buffer));

// 同步到磁盘
fsync(fd);

// 截断文件
ftruncate(fd, 10);

// 关闭文件
close(fd);
```

### 标准 I/O 流操作示例

```c
#include <stdio.h>

// 打开文件流
FILE *fp = fopen("/SD:/sdmmc.txt", "w+");

// 写入数据
fwrite("hello world!", 1, 12, fp);

// 刷新缓冲区
fflush(fp);

// 定位到文件开头
fseek(fp, 0, SEEK_SET);

// 读取数据
char buffer[80];
size_t bytes = fread(buffer, 1, sizeof(buffer), fp);

// 关闭文件流
fclose(fp);
```

## 配置说明

### 必需的配置项（prj.conf）

```
CONFIG_FILE_SYSTEM=y               # 使能文件系统支持
CONFIG_FATFS_FILESYSTEM=y          # 使能 FatFS 文件系统
CONFIG_DISK_DRIVER=y               # 使能磁盘驱动
CONFIG_DISK_DRIVER_SDMMC=y         # 使能 SD/MMC 磁盘驱动

CONFIG_LSFS=y                      # 使能 LSFS 抽象层
CONFIG_LSFS_FAT=y                  # 使能 LSFS 的 FatFS 支持
CONFIG_LSFS_REGISTER_VFS=y         # 注册 LSFS 到 LVFS

CONFIG_LVFS=y                      # 使能 LVFS 虚拟文件系统
CONFIG_LVFS_POSIX_API=y            # 使能 POSIX API 支持
CONFIG_LVFS_POSIX_API_ALIAS=y      # 使能 POSIX API 别名（标准 I/O 流）
```

### 配置选项说明

- **`CONFIG_LVFS_POSIX_API`**：使能 POSIX 文件描述符接口（`open`, `read`, `write` 等）
- **`CONFIG_LVFS_POSIX_API_ALIAS`**：使能标准 I/O 流接口（`fopen`, `fread`, `fwrite` 等）
- **`CONFIG_LSFS_REGISTER_VFS`**：自动注册 LSFS 到 LVFS，允许 POSIX API 访问 LSFS 文件系统

## 打开模式标志

### open() 标志位

```c
O_RDONLY    // 只读模式
O_WRONLY    // 只写模式
O_RDWR      // 读写模式
O_CREAT     // 文件不存在则创建
O_APPEND    // 追加模式
O_TRUNC     // 打开时清空文件
```

### fopen() 模式字符串

```c
"r"         // 只读，文件必须存在
"w"         // 只写，文件不存在则创建，存在则清空
"a"         // 追加，文件不存在则创建
"r+"        // 读写，文件必须存在
"w+"        // 读写，文件不存在则创建，存在则清空
"a+"        // 读写追加，文件不存在则创建
```

## 注意事项

1. **初始化顺序**：必须按照以下顺序初始化
   - 先调用 `lisa_sdmmc_probe()` 初始化 SD 卡设备
   - 再调用 `disk_init()` 初始化磁盘子系统
   - 然后调用 `lvfs_init()` 初始化 LVFS（必须在 `lsfs_init()` 之前）
   - 接着调用 `lsfs_init()` 初始化 LSFS
   - 最后调用 `lsfs_mount()` 挂载文件系统

2. **错误处理**：POSIX API 遵循标准错误处理机制
   - 成功时返回非负值（文件描述符、字节数等）
   - 失败时返回 `-1` 并设置 `errno`
   - 使用 `errno` 获取具体错误码

3. **文件描述符管理**：
   - `open()` 返回的文件描述符必须使用 `close()` 关闭
   - 不要混用文件描述符和 FILE 指针

4. **流缓冲**：
   - `fwrite()` 写入的数据可能在缓冲区中
   - 使用 `fflush()` 或 `fclose()` 确保数据写入磁盘
   - 文件描述符接口（`write()`）通常直接写入，但仍建议使用 `fsync()` 同步

5. **路径格式**：
   - 文件路径必须包含挂载点（如 `/SD:/test.txt`）
   - 挂载点格式：`/设备名:`（如 `/SD:`, `/RAM:`）

6. **配置依赖**：
   - 标准 I/O 流接口需要启用 `CONFIG_LVFS_POSIX_API_ALIAS`
   - 如果未启用，只能使用文件描述符接口

## 与 LSFS 接口的对比

| 特性 | POSIX 接口（LVFS） | LSFS 接口 |
|------|-------------------|-----------|
| 接口风格 | 标准 POSIX API | LSFS 专有 API |
| 移植性 | 符合 POSIX 标准，易于移植 | ARCS SDK 特有 |
| 文件操作 | `open`, `read`, `write` | `lsfs_open`, `lsfs_read`, `lsfs_write` |
| 文件对象 | 文件描述符（`int`）或 FILE 指针 | `struct lsfs_file_t` |
| 资源占用 | 略高（多一层 LVFS） | 更轻量级 |
| 适用场景 | 应用级开发、移植现有代码 | 资源受限的嵌入式系统 |

**选择建议：**
- 如果移植现有应用或需要标准兼容性，使用 **POSIX 接口**（本示例）
- 如果追求最小资源占用和直接控制，使用 **LSFS 接口**（参见 `samples/modules/fs/lsfs/`）

## 扩展功能

LVFS 和 LSFS 支持更多高级功能，详见组件文档 `modules/fs/README.md`：

- **多存储介质**：同时挂载 SD Card、Flash Disk、RAM Disk
- **目录操作**：创建目录、删除文件、重命名（POSIX: `mkdir`, `unlink`, `rename`）
- **文件信息查询**：`stat()`, `fstat()` 获取文件属性
- **大文件支持**：64位文件接口处理大于2GB的文件
- **工作目录管理**：`chdir()`, `getcwd()`
