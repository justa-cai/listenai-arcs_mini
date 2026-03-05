#!/usr/bin/env python3
"""
合并多个 bin 文件到一个完整的固件文件
空白区域填充 0xFF（Flash 擦除后的默认值）
"""

import os
import sys
from pathlib import Path

# 固件配置：(地址偏移, bin文件路径)
FIRMWARE_MAP = [
    (0x000000, "./res/ap.bin"),
    (0x100000, "./res/algo/face_detect_thinker.bin"),
    (0x170000, "./res/algo/face_align_thinker.bin"),
    (0x300000, "./res/algo/face_nir_thinker_lineart_split.bin"),
    (0x410000, "./res/algo/face_verify_thinker.bin"),
    (0x800000, "./build/arcs.bin"),
]

# 输出文件配置
OUTPUT_FILE = "./merged_firmware.bin"
FILL_BYTE = 0xFF  # 空白区域填充字节（0xFF 或 0x00）

def merge_bins():
    """合并所有 bin 文件"""
    
    # 检查所有输入文件是否存在
    print("检查输入文件...")
    for offset, filepath in FIRMWARE_MAP:
        if not os.path.exists(filepath):
            print(f"❌ 错误: 文件不存在: {filepath}")
            return False
        file_size = os.path.getsize(filepath)
        print(f"  ✓ 0x{offset:08X}: {filepath} ({file_size} bytes)")
    
    # 计算总固件大小（最后一个文件的结束位置）
    last_offset, last_file = FIRMWARE_MAP[-1]
    last_size = os.path.getsize(last_file)
    total_size = last_offset + last_size
    
    print(f"\n固件总大小: {total_size} bytes ({total_size / 1024 / 1024:.2f} MB)")
    
    # 创建填充的固件缓冲区
    print(f"创建固件缓冲区，填充字节: 0x{FILL_BYTE:02X}...")
    firmware = bytearray([FILL_BYTE] * total_size)
    
    # 将每个 bin 文件写入对应位置
    print("\n合并 bin 文件...")
    for offset, filepath in FIRMWARE_MAP:
        print(f"  写入 {filepath} 到偏移 0x{offset:08X}...")
        with open(filepath, 'rb') as f:
            data = f.read()
            firmware[offset:offset + len(data)] = data
            print(f"    ✓ 已写入 {len(data)} bytes")
    
    # 写入输出文件
    print(f"\n保存合并后的固件到 {OUTPUT_FILE}...")
    with open(OUTPUT_FILE, 'wb') as f:
        f.write(firmware)
    
    output_size = os.path.getsize(OUTPUT_FILE)
    print(f"✓ 完成! 输出文件大小: {output_size} bytes ({output_size / 1024 / 1024:.2f} MB)")
    
    # 显示固件布局
    print("\n固件布局:")
    print("=" * 70)
    print(f"{'起始地址':<12} {'结束地址':<12} {'大小':<12} {'文件'}")
    print("-" * 70)
    
    for i, (offset, filepath) in enumerate(FIRMWARE_MAP):
        file_size = os.path.getsize(filepath)
        end_offset = offset + file_size
        
        # 显示当前文件
        print(f"0x{offset:08X}  0x{end_offset:08X}  {file_size:>10} B  {filepath}")
        
        # 显示空白区域
        if i < len(FIRMWARE_MAP) - 1:
            next_offset = FIRMWARE_MAP[i + 1][0]
            gap_size = next_offset - end_offset
            if gap_size > 0:
                print(f"0x{end_offset:08X}  0x{next_offset:08X}  {gap_size:>10} B  (填充 0x{FILL_BYTE:02X})")
    
    print("=" * 70)
    
    return True

def main():
    """主函数"""
    print("=" * 70)
    print("固件合并工具")
    print("=" * 70)
    print()
    
    # 切换到脚本所在目录
    script_dir = Path(__file__).parent
    os.chdir(script_dir)
    print(f"工作目录: {os.getcwd()}\n")
    
    # 执行合并
    if merge_bins():
        print("\n✓ 固件合并成功!")
        print(f"\n烧录命令:")
        print(f"  cskburn -s /dev/ttyACM0 -b 3000000 -C arcs 0x00 {OUTPUT_FILE}")
        return 0
    else:
        print("\n❌ 固件合并失败!")
        return 1

if __name__ == "__main__":
    sys.exit(main())
