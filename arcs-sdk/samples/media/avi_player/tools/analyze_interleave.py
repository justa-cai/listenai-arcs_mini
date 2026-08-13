#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
AVI 交错模式分析工具

分析 AVI 文件中音视频帧的交错模式,找出规律
"""

import sys
from pathlib import Path
from collections import Counter


def analyze_interleave_pattern(log_file: str):
    """分析交错模式"""
    
    print("🔍 AVI 交错模式分析")
    print("=" * 70)
    
    # 读取日志文件
    with open(log_file, 'r', encoding='utf-8') as f:
        lines = f.readlines()
    
    # 提取帧信息
    frames = []
    in_frame_section = False
    
    for line in lines:
        if '所有帧详细信息' in line or '前 15 帧' in line:
            in_frame_section = True
            continue
        
        if in_frame_section:
            if line.strip().startswith('视频'):
                frames.append('V')
            elif line.strip().startswith('音频'):
                frames.append('A')
            elif '✅' in line or '省略' in line:
                break
    
    if not frames:
        print("❌ 未找到帧信息,请确保日志文件包含 --all-frames 选项的输出")
        return
    
    print(f"\n总帧数: {len(frames):,} (视频: {frames.count('V'):,}, 音频: {frames.count('A'):,})")
    
    # 分析交错模式
    print("\n" + "=" * 70)
    print("📊 交错模式统计")
    print("=" * 70)
    
    # 1. 查找连续音频帧
    audio_runs = []
    current_run = 0
    for frame in frames:
        if frame == 'A':
            current_run += 1
        else:
            if current_run > 0:
                audio_runs.append(current_run)
            current_run = 0
    if current_run > 0:
        audio_runs.append(current_run)
    
    audio_run_stats = Counter(audio_runs)
    print(f"\n连续音频帧数量分布:")
    for num_audio, count in sorted(audio_run_stats.items()):
        percent = count * 100.0 / len(audio_runs)
        print(f"  {num_audio} 音频帧连续: {count:5,} 次 ({percent:5.1f}%)")
    
    # 2. 查找视频帧间的音频帧数量
    video_gaps = []
    audio_count = 0
    for frame in frames:
        if frame == 'A':
            audio_count += 1
        else:  # 'V'
            video_gaps.append(audio_count)
            audio_count = 0
    
    if video_gaps:
        gap_stats = Counter(video_gaps)
        print(f"\n视频帧间音频帧数量分布:")
        for num_audio, count in sorted(gap_stats.items()):
            percent = count * 100.0 / len(video_gaps)
            print(f"  每个视频帧后跟 {num_audio} 个音频帧: {count:5,} 次 ({percent:5.1f}%)")
        
        avg_gap = sum(video_gaps) / len(video_gaps)
        print(f"\n  平均: 每个视频帧后跟 {avg_gap:.2f} 个音频帧")
    
    # 3. 查找重复模式
    print(f"\n" + "=" * 70)
    print("🔄 重复交错模式")
    print("=" * 70)
    
    # 查找前100帧的模式
    pattern_length = min(100, len(frames))
    pattern = ''.join(frames[:pattern_length])
    
    # 尝试找出重复单元
    for unit_len in range(2, 20):
        unit = pattern[:unit_len]
        # 检查是否重复
        repeated = unit * (pattern_length // unit_len)
        if pattern.startswith(repeated):
            print(f"\n发现重复单元 (长度 {unit_len}):")
            print(f"  模式: {unit}")
            
            # 统计这个模式
            v_count = unit.count('V')
            a_count = unit.count('A')
            print(f"  组成: {v_count} 个视频帧 + {a_count} 个音频帧")
            print(f"  比例: 1 视频 : {a_count/v_count:.2f} 音频" if v_count > 0 else "")
            break
    else:
        print("\n未发现明显的重复模式")
        print(f"前 20 帧交错序列: {''.join(frames[:20])}")
    
    # 4. 显示前30帧的详细模式
    print(f"\n" + "=" * 70)
    print("📋 前 30 帧交错序列")
    print("=" * 70)
    
    print("\n  ", end="")
    for i, frame in enumerate(frames[:30]):
        if frame == 'V':
            print(f"[V{sum(1 for f in frames[:i+1] if f == 'V')-1}]", end=" ")
        else:
            print(f"A{sum(1 for f in frames[:i+1] if f == 'A')-1}", end=" ")
        
        if (i + 1) % 10 == 0:
            print("\n  ", end="")
    
    print("\n\n  说明: [Vn] = 视频帧n, An = 音频帧n")
    
    # 5. 时间同步分析
    print(f"\n" + "=" * 70)
    print("⏱️  时间同步分析")
    print("=" * 70)
    
    # 从日志提取帧率和采样率
    video_fps = None
    audio_interval = None
    
    for line in lines:
        if '帧率:' in line and 'fps' in line:
            try:
                video_fps = float(line.split('fps')[0].split(':')[-1].strip())
            except:
                pass
        if '帧间隔:' in line and '音频帧' in lines[lines.index(line)-5:lines.index(line)]:
            try:
                audio_interval = float(line.split('ms')[0].split(':')[-1].strip())
            except:
                pass
    
    if video_fps and audio_interval:
        video_interval = 1000.0 / video_fps
        ratio = video_interval / audio_interval
        
        print(f"\n  视频帧间隔: {video_interval:.2f} ms ({video_fps:.2f} fps)")
        print(f"  音频帧间隔: {audio_interval:.2f} ms")
        print(f"  理论比例: 1 视频 : {ratio:.2f} 音频")
        print(f"\n  交错策略: 按时间戳顺序交错 (Time-based Interleaving)")
        print(f"  说明: 编码器按播放时间戳将音视频帧排序,保证同步")


def main():
    """主函数"""
    if len(sys.argv) < 2:
        print(f"用法: {sys.argv[0]} <info.log>")
        print(f"\n示例:")
        print(f"  # 先生成包含所有帧的日志")
        print(f"  python3 avi_info.py video.avi --all-frames > info.log")
        print(f"  # 然后分析交错模式")
        print(f"  python3 {sys.argv[0]} info.log")
        sys.exit(1)
    
    log_file = sys.argv[1]
    
    if not Path(log_file).exists():
        print(f"❌ 文件不存在: {log_file}")
        sys.exit(1)
    
    try:
        analyze_interleave_pattern(log_file)
        print("\n✅ 分析完成!")
        
    except Exception as e:
        print(f"\n❌ 分析失败: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == '__main__':
    main()
