import os
import re
import subprocess
from PIL import Image
from datetime import datetime

# 基础配置
ADB_PATH = "adb"
DEVICE_PATH = "/SD:/adb/"
# 生成日期时间作为文件夹名
now = datetime.now()
date_string = now.strftime("%Y%m%d_%H%M%S")
OUTPUT_DIR = f"./build/output/{date_string}"
RAW_FRAMES_DIR = os.path.join(OUTPUT_DIR, "raw_frames")

def run_adb_command(command):
    """执行ADB命令并返回输出"""
    try:
        result = subprocess.run(
            command,
            shell=True,
            capture_output=True,
            text=True,
            check=True
        )
        return result.stdout.strip()
    except subprocess.CalledProcessError as e:
        print(f"ADB命令执行失败: {e}")
        return None

def parse_dimensions(filename):
    """从文件名解析宽高"""
    match = re.search(r"_(\d+)x(\d+)\.yuv$", filename)
    if match:
        return int(match.group(1)), int(match.group(2))
    return None, None

def process_yuv400_file(filepath, output_dir, is_multi_frame=False):
    """处理YUV400文件"""
    # 从文件名获取尺寸
    filename = os.path.basename(filepath)
    width, height = parse_dimensions(filename)
    if not width or not height:
        print(f"无法解析尺寸: {filename}")
        return

    # 读取YUV数据
    with open(filepath, "rb") as f:
        yuv_data = f.read()

    # 计算帧数
    frame_size = width * height
    total_frames = len(yuv_data) // frame_size

    # 创建输出目录
    os.makedirs(output_dir, exist_ok=True)

    # 处理帧数据
    for frame_idx in range(total_frames if is_multi_frame else 1):
        start = frame_idx * frame_size
        end = start + frame_size
        frame_data = yuv_data[start:end]

        # 创建灰度图像
        img = Image.frombytes("L", (width, height), frame_data)

        # 生成输出路径
        output_path = os.path.join(
            output_dir,
            f"{os.path.splitext(filename)[0]}_frame{frame_idx+1:03d}.jpg"
        )
        img.save(output_path, "JPEG")

def main():
    # 创建输出目录
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    # 获取设备文件列表
    ls_output = run_adb_command(f"{ADB_PATH} ls {DEVICE_PATH}")
    if not ls_output:
        return

    device_path = DEVICE_PATH

    # 如果该目录下有多个文件，则在显示在控制台
    if len(ls_output.split("\n")) > 1:
        print("发现多个文件夹:\n")
        print(ls_output)
        # 控制台输入
        folder = input("请输入要拉取的文件夹:")
        device_path = f"{DEVICE_PATH}/{folder}"
        ls_output = run_adb_command(f"{ADB_PATH} ls {device_path}")
        if not ls_output:
            print("未找到指定文件夹\n")
            return
    else:
        device_path = DEVICE_PATH + "/" + re.compile('.+ .+ .+ ').sub('', ls_output.split('\n')[0]).strip()
        ls_output = run_adb_command(f"{ADB_PATH} ls {device_path}")
        if not ls_output:
            return

    # 筛选目标文件
    target_files = []
    prefixes = (r".+ .+ .+ raw_img", r".+ .+ .+ stitch_img", r".+ .+ .+ cutline_img")
    for line in ls_output.split("\n"):
        if any(re.match(prefix, line) for prefix in prefixes) and line.endswith(".yuv"):
            target_files.append(re.compile('.+ .+ .+ ').sub('', line).strip())

    # 拉取文件到本地
    for filename in target_files:
        remote_path = os.path.join(device_path, filename).replace("\\", "/")
        exit_code = os.system(f"{ADB_PATH} pull {remote_path} {OUTPUT_DIR}")
        if exit_code != 0:
            print(f"文件拉取失败: {filename}")
            continue

        local_path = os.path.join(OUTPUT_DIR, filename)

        # 根据文件类型处理
        if filename.startswith("raw_img"):
            process_yuv400_file(local_path, RAW_FRAMES_DIR, is_multi_frame=True)
        else:
            process_yuv400_file(local_path, OUTPUT_DIR, is_multi_frame=False)

if __name__ == "__main__":
    main()
