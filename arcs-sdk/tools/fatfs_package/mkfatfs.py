#!/usr/bin/env python3
"""
FAT32 文件系统镜像生成工具

该脚本用于将指定目录打包为 FAT32 文件系统镜像文件。
支持自定义镜像大小、卷标等参数。

依赖：
  - mtools (mcopy, mformat, mlabel 等工具)

安装依赖：
  Ubuntu/Debian: sudo apt-get install mtools
  CentOS/RHEL:   sudo yum install mtools
  macOS:         brew install mtools
"""

import os
import sys
import argparse
import subprocess
import shutil
import tempfile


def parse_size(size_str):
    """解析大小字符串，支持 K/M/G 后缀"""
    size_str = size_str.strip().upper()
    multipliers = {
        'K': 1024,
        'M': 1024 * 1024,
        'G': 1024 * 1024 * 1024,
    }

    if size_str[-1] in multipliers:
        return int(size_str[:-1]) * multipliers[size_str[-1]]
    else:
        try:
            return int(size_str, 0)
        except ValueError:
            return int(size_str)


def check_mtools():
    """检查 mtools 是否已安装"""
    try:
        subprocess.run(['mformat', '--version'],
                      stdout=subprocess.PIPE,
                      stderr=subprocess.PIPE,
                      check=False)
        return True
    except FileNotFoundError:
        return False


def create_fat_image(output_file, size_bytes, label=None):
    """
    创建空的 FAT 文件系统镜像

    Args:
        output_file: 输出镜像文件路径
        size_bytes: 镜像大小（字节）
        label: 卷标（可选）
    """
    # 创建空镜像文件
    with open(output_file, 'wb') as f:
        f.write(b'\x00' * size_bytes)

    print(f"Created blank image file: {output_file} ({size_bytes} bytes)")

    # 根据镜像大小自动选择 FAT 类型
    # FAT12: < 16MB
    # FAT16: 16MB - 2GB
    # FAT32: > 32MB (推荐)
    size_mb = size_bytes / (1024 * 1024)
    if size_mb >= 32:
        fat_type = '32'
    elif size_mb >= 16:
        fat_type = '16'
    else:
        fat_type = '12'

    # 使用 mformat 格式化为 FAT 文件系统
    # mformat -i <image> -v <label> ::
    # 不使用 -F 参数，让 mformat 根据大小自动选择 FAT 类型
    cmd = ['mformat', '-i', output_file]

    # 添加卷标
    if label:
        cmd.extend(['-v', label])

    cmd.append('::')

    try:
        result = subprocess.run(cmd, capture_output=True, text=True, check=True)
        print(f"Formatted as FAT{fat_type} filesystem")
        if result.stdout:
            print(result.stdout)
    except subprocess.CalledProcessError as e:
        print(f"Error formatting image: {e}")
        print(f"stderr: {e.stderr}")
        sys.exit(1)

    return fat_type


def copy_directory_to_image(image_file, source_dir):
    """
    将目录内容复制到 FAT 镜像中

    Args:
        image_file: FAT 镜像文件路径
        source_dir: 源目录路径
    """
    if not os.path.isdir(source_dir):
        print(f"Error: Source directory does not exist: {source_dir}")
        sys.exit(1)

    # 遍历源目录，复制所有文件和子目录
    for root, dirs, files in os.walk(source_dir):
        # 计算相对路径
        rel_path = os.path.relpath(root, source_dir)

        # 在镜像中创建对应的目录结构
        if rel_path != '.':
            # mtools 路径格式: ::/path/to/dir
            mtools_path = '::/' + rel_path.replace(os.sep, '/')
            cmd = ['mmd', '-i', image_file, mtools_path]
            try:
                subprocess.run(cmd, capture_output=True, text=True, check=True)
                print(f"Created directory: {mtools_path}")
            except subprocess.CalledProcessError as e:
                # 目录可能已存在，忽略错误
                if 'exists' not in e.stderr.lower():
                    print(f"Warning: Failed to create directory {mtools_path}: {e.stderr}")

        # 复制当前目录下的所有文件
        for filename in files:
            src_file = os.path.join(root, filename)

            if rel_path == '.':
                dst_path = '::/' + filename
            else:
                dst_path = '::/' + rel_path.replace(os.sep, '/') + '/' + filename

            cmd = ['mcopy', '-i', image_file, '-o', src_file, dst_path]
            try:
                subprocess.run(cmd, capture_output=True, text=True, check=True)
                print(f"Copied: {src_file} -> {dst_path}")
            except subprocess.CalledProcessError as e:
                print(f"Error copying {src_file}: {e.stderr}")
                sys.exit(1)


def get_directory_size(directory):
    """
    计算目录的总大小（包括所有文件和子目录）

    Args:
        directory: 目录路径

    Returns:
        tuple: (总大小(字节), 文件数量)
    """
    total_size = 0
    file_count = 0

    for root, dirs, files in os.walk(directory):
        for filename in files:
            file_path = os.path.join(root, filename)
            try:
                file_size = os.path.getsize(file_path)
                total_size += file_size
                file_count += 1
            except (OSError, IOError) as e:
                print(f"Warning: Cannot access file {file_path}: {e}")

    return total_size, file_count


def format_size(size_bytes):
    """将字节数格式化为人类可读的格式"""
    for unit in ['B', 'KB', 'MB', 'GB']:
        if size_bytes < 1024.0:
            return f"{size_bytes:.2f} {unit}"
        size_bytes /= 1024.0
    return f"{size_bytes:.2f} TB"


def list_image_contents(image_file):
    """列出镜像中的文件内容"""
    print(f"\nContents of {image_file}:")
    cmd = ['mdir', '-i', image_file, '-/']
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, check=True)
        print(result.stdout)
    except subprocess.CalledProcessError as e:
        print(f"Error listing image contents: {e.stderr}")


def main():
    parser = argparse.ArgumentParser(
        description='Create FAT32 filesystem image from a directory',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # 创建 16MB 的 FAT32 镜像，打包 resources 目录
  %(prog)s -o disk.img -s 16M -d resources

  # 创建 32MB 的镜像，指定卷标
  %(prog)s -o disk.img -s 32M -d mydata -l MYDISK

  # 创建并验证镜像内容
  %(prog)s -o disk.img -s 128M -d data -v
        """
    )

    parser.add_argument('-o', '--output', required=True,
                       help='Output FAT32 image file path')
    parser.add_argument('-s', '--size', required=True,
                       help='Image size (e.g., 16M, 32M, 1G)')
    parser.add_argument('-d', '--directory', required=True,
                       help='Source directory to package')
    parser.add_argument('-l', '--label',
                       help='Volume label (optional)')
    parser.add_argument('-v', '--verify', action='store_true',
                       help='List image contents after creation')

    args = parser.parse_args()

    # 检查 mtools
    if not check_mtools():
        print("Error: mtools not found. Please install mtools:")
        print("  Ubuntu/Debian: sudo apt-get install mtools")
        print("  CentOS/RHEL:   sudo yum install mtools")
        print("  macOS:         brew install mtools")
        sys.exit(1)

    # 解析镜像大小
    try:
        size_bytes = parse_size(args.size)
    except (ValueError, KeyError) as e:
        print(f"Error: Invalid size format: {args.size}")
        print("Use format like: 16M, 32M, 1G, or numeric bytes")
        sys.exit(1)

    # 检查源目录
    if not os.path.isdir(args.directory):
        print(f"Error: Source directory not found: {args.directory}")
        sys.exit(1)

    # 计算源目录大小
    print(f"Analyzing source directory...")
    dir_size, file_count = get_directory_size(args.directory)

    # FAT32 文件系统元数据开销约为 5-10%，这里按 15% 计算以留有余量
    # 实际可用空间约为镜像大小的 85%
    overhead_ratio = 0.15
    usable_size = int(size_bytes * (1 - overhead_ratio))

    print(f"Directory size: {format_size(dir_size)} ({dir_size} bytes)")
    print(f"File count:     {file_count}")
    print(f"Image size:     {format_size(size_bytes)} ({size_bytes} bytes)")
    print(f"Usable size:    ~{format_size(usable_size)} (after FAT32 overhead)")
    print()

    # 检查目录大小是否超过镜像可用空间
    if dir_size > usable_size:
        print(f"ERROR: Source directory is too large!")
        print(f"  Directory size:        {format_size(dir_size)}")
        print(f"  Usable image size:     {format_size(usable_size)}")
        print(f"  Minimum required size: {format_size(int(dir_size / (1 - overhead_ratio)))}")
        print()
        print(f"Suggestion: Increase image size to at least {format_size(int(dir_size / (1 - overhead_ratio)) * 1.1)}")
        sys.exit(1)

    # 如果空间紧张（使用率超过80%），给出警告
    usage_ratio = dir_size / usable_size
    if usage_ratio > 0.8:
        print(f"WARNING: Disk space will be tight ({usage_ratio * 100:.1f}% usage)")
        print(f"  Consider increasing image size for better reliability")
        print()

    # 创建输出目录
    output_dir = os.path.dirname(os.path.abspath(args.output))
    if output_dir and not os.path.exists(output_dir):
        os.makedirs(output_dir)
        print(f"Created output directory: {output_dir}")

    print(f"=== Creating FAT Image ===")
    print(f"Output file: {args.output}")
    print(f"Source dir:  {args.directory}")
    if args.label:
        print(f"Volume label: {args.label}")
    print()

    # 创建 FAT 镜像（自动选择 FAT 类型）
    fat_type = create_fat_image(args.output, size_bytes, args.label)

    # 复制目录内容到镜像
    print(f"\n=== Copying files to image ===")
    copy_directory_to_image(args.output, args.directory)

    # 验证（可选）
    if args.verify:
        list_image_contents(args.output)

    print(f"\n=== Success ===")
    print(f"FAT{fat_type} image created: {args.output}")


if __name__ == '__main__':
    main()
