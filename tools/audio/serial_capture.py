#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
USB CDC串口录音脚本 - 使用自定义协议
详细协议规范请参考：docs/usb_cdc_protocol.md
"""

import serial
import argparse
import sys
import os
import time
import hashlib
import struct
import threading
from datetime import datetime
from enum import IntEnum


class FrameType(IntEnum):
    """帧类型定义"""
    CMD_REQUEST = 0x01
    CMD_RESPONSE = 0x02
    AUDIO_DATA = 0x03
    MD5_DATA = 0x04


class CommandID(IntEnum):
    """命令码定义"""
    START_RECORD = 0x01
    STOP_RECORD = 0x02
    QUERY_STATUS = 0x03


class StatusCode(IntEnum):
    """状态码定义"""
    OK = 0x00
    ERROR = 0x01
    BUSY = 0x02
    UNSUPPORTED = 0x03


class ParseState(IntEnum):
    """解析状态机"""
    MAGIC1 = 0
    MAGIC2 = 1
    TYPE = 2
    LENGTH = 3
    DATA = 4
    CHECKSUM = 5


class CDCProtocol:
    """CDC协议处理类"""

    MAGIC_MSB = 0xAA
    MAGIC_LSB = 0x55
    HEADER_SIZE = 7

    # 数据长度限制 - 区分命令帧和音频帧
    MAX_CMD_DATA_SIZE = 64      # 命令/响应/MD5最大数据长度

    def __init__(self):
        self.reset()

    def reset(self):
        """重置解析器状态"""
        self.state = ParseState.MAGIC1
        self.frame_type = 0
        self.data_length = 0
        self.data = bytearray()
        self.length_buf = bytearray()
        self.checksum = 0

    @staticmethod
    def calc_checksum(data):
        """计算XOR校验和"""
        checksum = 0
        for byte in data:
            checksum ^= byte
        return checksum

    def build_frame(self, frame_type, data=b''):
        """构建协议帧"""

        # 构建帧头
        frame = bytearray()
        frame.append(self.MAGIC_MSB)
        frame.append(self.MAGIC_LSB)
        frame.append(frame_type)

        # 添加长度（小端序）
        frame.extend(struct.pack('<I', len(data)))

        # 添加数据
        frame.extend(data)

        # 计算并添加校验和
        checksum = self.calc_checksum(frame)
        frame.append(checksum)

        return bytes(frame)

    def build_cmd_request(self, cmd_id, params=b''):
        """构建命令请求帧"""
        data = bytearray()
        data.append(cmd_id)
        data.extend(params)
        return self.build_frame(FrameType.CMD_REQUEST, bytes(data))

    def parse_byte(self, byte):
        """
        解析单个字节
        返回：None=需要更多数据，dict=帧解析完成
        """
        if self.state == ParseState.MAGIC1:
            if byte == self.MAGIC_MSB:
                self.state = ParseState.MAGIC2
            # 否则继续等待

        elif self.state == ParseState.MAGIC2:
            if byte == self.MAGIC_LSB:
                self.state = ParseState.TYPE
            else:
                # 魔数错误，重新开始
                self.reset()
                if byte == self.MAGIC_MSB:
                    self.state = ParseState.MAGIC2

        elif self.state == ParseState.TYPE:
            self.frame_type = byte
            self.state = ParseState.LENGTH
            self.length_buf = bytearray()

        elif self.state == ParseState.LENGTH:
            self.length_buf.append(byte)
            if len(self.length_buf) >= 4:
                # 小端序解析长度
                self.data_length = struct.unpack('<I', self.length_buf)[0]


                self.data = bytearray()
                if self.data_length > 0:
                    self.state = ParseState.DATA
                else:
                    self.state = ParseState.CHECKSUM

        elif self.state == ParseState.DATA:
            self.data.append(byte)
            if len(self.data) >= self.data_length:
                self.state = ParseState.CHECKSUM

        elif self.state == ParseState.CHECKSUM:
            self.checksum = byte

            # 音频帧（Type=0x03）的校验位固定为0x00，不进行校验
            if self.frame_type == FrameType.AUDIO_DATA:
                # 音频帧不验证校验和，固定填充0x00
                pass
            else:
                # 其他帧验证校验和
                frame_for_check = bytearray()
                frame_for_check.append(self.MAGIC_MSB)
                frame_for_check.append(self.MAGIC_LSB)
                frame_for_check.append(self.frame_type)
                frame_for_check.extend(struct.pack('<I', self.data_length))
                frame_for_check.extend(self.data)

                calc_checksum = self.calc_checksum(frame_for_check)

                if calc_checksum != self.checksum:
                    # 校验失败
                    print(f"[警告] 校验和错误: 期望0x{calc_checksum:02X}, 收到0x{self.checksum:02X}")
                    self.reset()
                    return None

            # 帧解析完成
            frame = {
                'type': self.frame_type,
                'data': bytes(self.data)
            }

            # 重置状态以准备下一帧
            self.reset()
            return frame

        return None


class AudioRecorder:
    """音频录音器"""

    def __init__(self, port, baudrate, output_file, timeout=1):
        self.port = port
        self.baudrate = baudrate
        self.output_file = output_file
        self.timeout = timeout

        self.serial = None
        self.protocol = CDCProtocol()
        self.audio_file = None
        self.md5_ctx = hashlib.md5()

        self.byte_count = 0
        self.seq_num = 0
        self.device_md5 = None
        self.running = False
        self.recording = False

        self.rx_thread = None
        self.lock = threading.Lock()

    def connect(self):
        """连接串口"""
        try:
            self.serial = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=self.timeout
            )

            print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] 串口已打开: {self.port}")
            print(f"波特率: {self.baudrate}")
            print(f"输出文件: {self.output_file}")
            print("-" * 50)

            # 清空串口缓存
            self.serial.reset_input_buffer()
            self.serial.reset_output_buffer()
            print("串口缓存已清空")

            return True

        except serial.SerialException as e:
            print(f"串口错误: {e}", file=sys.stderr)
            return False

    def disconnect(self):
        """断开串口"""
        if self.serial and self.serial.is_open:
            self.serial.close()
            print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] 串口已关闭")

    def send_command(self, cmd_id, params=b''):
        """发送命令"""
        if not self.serial or not self.serial.is_open:
            return False

        try:
            frame = self.protocol.build_cmd_request(cmd_id, params)
            self.serial.write(frame)
            self.serial.flush()

            cmd_name = {
                CommandID.START_RECORD: "开始录音",
                CommandID.STOP_RECORD: "停止录音",
                CommandID.QUERY_STATUS: "查询状态"
            }.get(cmd_id, f"未知命令(0x{cmd_id:02X})")

            print(f"[命令] 发送: {cmd_name}")
            return True

        except Exception as e:
            print(f"[错误] 发送命令失败: {e}")
            return False

    def handle_cmd_response(self, data):
        """处理命令响应"""
        if len(data) < 2:
            print("[警告] 命令响应数据过短")
            return

        cmd_id = data[0]
        status = data[1]
        response_data = data[2:] if len(data) > 2 else b''

        cmd_name = {
            CommandID.START_RECORD: "开始录音",
            CommandID.STOP_RECORD: "停止录音",
            CommandID.QUERY_STATUS: "查询状态"
        }.get(cmd_id, f"0x{cmd_id:02X}")

        status_name = {
            StatusCode.OK: "成功",
            StatusCode.ERROR: "失败",
            StatusCode.BUSY: "忙碌",
            StatusCode.UNSUPPORTED: "不支持"
        }.get(status, f"0x{status:02X}")

        print(f"[响应] {cmd_name}: {status_name}")

        # 如果是状态查询响应，解析详细信息
        if cmd_id == CommandID.QUERY_STATUS and status == StatusCode.OK and len(response_data) >= 9:
            state = response_data[0]
            total_bytes = struct.unpack('<I', response_data[1:5])[0]
            seq_num = struct.unpack('<I', response_data[5:9])[0]
            state_name = "正在录音" if state == 0x01 else "空闲"
            print(f"  状态: {state_name}")
            print(f"  已传输: {total_bytes} 字节")
            print(f"  序列号: {seq_num}")

    def handle_audio_data(self, data):
        """处理音频数据"""
        if len(data) < 4:
            print("[警告] 音频数据过短")
            return

        # 解析序列号
        seq_num = struct.unpack('<I', data[0:4])[0]
        pcm_data = data[4:]

        # 检查序列号
        if seq_num != self.seq_num:
            print(f"\n[警告] 序列号跳变: 期望{self.seq_num}, 收到{seq_num}")
            self.seq_num = seq_num

        # 写入文件
        if self.audio_file:
            self.audio_file.write(pcm_data)
            self.audio_file.flush()

            # 更新MD5
            self.md5_ctx.update(pcm_data)

            # 更新统计
            self.byte_count += len(pcm_data)
            self.seq_num += 1

            # 打印进度
            print(f"\r已接收: {self.byte_count} 字节 (序列号: {seq_num})", end='', flush=True)

    def handle_md5_data(self, data):
        """处理MD5数据"""
        if len(data) != 16:
            print(f"\n[警告] MD5数据长度错误: {len(data)}")
            return

        self.device_md5 = data.hex()
        print(f"\n[MD5] 设备端: {self.device_md5}")

    def handle_frame(self, frame):
        """处理接收到的帧"""
        frame_type = frame['type']
        data = frame['data']

        if frame_type == FrameType.CMD_RESPONSE:
            self.handle_cmd_response(data)
        elif frame_type == FrameType.AUDIO_DATA:
            self.handle_audio_data(data)
        elif frame_type == FrameType.MD5_DATA:
            self.handle_md5_data(data)
        else:
            print(f"[警告] 未知帧类型: 0x{frame_type:02X}")

    def receive_thread_func(self):
        """接收线程函数"""
        parser = CDCProtocol()

        while self.running:
            try:
                if self.serial and self.serial.is_open and self.serial.in_waiting > 0:
                    data = self.serial.read(self.serial.in_waiting)

                    # 逐字节解析
                    for byte in data:
                        frame = parser.parse_byte(byte)
                        if frame:
                            with self.lock:
                                self.handle_frame(frame)
                else:
                    time.sleep(0.01)

            except Exception as e:
                print(f"\n[错误] 接收线程异常: {e}")
                break

    def start_recording(self):
        """开始录音"""
        # 打开输出文件
        try:
            self.audio_file = open(self.output_file, 'wb')
        except IOError as e:
            print(f"文件错误: {e}", file=sys.stderr)
            return False

        # 重置统计
        self.byte_count = 0
        self.seq_num = 0
        self.md5_ctx = hashlib.md5()
        self.device_md5 = None

        # 启动接收线程
        self.running = True
        self.rx_thread = threading.Thread(target=self.receive_thread_func, daemon=True)
        self.rx_thread.start()

        # 发送开始录音命令
        print("-" * 50)
        if not self.send_command(CommandID.START_RECORD):
            self.running = False
            if self.audio_file:
                self.audio_file.close()
            return False

        # 等待响应
        time.sleep(0.5)

        self.recording = True
        print("开始接收音频数据... (按 Ctrl+C 停止)")
        print("-" * 50)

        return True

    def stop_recording(self):
        """停止录音"""
        if not self.recording:
            return

        print("\n\n停止录音...")
        print("-" * 50)

        # 发送停止录音命令
        self.send_command(CommandID.STOP_RECORD)

        # 等待剩余数据和MD5
        print("等待设备发送MD5...")
        time.sleep(2)

        # 停止接收线程
        self.recording = False
        self.running = False
        if self.rx_thread:
            self.rx_thread.join(timeout=3)

        # 关闭文件
        if self.audio_file:
            self.audio_file.close()
            self.audio_file = None

        # 计算本地MD5
        local_md5 = self.md5_ctx.hexdigest()

        print("-" * 50)
        print(f"录音完成!")
        print(f"总字节数: {self.byte_count}")
        print(f"总帧数: {self.seq_num}")
        print(f"PC端MD5: {local_md5}")

        if self.device_md5:
            print(f"设备端MD5: {self.device_md5}")
            if local_md5 == self.device_md5:
                print("MD5校验: 通过 ✓")
            else:
                print("MD5校验: 失败 ✗")
                print("[警告] 数据可能在传输过程中损坏!")
        else:
            print("设备端MD5: 未收到")

        print("-" * 50)

    def run(self):
        """运行录音"""
        if not self.connect():
            return False

        try:
            if not self.start_recording():
                return False

            # 等待用户中断
            while self.recording:
                time.sleep(0.1)

        except KeyboardInterrupt:
            print("\n\n用户中断")

        finally:
            self.stop_recording()
            self.disconnect()

        return True


def main():
    parser = argparse.ArgumentParser(
        description='USB CDC串口录音工具 - 使用自定义协议',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
使用示例:
  # Linux
  %(prog)s -p /dev/ttyACM0 -b 115200 -o output.pcm

  # Windows
  %(prog)s -p COM3 -b 115200 -o output.pcm

  # 指定超时时间
  %(prog)s -p /dev/ttyACM0 -b 921600 -o data.pcm -t 2

协议说明:
  本工具使用自定义二进制协议进行通信，详见 docs/usb_cdc_protocol.md

  命令流程:
  1. PC发送"开始录音"命令
  2. 设备响应确认
  3. 设备持续发送音频数据帧（带序列号）
  4. PC发送"停止录音"命令
  5. 设备发送MD5校验值
  6. PC对比MD5验证数据完整性
        '''
    )

    parser.add_argument(
        '-p', '--port',
        required=True,
        help='串口号 (Linux: /dev/ttyACM0, Windows: COM3)'
    )

    parser.add_argument(
        '-b', '--baudrate',
        type=int,
        required=True,
        help='波特率 (如: 9600, 115200, 921600)'
    )

    parser.add_argument(
        '-o', '--output',
        required=True,
        help='输出文件路径 (.pcm)'
    )

    parser.add_argument(
        '-t', '--timeout',
        type=float,
        default=1,
        help='串口读取超时时间(秒), 默认: 1'
    )

    args = parser.parse_args()

    # 检查输出目录是否存在
    output_dir = os.path.dirname(args.output)
    if output_dir and not os.path.exists(output_dir):
        print(f"创建输出目录: {output_dir}")
        os.makedirs(output_dir, exist_ok=True)

    # 创建录音器并运行
    recorder = AudioRecorder(args.port, args.baudrate, args.output, args.timeout)
    success = recorder.run()

    sys.exit(0 if success else 1)


if __name__ == '__main__':
    main()
