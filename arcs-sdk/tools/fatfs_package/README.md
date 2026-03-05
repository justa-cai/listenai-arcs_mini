# FAT32 文件系统镜像打包工具

## 简介

`mkfatfs.py` 是一个用于将指定目录打包为 FAT32 文件系统镜像的 Python 脚本。生成的镜像可以用于嵌入式系统、SD卡模拟等场景。

## 依赖安装

脚本依赖 `mtools` 工具包，需要先安装：

```bash
# Ubuntu/Debian
sudo apt-get install mtools

# CentOS/RHEL
sudo yum install mtools

# macOS
brew install mtools
```

## 使用方法

### 基本语法

```bash
./mkfatfs.py -o <输出镜像> -s <镜像大小> -d <源目录> [选项]
```

### 参数说明

- `-o, --output`: (必需) 输出的 FAT32 镜像文件路径
- `-s, --size`: (必需) 镜像大小，支持 K/M/G 后缀 (例如: 16M, 32M, 1G)
- `-d, --directory`: (必需) 要打包的源目录路径
- `-l, --label`: (可选) 卷标名称
- `-v, --verify`: (可选) 创建后列出镜像内容进行验证

### 使用示例

#### 示例 1: 创建基本的 FAT32 镜像

```bash
# 将 resources 目录打包为 16MB 的 FAT32 镜像
./mkfatfs.py -o disk.img -s 16M -d resources
```

#### 示例 2: 创建带卷标的镜像

```bash
# 创建 32MB 镜像，指定卷标为 MYDISK
./mkfatfs.py -o disk.img -s 32M -d mydata -l MYDISK
```

#### 示例 3: 创建镜像并验证

```bash
# 创建 128MB 镜像并列出内容
./mkfatfs.py -o disk.img -s 128M -d data -v
```

#### 示例 4: 用于嵌入式项目

```bash
# 为嵌入式系统创建资源镜像
./mkfatfs.py -o build/fatfs.img -s 8M -d assets/resources -l ASSETS
```

## 镜像大小说明

- FAT32 最小推荐大小: 32MB
- 对于较小的镜像 (<32MB)，mtools 可能自动使用 FAT12 或 FAT16
- 建议镜像大小略大于实际文件总大小，以预留文件系统元数据空间

## 目录结构

源目录的完整结构（包括子目录）都会被保留在生成的镜像中。

例如，源目录结构：
```
resources/
├── audio/
│   ├── music.mp3
│   └── sound.wav
└── images/
    ├── logo.png
    └── background.jpg
```

生成的镜像中将保持相同的目录结构。

## 验证镜像

可以使用以下命令验证生成的镜像：

```bash
# 使用 mtools 列出镜像内容
mdir -i disk.img -/

# 挂载镜像（Linux）
sudo mount -o loop disk.img /mnt
ls -R /mnt
sudo umount /mnt
```

## 故障排除

### mtools 未找到

如果出现 "mtools not found" 错误，请先安装 mtools 工具包。

### 镜像太小

如果源目录文件太多或太大，可能需要增加镜像大小。错误信息通常会提示空间不足。

### 文件名限制

FAT32 对文件名有一些限制：
- 长文件名最多 255 个字符
- 不支持某些特殊字符
- 文件名不区分大小写

## 相关工具

- `mdir`: 列出镜像内容
- `mcopy`: 复制文件到/从镜像
- `mdel`: 删除镜像中的文件
- `mmd`: 在镜像中创建目录
- `mlabel`: 设置/查看卷标

## 许可证

本工具遵循项目整体许可证。
