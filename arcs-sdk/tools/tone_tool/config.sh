#!/bin/bash

# 平台类型
OS_ARCH=`arch`

############ 工具文件配置 ############
# FFPEG x86-64 平台工具
# 如需其它平台工具, 请自行下载后更改此配置
FFMPEG_EXE=$CUR_DIR/cmd/ffmpeg-64

# 打包音频文件为tone.bin的工具
TONETOOL_EXE=$CUR_DIR/cmd/tone_tool

# 去除 ID3TAG 工具
ID3TAG_EXE=id3v2
#####################################


############ 裁剪静音功能配置 ############
# 静音检测阈值, 以dB为单位
MUTE_THRESHOLD=-50dB

# 裁剪静音时, 音频开始至少保留的静音时长, 秒为单位
MUTE_SAVE_PREFIX_TIME=0.01

# 裁剪静音时, 音频结尾至少保留的静音时长, 秒为单位
MUTE_SAVE_SUFFIX_TIME=0.01

# 裁剪静音后生成的文件的码率, 默认按照 24kbps 生成
# 可配置码率有: 16kbps, 24kbps, 40kbps, 56kbps, 64kbps, 112kbps, 128kbps ...
MUTE_AUDIO_RATE=24k
#####################################


############ 打包音频包 ############
# 音频二进制文件输出路径, 若不需要输出, 则赋值空 ''
PACK_TONE_OUT_BINARY=''
# PACK_TONE_OUT_BINARY=$CUR_DIR/../../res

# 音频头文件输出路径, 若不需要输出, 则赋值空 ''
PACK_TONE_OUT_HEADER=''
#####################################