import os
import shutil
import argparse

def read_deployignore(file_path):
    """读取 .deployignore 文件，并返回一个忽略模式列表"""
    ignore_patterns = []
    if os.path.exists(file_path):
        with open(file_path, 'r') as f:
            for line in f:
                line = line.strip()
                if line and not line.startswith('#'):
                    ignore_patterns.append(line)
    return ignore_patterns

def should_ignore(file_path, ignore_patterns):
    """检查文件或目录名称是否应该被忽略"""
    # 获取文件或目录的名称
    file_name = os.path.basename(file_path)

    for pattern in ignore_patterns:
        if pattern.endswith('/'):
            # 如果模式是目录，比较名称
            if file_name == pattern[:-1]:
                print(f"ignore directory:{file_path}, {file_name}")
                return True
        else:
            # 检查名称是否匹配
            if file_name == pattern:
                print(f"ignore file:{file_path}, {file_name}")
                return True

    return False

def copy_with_ignore(src, dst, ignore_patterns):
    """将文件从 src 复制到 dst，忽略 .deployignore 中的配置"""
    os.makedirs(dst, exist_ok=True)
    for root, dirs, files in os.walk(src):

        # 计算当前目录的相对路径
        relative_path = os.path.relpath(root, src)

        # 过滤被忽略的目录
        dirs[:] = [d for d in dirs if not should_ignore(os.path.join(root, d), ignore_patterns)]

        # 如果当前目录被忽略，跳过该目录及其内容
        if should_ignore(root, ignore_patterns):
            continue

        # 创建目标目录
        target_dir = os.path.join(dst, relative_path)
        os.makedirs(target_dir, exist_ok=True)

        for file_name in files:
            file_path = os.path.join(root, file_name)
            if not should_ignore(file_path, ignore_patterns):
                shutil.copy(file_path, target_dir)


if __name__ == "__main__":
    # 设置命令行参数解析
    parser = argparse.ArgumentParser(description='Copy files while ignoring patterns from .deployignore.')
    parser.add_argument('-s', '--source', type=str, required=True, help='The source directory to copy files from.')
    parser.add_argument('-d', '--destination', type=str, required=True, help='The destination directory to copy files to.')
    parser.add_argument('-i','--deployignore', type=str, default='.deployignore', help='Path to the .deployignore file.')

    args = parser.parse_args()

    # 读取 .deployignore 文件
    deployignore_file = os.path.join(args.source, args.deployignore)
    ignore_patterns = read_deployignore(deployignore_file)

    # 执行文件复制
    copy_with_ignore(args.source, args.destination, ignore_patterns)
