#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
根据串号找到对应的串口设备
用法: ./find_port_by_serial.sh <串号>
"""

import sys
import os
import glob
import stat

def get_device_serial(device_path):
    """获取设备的序列号"""
    try:
        # 提取设备基本名称
        device_name = os.path.basename(device_path)
        
        # 在 /sys/class/tty/ 下找对应目录
        sys_path = "/sys/class/tty/" + device_name
        if not os.path.exists(sys_path):
            return None
        
        # 可能的序列号路径
        serial_paths = [
            os.path.join(sys_path, "device", "serial"),
            os.path.join(sys_path, "device", "../serial"),
            os.path.join(sys_path, "device", "../../serial"),
            os.path.join(sys_path, "device", "id", "serial"),
            os.path.join(sys_path, "device", "device", "serial")
        ]
        
        # 尝试读取序列号
        for path in serial_paths:
            if os.path.exists(path):
                with open(path, 'r') as f:
                    serial = f.read().strip()
                    if serial:
                        return serial
    except Exception as e:
        sys.stderr.write("Error reading serial from sysfs: {}\n".format(e))
    
    return None



def find_device_by_serial(target_serial):
    """根据序列号查找设备"""
    # 获取所有串口设备
    tty_devices = glob.glob("/dev/ttyACM*") + glob.glob("/dev/ttyUSB*")
    
    # 如果没有找到任何设备，输出调试信息
    if not tty_devices:
        sys.stderr.write("No ttyACM or ttyUSB devices found\n")
        try:
            # 列出所有tty设备，帮助调试
            os.system("ls -la /dev/tty* 2>/dev/null || true")
        except:
            pass
        return None
    
    # 逐个检查设备
    for device in tty_devices:
        if not os.path.exists(device):
            continue
        
        # 检查是否是字符设备
        try:
            mode = os.stat(device).st_mode
            is_char_device = stat.S_ISCHR(mode)
            if not is_char_device:
                continue
        except Exception as e:
            sys.stderr.write("Error checking device {}: {}\n".format(device, e))
            continue
        
        # 获取设备序列号
        serial = get_device_serial(device)
        
        if serial and serial == target_serial:
            return device
    
    return None

def main():
    # 检查参数
    if len(sys.argv) != 2:
        sys.stdout.write("Usage: {} <serial_number>\n".format(sys.argv[0]))
        sys.exit(1)
    
    serial_number = sys.argv[1]
    
    # 执行设备搜索
    device_path = find_device_by_serial(serial_number)
    
    if device_path:
        # 只输出设备路径，便于脚本捕获
        sys.stdout.write(device_path + "\n")
        sys.exit(0)
    else:
        sys.stderr.write("Device with serial number {} not found\n".format(serial_number))
        sys.exit(1)

if __name__ == "__main__":
    main()
