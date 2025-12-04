import os
import argparse
import re

def batch_rename(folder, old_pattern, new_pattern):
    """
    批量重命名文件
    :param folder: 文件夹路径
    :param old_pattern: 旧文件名模式（支持正则）
    :param new_pattern: 新文件名模式（支持 {num} 格式化）
    """
    for filename in os.listdir(folder):
        # 匹配旧文件名（如 frame-000001.png）
        match = re.fullmatch(old_pattern, filename)
        if match:
            # 提取数字部分（如 000001）
            num_str = match.group(1)
            num = int(num_str)  # 转为整数（去掉前导零）
            
            # 构造新文件名（如 wakeup_001.png）
            new_filename = new_pattern.format(num=num)
            
            # 重命名文件
            old_path = os.path.join(folder, filename)
            new_path = os.path.join(folder, new_filename)
            os.rename(old_path, new_path)
            print(f"Renamed: {filename} -> {new_filename}")

if __name__ == "__main__":
    # 设置命令行参数
    parser = argparse.ArgumentParser(description="批量重命名文件")
    parser.add_argument("--folder", required=True, help="文件夹路径")
    parser.add_argument("--old", required=True, help="旧文件名模式（正则表达式）")
    parser.add_argument("--new", required=True, help="新文件名模式（支持 {num} 格式化）")
    args = parser.parse_args()

    # 执行批量重命名
    batch_rename(args.folder, args.old, args.new)
    print("批量重命名完成！")