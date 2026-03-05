#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
USB CDC串口录音工具 - GUI版本
支持实时波形显示和录音控制
"""

import sys
import os
import serial
import time
import hashlib
import struct
import threading
import queue
from datetime import datetime
from enum import IntEnum
import numpy as np

# GUI相关
import tkinter as tk
from tkinter import ttk, filedialog, messagebox
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from matplotlib.figure import Figure
import matplotlib.animation as animation


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
    MAX_CMD_DATA_SIZE = 64
    MAX_AUDIO_DATA_SIZE = 65536
    MAX_DATA_SIZE = MAX_AUDIO_DATA_SIZE

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
        frame = bytearray()
        frame.append(self.MAGIC_MSB)
        frame.append(self.MAGIC_LSB)
        frame.append(frame_type)
        frame.extend(struct.pack('<I', len(data)))
        frame.extend(data)
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
        """解析单个字节"""
        if self.state == ParseState.MAGIC1:
            if byte == self.MAGIC_MSB:
                self.state = ParseState.MAGIC2

        elif self.state == ParseState.MAGIC2:
            if byte == self.MAGIC_LSB:
                self.state = ParseState.TYPE
            else:
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
                self.data_length = struct.unpack('<I', self.length_buf)[0]
                if self.data_length > self.MAX_DATA_SIZE:
                    self.reset()
                    return None
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

            # 音频帧不验证校验和
            if self.frame_type == FrameType.AUDIO_DATA:
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
                    self.reset()
                    return None

            # 帧解析完成
            frame = {
                'type': self.frame_type,
                'data': bytes(self.data)
            }
            self.reset()
            return frame

        return None


class AudioRecorderGUI:
    """带GUI的音频录音器"""

    def __init__(self, root):
        self.root = root
        self.root.title("USB CDC 录音工具")
        self.root.geometry("900x700")

        # 录音状态
        self.recording = False
        self.serial = None
        self.rx_thread = None
        self.running = False
        self.audio_file = None
        self.byte_count = 0
        self.seq_num = 0
        self.md5_ctx = None
        self.device_md5 = None
        self.is_saving_file = False  # 记录当前是否在保存文件

        # 音频数据队列（用于波形显示）
        self.audio_queue = queue.Queue(maxsize=100)
        self.waveform_data = {}  # 字典，key为通道号，value为numpy数组
        self.max_waveform_samples = 16000  # 显示1秒的数据（假设16kHz采样率）
        self.x_axis_max = 16000  # X轴显示的最大样本数（可通过滚轮调节）
        self.x_axis_auto_fit = False  # 是否自动适应显示所有数据
        self.x_view_position = 0  # X轴视图起始位置（用于滚动条）
        self.auto_scroll = True  # 是否自动滚动到最新数据
        self.updating_scrollbar = False  # 标志：正在程序更新滚动条（防止触发回调）

        # 波形线条（多通道）
        self.waveform_lines = []
        self.channel_colors = ['b', 'r', 'g', 'orange', 'purple', 'cyan', 'magenta', 'brown']

        # Y轴范围控制（每个通道独立）
        self.y_limits = {}  # {channel: (ymin, ymax)}

        # 协议解析器
        self.protocol = CDCProtocol()
        self.lock = threading.Lock()

        # 创建GUI
        self.create_widgets()

    def create_widgets(self):
        """创建GUI组件"""
        # === 顶部：配置区域 ===
        config_frame = ttk.LabelFrame(self.root, text="配置", padding=10)
        config_frame.pack(fill=tk.X, padx=10, pady=5)

        # 串口选择
        ttk.Label(config_frame, text="串口:").grid(row=0, column=0, sticky=tk.W, padx=5)
        self.port_var = tk.StringVar(value="/dev/ttyACM0")
        port_entry = ttk.Entry(config_frame, textvariable=self.port_var, width=20)
        port_entry.grid(row=0, column=1, padx=5)

        # 波特率
        ttk.Label(config_frame, text="波特率:").grid(row=0, column=2, sticky=tk.W, padx=5)
        self.baud_var = tk.StringVar(value="921600")
        baud_combo = ttk.Combobox(config_frame, textvariable=self.baud_var, width=15,
                                   values=["115200", "460800", "921600", "1500000"])
        baud_combo.grid(row=0, column=3, padx=5)

        # 采样率
        ttk.Label(config_frame, text="采样率:").grid(row=0, column=4, sticky=tk.W, padx=5)
        self.sample_rate_var = tk.StringVar(value="16000")
        sample_rate_combo = ttk.Combobox(config_frame, textvariable=self.sample_rate_var, width=10,
                                         values=["8000", "16000", "32000", "48000"],
                                         state="readonly")
        sample_rate_combo.grid(row=0, column=5, padx=5)

        # 位深
        ttk.Label(config_frame, text="位深:").grid(row=0, column=6, sticky=tk.W, padx=5)
        self.bit_depth_var = tk.StringVar(value="16")
        bit_depth_combo = ttk.Combobox(config_frame, textvariable=self.bit_depth_var, width=8,
                                       values=["16"],
                                       state="readonly")
        bit_depth_combo.grid(row=0, column=7, padx=5)

        # 通道数
        ttk.Label(config_frame, text="通道数:").grid(row=0, column=8, sticky=tk.W, padx=5)
        self.channels_var = tk.StringVar(value="1")
        channels_combo = ttk.Combobox(config_frame, textvariable=self.channels_var, width=8,
                                      values=["1", "2", "3", "4", "5", "6", "8"],
                                      state="readonly")
        channels_combo.grid(row=0, column=9, padx=5)
        channels_combo.bind("<<ComboboxSelected>>", self.on_channels_changed)

        # 是否保存文件选项和输出文件（同一行）
        self.save_file_var = tk.BooleanVar(value=True)
        save_check = ttk.Checkbutton(config_frame, text="保存文件:",
                                      variable=self.save_file_var,
                                      command=self.toggle_file_controls)
        save_check.grid(row=1, column=0, sticky=tk.W, padx=5, pady=5)

        self.output_var = tk.StringVar(value="output.pcm")
        self.output_entry = ttk.Entry(config_frame, textvariable=self.output_var, width=35)
        self.output_entry.grid(row=1, column=1, columnspan=2, padx=5, pady=5, sticky=tk.W)

        self.browse_btn = ttk.Button(config_frame, text="浏览...", command=self.browse_output_file)
        self.browse_btn.grid(row=1, column=3, padx=5, pady=5)

        # === 中间：控制按钮区域 ===
        control_frame = ttk.Frame(self.root, padding=10)
        control_frame.pack(fill=tk.X, padx=10, pady=5)

        self.start_btn = ttk.Button(control_frame, text="开始录音", command=self.start_recording,
                                     state=tk.NORMAL, width=15)
        self.start_btn.pack(side=tk.LEFT, padx=5)

        self.stop_btn = ttk.Button(control_frame, text="停止录音", command=self.stop_recording,
                                    state=tk.DISABLED, width=15)
        self.stop_btn.pack(side=tk.LEFT, padx=5)

        # === 波形显示区域 ===
        waveform_frame = ttk.LabelFrame(self.root, text="实时波形", padding=10)
        waveform_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)

        # 创建matplotlib图形（初始为空，稍后根据通道数创建子图）
        self.fig = Figure(figsize=(10, 6), dpi=100)
        self.axes = []  # 多个子图的列表

        # 将matplotlib嵌入到tkinter
        self.canvas = FigureCanvasTkAgg(self.fig, master=waveform_frame)
        self.canvas.draw()
        self.canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True)

        # 添加横向滚动条
        scrollbar_frame = ttk.Frame(waveform_frame)
        scrollbar_frame.pack(fill=tk.X, pady=(5, 0))

        ttk.Label(scrollbar_frame, text="时间轴:").pack(side=tk.LEFT, padx=5)

        self.x_scrollbar = ttk.Scale(scrollbar_frame, from_=0, to=100, orient=tk.HORIZONTAL,
                                     command=self.on_x_scroll)
        self.x_scrollbar.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=5)

        self.auto_scroll_var = tk.BooleanVar(value=True)
        self.auto_scroll_check = ttk.Checkbutton(scrollbar_frame, text="自动滚动",
                                                  variable=self.auto_scroll_var,
                                                  command=self.toggle_auto_scroll)
        self.auto_scroll_check.pack(side=tk.LEFT, padx=5)

        # 初始化波形线（多通道）- 必须在canvas创建后
        self.init_waveform_lines()

        # 绑定鼠标滚轮事件用于Y轴范围调节
        self.canvas.mpl_connect('scroll_event', self.on_scroll)

        # === 底部：状态信息区域 ===
        status_frame = ttk.LabelFrame(self.root, text="状态信息", padding=10)
        status_frame.pack(fill=tk.X, padx=10, pady=5)

        # 状态标签
        self.status_label = ttk.Label(status_frame, text="就绪", foreground="green", font=("Arial", 10, "bold"))
        self.status_label.grid(row=0, column=0, columnspan=4, sticky=tk.W, pady=5)

        # 统计信息
        ttk.Label(status_frame, text="已接收:").grid(row=1, column=0, sticky=tk.W)
        self.bytes_label = ttk.Label(status_frame, text="0 字节", width=20)
        self.bytes_label.grid(row=1, column=1, sticky=tk.W)

        ttk.Label(status_frame, text="序列号:").grid(row=1, column=2, sticky=tk.W, padx=20)
        self.seq_label = ttk.Label(status_frame, text="0", width=15)
        self.seq_label.grid(row=1, column=3, sticky=tk.W)

        ttk.Label(status_frame, text="PC MD5:").grid(row=2, column=0, sticky=tk.W)
        self.pc_md5_label = ttk.Label(status_frame, text="-", width=40)
        self.pc_md5_label.grid(row=2, column=1, columnspan=3, sticky=tk.W)

        ttk.Label(status_frame, text="设备 MD5:").grid(row=3, column=0, sticky=tk.W)
        self.device_md5_label = ttk.Label(status_frame, text="-", width=40)
        self.device_md5_label.grid(row=3, column=1, columnspan=3, sticky=tk.W)

        # 启动波形更新动画
        self.ani = animation.FuncAnimation(self.fig, self.update_waveform,
                                          interval=100, blit=False)

    def init_waveform_lines(self):
        """初始化波形线（每个通道独立子图）"""
        # 清除所有旧的子图
        self.fig.clear()
        self.axes = []
        self.waveform_lines = []

        # 获取通道数
        num_channels = int(self.channels_var.get())

        # 为每个通道创建独立的子图
        for ch in range(num_channels):
            # 创建子图 (行数, 列数, 索引)
            ax = self.fig.add_subplot(num_channels, 1, ch + 1)

            # 设置子图属性 - 将通道标签放在左边作为Y轴标签
            ax.set_ylabel(f'CH{ch}', fontsize=10, fontweight='bold', rotation=0,
                         labelpad=20, va='center')
            ax.grid(True, alpha=0.3)
            ax.set_ylim(-32768, 32767)

            # 设置Y轴刻度标签字体大小
            ax.tick_params(axis='y', labelsize=7)
            ax.tick_params(axis='x', labelsize=7)

            # 只在最后一个子图显示X轴标签
            if ch == num_channels - 1:
                ax.set_xlabel('样本', fontsize=8)
            else:
                ax.set_xticklabels([])

            # 为该子图创建波形线
            color = self.channel_colors[ch % len(self.channel_colors)]
            line, = ax.plot([], [], color=color, linewidth=0.8)

            self.axes.append(ax)
            self.waveform_lines.append(line)

            # 初始化该通道的Y轴范围
            self.y_limits[ch] = (-32768, 32767)

        # 调整子图间距 - 更紧凑的布局
        self.fig.subplots_adjust(left=0.08, right=0.98, top=0.98, bottom=0.05, hspace=0.15)

        # 初始化波形数据字典
        self.waveform_data = {ch: np.array([]) for ch in range(num_channels)}

        self.canvas.draw()

    def on_channels_changed(self, event=None):
        """通道数改变时的回调"""
        if not self.recording:
            self.init_waveform_lines()
            # 更新最大样本数（根据采样率）
            sample_rate = int(self.sample_rate_var.get())
            self.max_waveform_samples = sample_rate  # 显示1秒数据
            self.x_axis_max = sample_rate  # 同步更新X轴最大值

    def on_x_scroll(self, value):
        """X轴滚动条回调"""
        # 当用户拖动滚动条时，禁用自动滚动
        if self.auto_scroll_var.get():
            self.auto_scroll_var.set(False)
            self.auto_scroll = False

        # 更新视图位置（value 是0-100的百分比）
        self.x_view_position = float(value)

    def toggle_auto_scroll(self):
        """切换自动滚动模式"""
        self.auto_scroll = self.auto_scroll_var.get()
        if self.auto_scroll:
            # 启用自动滚动时，将视图移到最右侧
            self.x_scrollbar.set(100)
            self.x_view_position = 100

    def on_scroll(self, event):
        """鼠标滚轮事件 - 调整Y轴或X轴范围"""
        if event.y is None:
            return

        target_channel = None
        target_ax = None

        # 先尝试检测鼠标是否在某个子图内
        if event.inaxes is not None:
            for ch, ax in enumerate(self.axes):
                if event.inaxes == ax:
                    target_channel = ch
                    target_ax = ax
                    break

        # 如果不在子图内，检测鼠标Y坐标在哪个子图的垂直范围内（包括Y轴区域和间隙）
        if target_channel is None:
            for ch, ax in enumerate(self.axes):
                bbox = ax.get_window_extent()
                # 扩大检测范围：上下各扩展10像素
                if bbox.y0 - 10 <= event.y <= bbox.y1 + 10:
                    target_channel = ch
                    target_ax = ax
                    break

        # 如果找到了目标通道
        if target_channel is not None and target_ax is not None:
            bbox = target_ax.get_window_extent()

            # 判断鼠标在Y轴左侧还是右侧（波形区域）
            # Y轴区域大约在子图左边界后60像素内
            y_axis_right_edge = bbox.x0 + 60

            if event.x is not None and event.x > y_axis_right_edge:
                # 鼠标在波形区域（Y轴右侧）- 调节X轴范围（所有通道同步）
                zoom_factor = 0.9 if event.button == 'up' else 1.1

                if self.x_axis_auto_fit and event.button == 'up':
                    # 当前是自动适应模式，向上滚动开始缩放
                    sample_rate = int(self.sample_rate_var.get())
                    self.x_axis_max = int(sample_rate * zoom_factor)
                    self.x_axis_auto_fit = False
                else:
                    # 计算新的X轴最大值
                    new_x_max = self.x_axis_max * zoom_factor

                    # 限制范围：最小800个样本，最大100000个样本
                    new_x_max = max(800, min(100000, int(new_x_max)))

                    # 如果缩小到最小值，切换到自动适应模式
                    if new_x_max <= 800 and event.button == 'down':
                        self.x_axis_auto_fit = True
                    else:
                        self.x_axis_auto_fit = False
                        self.x_axis_max = new_x_max

                # 不在这里直接设置xlim，让update_waveform处理
                self.canvas.draw_idle()

            else:
                # 鼠标在Y轴区域 - 调节该通道的Y轴范围
                # 获取当前Y轴范围
                ymin, ymax = self.y_limits.get(target_channel, (-32768, 32767))
                y_range = ymax - ymin
                y_center = (ymin + ymax) / 2

                # 滚轮向上：缩小范围（放大显示）
                # 滚轮向下：扩大范围（缩小显示）
                zoom_factor = 0.9 if event.button == 'up' else 1.1

                # 计算新的Y轴范围
                new_range = y_range * zoom_factor

                # 限制最小和最大范围
                new_range = max(100, min(65536, new_range))

                # 计算新的Y轴上下限（保持中心不变）
                new_ymin = y_center - new_range / 2
                new_ymax = y_center + new_range / 2

                # 更新Y轴范围
                self.y_limits[target_channel] = (new_ymin, new_ymax)
                target_ax.set_ylim(new_ymin, new_ymax)
                self.canvas.draw_idle()

    def parse_interleaved_pcm(self, pcm_data):
        """
        解析交叉排列的PCM数据
        输入格式: ch0 ch1 ch2 ... ch0 ch1 ch2 ...
        返回: {0: [ch0_samples], 1: [ch1_samples], ...}
        """
        num_channels = int(self.channels_var.get())
        bit_depth = int(self.bit_depth_var.get())

        # 将bytes转换为int16数组
        if bit_depth == 16:
            samples = np.frombuffer(pcm_data, dtype=np.int16)
        else:
            # 未来可以扩展支持其他位深
            samples = np.frombuffer(pcm_data, dtype=np.int16)

        # 分离各通道数据
        channel_data = {}
        for ch in range(num_channels):
            channel_data[ch] = samples[ch::num_channels]

        return channel_data

    def browse_output_file(self):
        """浏览输出文件"""
        filename = filedialog.asksaveasfilename(
            defaultextension=".pcm",
            filetypes=[("PCM files", "*.pcm"), ("WAV files", "*.wav"), ("All files", "*.*")]
        )
        if filename:
            self.output_var.set(filename)

    def toggle_file_controls(self):
        """切换文件控件的启用状态"""
        if self.save_file_var.get():
            # 启用文件控件
            self.output_entry.config(state=tk.NORMAL)
            self.browse_btn.config(state=tk.NORMAL)
        else:
            # 禁用文件控件
            self.output_entry.config(state=tk.DISABLED)
            self.browse_btn.config(state=tk.DISABLED)

    def update_status(self, message, color="black"):
        """更新状态信息"""
        self.status_label.config(text=message, foreground=color)

    def update_statistics(self):
        """更新统计信息"""
        self.bytes_label.config(text=f"{self.byte_count} 字节")
        self.seq_label.config(text=str(self.seq_num))

    def update_waveform(self, frame):
        """更新波形显示（多通道支持）"""
        try:
            # 从队列获取新的音频数据
            while not self.audio_queue.empty():
                try:
                    new_data = self.audio_queue.get_nowait()

                    # 解析交叉排列的多通道数据
                    channel_data = self.parse_interleaved_pcm(new_data)

                    # 更新每个通道的数据
                    for ch, samples in channel_data.items():
                        if ch in self.waveform_data:
                            # 拼接新数据
                            self.waveform_data[ch] = np.concatenate([self.waveform_data[ch], samples])

                            # 限制最大缓冲区为1000000个样本
                            max_buffer_size = 1000000
                            if len(self.waveform_data[ch]) > max_buffer_size:
                                self.waveform_data[ch] = self.waveform_data[ch][-max_buffer_size:]

                except queue.Empty:
                    break

            # 更新每条波形线和对应子图的X轴
            num_channels = int(self.channels_var.get())

            for ch in range(num_channels):
                if ch < len(self.waveform_lines) and ch < len(self.axes) and ch in self.waveform_data:
                    data = self.waveform_data[ch]
                    if len(data) > 0:
                        # 更新波形线数据
                        self.waveform_lines[ch].set_data(range(len(data)), data)

                        # 为每个子图设置X轴范围
                        if self.x_axis_auto_fit:
                            # 自动适应模式：显示所有数据
                            self.axes[ch].set_xlim(0, max(len(data), 1000))
                        else:
                            # 固定窗口模式 - 根据滚动条位置显示
                            if len(data) <= self.x_axis_max:
                                # 数据不足，从0开始显示
                                self.axes[ch].set_xlim(0, max(self.x_axis_max, 1000))
                            else:
                                # 根据滚动条位置计算显示范围
                                if self.auto_scroll:
                                    # 自动滚动：显示最新数据
                                    x_end = len(data)
                                    x_start = max(0, x_end - self.x_axis_max)
                                    # 同步更新滚动条位置（不触发回调）
                                    self.x_scrollbar.set(100)
                                else:
                                    # 手动滚动：根据滚动条位置显示
                                    total_range = len(data) - self.x_axis_max
                                    x_start = int((self.x_view_position / 100.0) * total_range)
                                    x_start = max(0, min(x_start, total_range))
                                    x_end = x_start + self.x_axis_max

                                self.axes[ch].set_xlim(x_start, x_end)

                        # 应用该通道的Y轴范围
                        if ch in self.y_limits:
                            self.axes[ch].set_ylim(self.y_limits[ch])

        except Exception as e:
            print(f"波形更新错误: {e}")

        return tuple(self.waveform_lines) if self.waveform_lines else ()

    def send_command(self, cmd_id, params=b''):
        """发送命令"""
        if not self.serial or not self.serial.is_open:
            return False

        frame = self.protocol.build_cmd_request(cmd_id, params)
        try:
            self.serial.write(frame)
            return True
        except Exception as e:
            self.update_status(f"发送命令失败: {e}", "red")
            return False

    def handle_cmd_response(self, data):
        """处理命令响应"""
        if len(data) < 2:
            return

        cmd_id = data[0]
        status = data[1]

        status_str = {0: "成功", 1: "失败", 2: "忙碌", 3: "不支持"}.get(status, "未知")
        cmd_str = {1: "开始录音", 2: "停止录音", 3: "查询状态"}.get(cmd_id, f"命令{cmd_id}")

        message = f"{cmd_str}: {status_str}"
        color = "green" if status == 0 else "red"
        self.update_status(message, color)

    def handle_audio_data(self, data):
        """处理音频数据"""
        if len(data) < 4:
            return

        # 解析序列号
        seq_num = struct.unpack('<I', data[0:4])[0]
        pcm_data = data[4:]

        # 检查序列号
        if seq_num != self.seq_num:
            self.seq_num = seq_num

        # 写入文件（仅当启用保存时）
        if self.audio_file:
            self.audio_file.write(pcm_data)
            self.audio_file.flush()

        # 更新MD5（无论是否保存文件都需要计算）
        self.md5_ctx.update(pcm_data)

        # 更新统计
        self.byte_count += len(pcm_data)
        self.seq_num += 1

        # 更新GUI
        self.root.after(0, self.update_statistics)

        # 添加到波形队列（无论是否保存文件都显示波形）
        try:
            self.audio_queue.put_nowait(pcm_data)
        except queue.Full:
            # 队列满了，丢弃旧数据
            try:
                self.audio_queue.get_nowait()
                self.audio_queue.put_nowait(pcm_data)
            except:
                pass

    def handle_md5_data(self, data):
        """处理MD5数据"""
        if len(data) != 16:
            return

        self.device_md5 = data.hex()
        self.device_md5_label.config(text=self.device_md5)

        # 比较MD5
        pc_md5 = self.md5_ctx.hexdigest()
        self.pc_md5_label.config(text=pc_md5)

        # 显示结果
        if pc_md5 == self.device_md5:
            if self.is_saving_file:
                self.update_status("录音完成，MD5校验通过 ✓", "green")
            else:
                self.update_status("录音完成（未保存文件），MD5校验通过 ✓", "green")
        else:
            if self.is_saving_file:
                self.update_status("录音完成，MD5校验失败 ✗", "red")
            else:
                self.update_status("录音完成（未保存文件），MD5校验失败 ✗", "red")

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
                print(f"接收线程异常: {e}")
                break

    def start_recording(self):
        """开始录音"""
        port = self.port_var.get()
        baudrate = int(self.baud_var.get())
        save_file = self.save_file_var.get()
        output_file = self.output_var.get()

        # 打开串口
        try:
            self.serial = serial.Serial(port, baudrate, timeout=1)
            self.update_status(f"串口已打开: {port}", "green")
        except Exception as e:
            messagebox.showerror("错误", f"无法打开串口: {e}")
            return

        # 打开输出文件（仅当启用保存时）
        if save_file:
            try:
                self.audio_file = open(output_file, 'wb')
            except IOError as e:
                messagebox.showerror("错误", f"无法创建文件: {e}")
                self.serial.close()
                return
        else:
            self.audio_file = None  # 不保存文件

        # 重置统计
        self.byte_count = 0
        self.seq_num = 0
        self.md5_ctx = hashlib.md5()
        self.device_md5 = None

        # 重置波形数据为字典格式（多通道）
        num_channels = int(self.channels_var.get())
        self.waveform_data = {ch: np.array([]) for ch in range(num_channels)}

        # 重置X轴最大值和自动适应模式
        sample_rate = int(self.sample_rate_var.get())
        self.x_axis_max = sample_rate
        self.x_axis_auto_fit = False  # 开始录音时使用固定窗口模式

        # 重置滚动条状态
        self.x_view_position = 0
        self.auto_scroll = True
        self.auto_scroll_var.set(True)
        self.x_scrollbar.set(100)

        self.pc_md5_label.config(text="-")
        self.device_md5_label.config(text="-")
        self.update_statistics()

        # 启动接收线程
        self.running = True
        self.rx_thread = threading.Thread(target=self.receive_thread_func, daemon=True)
        self.rx_thread.start()

        # 发送开始录音命令
        time.sleep(0.1)
        if not self.send_command(CommandID.START_RECORD):
            self.update_status("发送开始录音命令失败", "red")
            self.running = False
            if self.audio_file:
                self.audio_file.close()
            if self.serial:
                self.serial.close()
            return

        self.recording = True
        self.is_saving_file = save_file  # 记录是否保存文件
        self.start_btn.config(state=tk.DISABLED)
        self.stop_btn.config(state=tk.NORMAL)

        # 更新状态信息
        if save_file:
            self.update_status(f"正在录音并保存到: {output_file}", "blue")
        else:
            self.update_status("正在录音（仅显示波形，不保存文件）", "blue")

    def stop_recording(self):
        """停止录音"""
        if not self.recording:
            return

        self.recording = False
        self.update_status("正在停止录音...", "orange")

        # 发送停止录音命令
        if self.send_command(CommandID.STOP_RECORD):
            # 等待MD5数据
            time.sleep(1)

        # 停止接收线程
        self.running = False
        if self.rx_thread:
            self.rx_thread.join(timeout=2)

        # 关闭文件和串口
        if self.audio_file:
            self.audio_file.close()
            self.audio_file = None

        if self.serial and self.serial.is_open:
            self.serial.close()
            self.serial = None

        self.start_btn.config(state=tk.NORMAL)
        self.stop_btn.config(state=tk.DISABLED)

        # 如果没有收到MD5数据，显示停止信息
        if not self.device_md5:
            if self.is_saving_file:
                self.update_status("录音已停止", "orange")
            else:
                self.update_status("录音已停止（未保存文件）", "orange")

    def on_closing(self):
        """窗口关闭事件"""
        if self.recording:
            if messagebox.askokcancel("退出", "录音正在进行，确定要退出吗？"):
                self.stop_recording()
                time.sleep(0.5)
                self.root.destroy()
        else:
            self.root.destroy()


def main():
    """主函数"""
    root = tk.Tk()
    app = AudioRecorderGUI(root)
    root.protocol("WM_DELETE_WINDOW", app.on_closing)
    root.mainloop()


if __name__ == "__main__":
    main()
