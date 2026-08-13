#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
AVI 文件信息解析工具

功能:
- 解析 AVI 文件结构 (RIFF/hdrl/movi)
- 显示视频/音频流头部信息
- 统计每一帧的时长和间隔
- 分析帧大小分布和码率

用法:
    python3 avi_info.py <avi_file>
    python3 avi_info.py video.avi
    python3 avi_info.py video.avi --all-frames  # 打印所有帧
"""

import sys
import struct
import argparse
from typing import Dict, List, Tuple
from pathlib import Path


class AVIParser:
    """AVI 文件解析器"""
    
    def __init__(self, filepath: str):
        self.filepath = filepath
        self.file_size = Path(filepath).stat().st_size
        self.f = None
        
        # 文件头信息
        self.avi_header = {}
        self.video_stream = {}
        self.audio_stream = {}
        
        # 帧信息
        self.video_frames = []
        self.audio_frames = []
        
    def read_fourcc(self) -> str:
        """读取 4 字节 FourCC"""
        return self.f.read(4).decode('ascii', errors='ignore')
    
    def read_dword(self) -> int:
        """读取 4 字节无符号整数 (小端序)"""
        return struct.unpack('<I', self.f.read(4))[0]
    
    def read_word(self) -> int:
        """读取 2 字节无符号整数"""
        return struct.unpack('<H', self.f.read(2))[0]
    
    def parse(self):
        """解析 AVI 文件"""
        with open(self.filepath, 'rb') as f:
            self.f = f
            
            # 解析 RIFF 头
            riff = self.read_fourcc()
            if riff != 'RIFF':
                raise ValueError(f"不是有效的 AVI 文件: {riff}")
            
            file_size = self.read_dword()
            avi_type = self.read_fourcc()
            if avi_type != 'AVI ':
                raise ValueError(f"不是 AVI 文件类型: {avi_type}")
            
            print(f"\n{'='*70}")
            print(f"文件: {self.filepath}")
            print(f"大小: {self.file_size:,} 字节 ({self.file_size/1024/1024:.2f} MB)")
            print(f"{'='*70}\n")
            
            # 解析各个 LIST
            while self.f.tell() < self.file_size - 8:
                chunk_id = self.read_fourcc()
                chunk_size = self.read_dword()
                chunk_end = self.f.tell() + chunk_size
                
                if chunk_id == 'LIST':
                    list_type = self.read_fourcc()
                    
                    if list_type == 'hdrl':
                        self._parse_hdrl(chunk_end)
                    elif list_type == 'movi':
                        self._parse_movi(chunk_end)
                    else:
                        # 跳过未知 LIST
                        self.f.seek(chunk_end)
                elif chunk_id == 'idx1':
                    # 跳过索引块
                    self.f.seek(chunk_end)
                else:
                    # 跳过未知块
                    self.f.seek(chunk_end)
                
                # 处理对齐
                if chunk_size % 2 == 1:
                    self.f.read(1)
    
    def _parse_hdrl(self, end_pos: int):
        """解析 hdrl (Header List)"""
        print("📋 AVI 头部信息 (hdrl)")
        print("-" * 70)
        
        while self.f.tell() < end_pos:
            chunk_id = self.read_fourcc()
            chunk_size = self.read_dword()
            chunk_start = self.f.tell()
            chunk_end = chunk_start + chunk_size
            
            if chunk_id == 'avih':
                self._parse_avih()
            elif chunk_id == 'LIST':
                list_type = self.read_fourcc()
                if list_type == 'strl':
                    self._parse_strl(chunk_end)
            
            self.f.seek(chunk_end)
            if chunk_size % 2 == 1:
                self.f.read(1)
        
        print()
    
    def _parse_avih(self):
        """解析 avih (Main AVI Header)"""
        micro_sec_per_frame = self.read_dword()
        max_bytes_per_sec = self.read_dword()
        padding_granularity = self.read_dword()
        flags = self.read_dword()
        total_frames = self.read_dword()
        initial_frames = self.read_dword()
        streams = self.read_dword()
        suggested_buffer_size = self.read_dword()
        width = self.read_dword()
        height = self.read_dword()
        
        fps = 1000000.0 / micro_sec_per_frame if micro_sec_per_frame > 0 else 0
        
        self.avi_header = {
            'micro_sec_per_frame': micro_sec_per_frame,
            'fps': fps,
            'max_bytes_per_sec': max_bytes_per_sec,
            'total_frames': total_frames,
            'streams': streams,
            'width': width,
            'height': height
        }
        
        print(f"  主头部 (avih):")
        print(f"    分辨率: {width} × {height}")
        print(f"    帧率: {fps:.2f} fps ({micro_sec_per_frame} μs/帧)")
        print(f"    总帧数: {total_frames:,}")
        print(f"    流数量: {streams}")
        print(f"    最大码率: {max_bytes_per_sec/1024:.1f} KB/s")
    
    def _parse_strl(self, end_pos: int):
        """解析 strl (Stream List)"""
        stream_info = {}
        
        while self.f.tell() < end_pos:
            chunk_id = self.read_fourcc()
            chunk_size = self.read_dword()
            chunk_start = self.f.tell()
            chunk_end = chunk_start + chunk_size
            
            if chunk_id == 'strh':
                stream_info = self._parse_strh()
            elif chunk_id == 'strf':
                if stream_info.get('type') == 'vids':
                    self._parse_video_format()
                elif stream_info.get('type') == 'auds':
                    self._parse_audio_format()
            
            self.f.seek(chunk_end)
            if chunk_size % 2 == 1:
                self.f.read(1)
    
    def _parse_strh(self) -> Dict:
        """解析 strh (Stream Header)"""
        fcc_type = self.read_fourcc()
        fcc_handler = self.read_fourcc()
        flags = self.read_dword()
        priority = self.read_word()
        language = self.read_word()
        initial_frames = self.read_dword()
        scale = self.read_dword()
        rate = self.read_dword()
        start = self.read_dword()
        length = self.read_dword()
        suggested_buffer_size = self.read_dword()
        quality = self.read_dword()
        sample_size = self.read_dword()
        
        stream_type = "视频" if fcc_type == 'vids' else "音频" if fcc_type == 'auds' else "未知"
        
        print(f"\n  流头部 (strh) - {stream_type}:")
        print(f"    类型: {fcc_type} ({stream_type})")
        print(f"    编码: {fcc_handler}")
        print(f"    时间基准: {rate}/{scale} = {rate/scale if scale > 0 else 0:.2f} Hz")
        print(f"    长度: {length:,} 帧/样本")
        
        return {'type': fcc_type, 'handler': fcc_handler}
    
    def _parse_video_format(self):
        """解析视频格式 (BITMAPINFOHEADER)"""
        size = self.read_dword()
        width = self.read_dword()
        height = self.read_dword()
        planes = self.read_word()
        bit_count = self.read_word()
        compression = self.read_fourcc()
        size_image = self.read_dword()
        x_pels_per_meter = self.read_dword()
        y_pels_per_meter = self.read_dword()
        clr_used = self.read_dword()
        clr_important = self.read_dword()
        
        self.video_stream = {
            'width': width,
            'height': height,
            'bit_count': bit_count,
            'compression': compression,
            'size_image': size_image
        }
        
        print(f"    格式 (strf):")
        print(f"      分辨率: {width} × {height}")
        print(f"      压缩: {compression}")
        print(f"      位深: {bit_count} bits")
        print(f"      图像大小: {size_image:,} 字节")
    
    def _parse_audio_format(self):
        """解析音频格式 (WAVEFORMATEX)"""
        format_tag = self.read_word()
        channels = self.read_word()
        samples_per_sec = self.read_dword()
        avg_bytes_per_sec = self.read_dword()
        block_align = self.read_word()
        bits_per_sample = self.read_word()
        
        format_name = "PCM" if format_tag == 1 else f"Unknown({format_tag})"
        channel_name = "Mono" if channels == 1 else "Stereo" if channels == 2 else f"{channels}ch"
        
        self.audio_stream = {
            'format': format_tag,
            'channels': channels,
            'sample_rate': samples_per_sec,
            'bit_rate': avg_bytes_per_sec,
            'bits_per_sample': bits_per_sample
        }
        
        print(f"    格式 (strf):")
        print(f"      编码: {format_name}")
        print(f"      采样率: {samples_per_sec:,} Hz")
        print(f"      通道: {channel_name}")
        print(f"      位深: {bits_per_sample} bits")
        print(f"      码率: {avg_bytes_per_sec*8/1000:.1f} kbps")
    
    def _parse_movi(self, end_pos: int):
        """解析 movi (Movie Data)"""
        print("🎬 媒体数据 (movi)")
        print("-" * 70)
        
        frame_count = 0
        video_frame_num = 0
        audio_frame_num = 0
        
        while self.f.tell() < end_pos - 8:
            chunk_id = self.read_fourcc()
            chunk_size = self.read_dword()
            
            if chunk_id == 'LIST':
                # 跳过嵌套的 LIST
                list_type = self.read_fourcc()
                chunk_size -= 4
            
            chunk_data_start = self.f.tell()
            
            # 解析帧类型
            if chunk_id == '00dc' or chunk_id == '00db':
                # 视频帧
                self.video_frames.append({
                    'index': video_frame_num,
                    'size': chunk_size,
                    'offset': chunk_data_start,
                    'type': 'compressed' if chunk_id == '00dc' else 'uncompressed'
                })
                video_frame_num += 1
            elif chunk_id == '01wb':
                # 音频帧
                self.audio_frames.append({
                    'index': audio_frame_num,
                    'size': chunk_size,
                    'offset': chunk_data_start
                })
                audio_frame_num += 1
            
            # 跳过数据
            self.f.seek(chunk_data_start + chunk_size)
            
            # 处理对齐
            if chunk_size % 2 == 1:
                self.f.read(1)
            
            frame_count += 1
            
            # 每 1000 帧打印进度
            if frame_count % 1000 == 0:
                print(f"  解析中... {frame_count:,} 帧 (视频: {video_frame_num:,}, 音频: {audio_frame_num:,})")
        
        print(f"\n  总计: {frame_count:,} 帧")
        print(f"    视频帧: {video_frame_num:,}")
        print(f"    音频帧: {audio_frame_num:,}")
        print()
    
    def analyze_frames(self):
        """分析帧信息"""
        print("📊 帧统计分析")
        print("-" * 70)
        
        # 视频帧分析
        if self.video_frames:
            print("\n  视频帧:")
            total_size = sum(f['size'] for f in self.video_frames)
            avg_size = total_size / len(self.video_frames)
            min_size = min(f['size'] for f in self.video_frames)
            max_size = max(f['size'] for f in self.video_frames)
            
            # 计算帧率和时长
            fps = self.avi_header.get('fps', 0)
            duration = len(self.video_frames) / fps if fps > 0 else 0
            frame_interval = 1000.0 / fps if fps > 0 else 0
            
            print(f"    总帧数: {len(self.video_frames):,}")
            print(f"    时长: {duration:.2f} 秒")
            print(f"    帧间隔: {frame_interval:.2f} ms ({fps:.2f} fps)")
            print(f"    总大小: {total_size:,} 字节 ({total_size/1024/1024:.2f} MB)")
            print(f"    平均帧大小: {avg_size:,.0f} 字节")
            print(f"    帧大小范围: {min_size:,} - {max_size:,} 字节")
            print(f"    视频码率: {total_size*8/duration/1000:.1f} kbps" if duration > 0 else "")
            
            # 帧大小分布
            print(f"\n    帧大小分布:")
            bins = [0, 1024, 2048, 4096, 8192, 16384, float('inf')]
            bin_labels = ['<1KB', '1-2KB', '2-4KB', '4-8KB', '8-16KB', '>16KB']
            bin_counts = [0] * len(bin_labels)
            
            for frame in self.video_frames:
                for i, limit in enumerate(bins[1:]):
                    if frame['size'] < limit:
                        bin_counts[i] += 1
                        break
            
            for label, count in zip(bin_labels, bin_counts):
                if count > 0:
                    percent = count * 100.0 / len(self.video_frames)
                    print(f"      {label:>8}: {count:6,} ({percent:5.1f}%)")
        
        # 音频帧分析
        if self.audio_frames:
            print("\n  音频帧:")
            total_size = sum(f['size'] for f in self.audio_frames)
            avg_size = total_size / len(self.audio_frames)
            
            # 计算时长
            sample_rate = self.audio_stream.get('sample_rate', 0)
            channels = self.audio_stream.get('channels', 1)
            bits_per_sample = self.audio_stream.get('bits_per_sample', 16)
            bytes_per_sample = bits_per_sample // 8 * channels
            
            if bytes_per_sample > 0 and sample_rate > 0:
                samples_per_frame = avg_size / bytes_per_sample
                frame_duration = samples_per_frame / sample_rate * 1000
                total_duration = len(self.audio_frames) * frame_duration / 1000
            else:
                frame_duration = 0
                total_duration = 0
            
            print(f"    总帧数: {len(self.audio_frames):,}")
            print(f"    时长: {total_duration:.2f} 秒" if total_duration > 0 else "")
            print(f"    帧间隔: {frame_duration:.2f} ms" if frame_duration > 0 else "")
            print(f"    总大小: {total_size:,} 字节 ({total_size/1024/1024:.2f} MB)")
            print(f"    平均帧大小: {avg_size:,.0f} 字节")
            print(f"    音频码率: {total_size*8/total_duration/1000:.1f} kbps" if total_duration > 0 else "")
        
        print()
    
    def print_sample_frames(self, num_frames: int = 10):
        """打印前 N 帧的详细信息"""
        print(f"📄 前 {num_frames} 帧详细信息")
        print("-" * 70)
        
        self._print_frames_table(num_frames)
    
    def print_all_frames(self):
        """打印所有帧的详细信息"""
        total_frames = len(self.video_frames) + len(self.audio_frames)
        print(f"📄 所有帧详细信息 (共 {total_frames:,} 帧)")
        print("-" * 70)
        
        self._print_frames_table(total_frames)
    
    def _print_frames_table(self, max_frames: int):
        """打印帧信息表格"""
        # 合并视频和音频帧按 offset 排序
        all_frames = []
        for vf in self.video_frames:
            all_frames.append(('V', vf['index'], vf['size'], vf['offset']))
        for af in self.audio_frames:
            all_frames.append(('A', af['index'], af['size'], af['offset']))
        
        all_frames.sort(key=lambda x: x[3])
        
        fps = self.avi_header.get('fps', 25)
        sample_rate = self.audio_stream.get('sample_rate', 44100)
        channels = self.audio_stream.get('channels', 2)
        bits_per_sample = self.audio_stream.get('bits_per_sample', 16)
        
        print(f"\n  {'类型':<4} {'帧号':>6} {'大小':>10} {'时间戳':>10}  说明")
        print(f"  {'-'*4} {'-'*6} {'-'*10} {'-'*10}  {'-'*30}")
        
        count = 0
        for frame_type, index, size, offset in all_frames:
            if count >= max_frames:
                break
                
            if frame_type == 'V':
                timestamp = index * 1000.0 / fps if fps > 0 else 0
                print(f"  {'视频':<4} {index:6,} {size:10,} {timestamp:9.1f}ms  JPEG 压缩帧")
            else:
                bytes_per_sample = bits_per_sample // 8 * channels
                samples = size / bytes_per_sample if bytes_per_sample > 0 else 0
                timestamp = index * samples / sample_rate * 1000 if sample_rate > 0 else 0
                duration = samples / sample_rate * 1000 if sample_rate > 0 else 0
                print(f"  {'音频':<4} {index:6,} {size:10,} {timestamp:9.1f}ms  PCM ({duration:.1f}ms)")
            
            count += 1
        
        if count < len(all_frames):
            print(f"\n  ... 省略 {len(all_frames) - count:,} 帧")
        
        print()


def main():
    """主函数"""
    parser = argparse.ArgumentParser(
        description='AVI 文件信息解析工具',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
示例:
  %(prog)s video.avi                  # 显示基本信息和前15帧
  %(prog)s video.avi --all-frames     # 显示所有帧详细信息
  %(prog)s video.avi -a               # 同上(简写)
  %(prog)s video.avi > info.log       # 保存到文件
        '''
    )
    
    parser.add_argument('file', help='AVI 文件路径')
    parser.add_argument('-a', '--all-frames', action='store_true',
                        help='打印所有帧的详细信息(默认只显示前15帧)')
    
    args = parser.parse_args()
    filepath = args.file
    
    if not Path(filepath).exists():
        print(f"❌ 文件不存在: {filepath}")
        sys.exit(1)
    
    try:
        parser = AVIParser(filepath)
        parser.parse()
        parser.analyze_frames()
        
        if args.all_frames:
            parser.print_all_frames()
        else:
            parser.print_sample_frames(15)
        
        print("✅ 解析完成!")
        
    except Exception as e:
        print(f"\n❌ 解析失败: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == '__main__':
    main()
