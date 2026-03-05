#!/bin/bash

# 当前目录
CUR_DIR=`pwd`
# 配置文件目录
CONFIG_FILE=$CUR_DIR/config.sh
# 使能配置
source $CONFIG_FILE

if [ ! -f $FFMPEG_EXE ]; then
    echo "工具不存在: $FFMPEG_EXE"
    exit
fi

if [ ! -f $TONETOOL_EXE ]; then
    echo "工具不存在: $TONETOOL_EXE"
    exit
fi

WHICH_ID3_TOOL=`which $ID3TAG_EXE`
if [ -z $WHICH_ID3_TOOL ]; then
    echo "工具不存在: $ID3TAG_EXE, 请安装: sudo apt-get install id3v2"
    exit
fi

PROC_USER_ARG=''
PROC_IS_FILE=0
PROC_ONLY_PACK=0

function showHelp()
{
    echo "脚本使用示例:"
    echo "1) ./proc.sh -p ./ring            -p: 仅打包路径下的音频文件"
    echo "2) ./proc.sh -d ./ring            -d: 处理并打包路径下的音频文件"
    echo "3) ./proc.sh -f ./ring/test.mp3   -f: 处理指定的音频文件"
}

# 脚本传参解析
if [[ $# -le 1 ]]; then
    showHelp
    exit
fi

while [[ $# -gt 0 ]]; do
    case "$1" in
        -h|--help)
            showHelp
            exit
            ;;
        -f|--file)
            if [ -n "$2" ]; then
                if [ -f $2 ]; then
                    echo "处理文件: $2"
                    PROC_IS_FILE=1
                    PROC_USER_ARG=$2
                    break
                else
                    echo "文件无效: $2 "
                    exit
                fi
            else
                echo "请指定文件"
                exit
            fi
            ;;
        -d|--dir)
            if [ -n "$2" ]; then
                if [ -d $2 ]; then
                    echo "处理目录: $2"
                    PROC_USER_ARG=$2
                    break
                else
                    echo "处理目录无效: $2 "
                    exit
                fi
            else
                echo "请指定处理目录"
                exit
            fi
            ;;
        -p|--pack)
            if [ -n "$2" ]; then
                if [ -d $2 ]; then
                    echo "仅打包目录: $2"
                    PROC_ONLY_PACK=1
                    PROC_USER_ARG=$2
                    break
                else
                    echo "打包目录无效: $2 "
                    exit
                fi
            else
                echo "请指定打包目录"
                exit
            fi
            ;;
        *)
            echo "不支持的选项: $1"
            showHelp
            exit
            ;;
    esac
    shift
done

echo "OS ARCH: $OS_ARCH"
echo "Please Visit: https://ffbinaries.com/downloads to view all ffmpeg versions"

# 静音去除
FP0=silenceremove
# 在音频开头修剪音频
FPSP=start_periods=1
# 停止裁剪音频之前必须检测到的非静音时长
FPSD=start_duration=0
# 静音检测阈值
FPST=start_threshold=$MUTE_THRESHOLD
# 头部起始静音时间
FPSS1=start_silence=$MUTE_SAVE_PREFIX_TIME
# 尾部起始静音时间
FPSS2=start_silence=$MUTE_SAVE_SUFFIX_TIME
# 使用幅度值 amplitude 计算
FPD=detection=peak
# 反转
FPAV=areverse

# FFMPEG 参数
FFMPEG_PARAM=${FP0}=${FPSP}:${FPSD}:${FPST}:${FPSS1}:${FPD},${FPAV},${FP0}=${FPSP}:${FPSD}:${FPST}:${FPSS2}:${FPD},${FPAV}

# 处理函数
function proc_audio()
{
    in_file=$1
    out_file=$2

    in_file_suffix=${in_file##*.}
    in_file_prefix=${in_file%%.*}
    out_file_prefix=${out_file%%.*}

    if [ $in_file_suffix == "pcm" ] || [ $in_file_suffix == "mp3" ]; then
        if [ $in_file_suffix == "pcm" ]; then
            mp3_file=$in_file_prefix.mp3

            if [ -f $mp3_file ]; then
                rm $mp3_file
            fi

            # 转换PCM为MP3, 只处理16K、单通道、16bit PCM音频
            echo "转换PCM为MP3: $in_file -> $mp3_file"
            $FFMPEG_EXE -f s16le -ar 16000 -ac 1 -i $in_file -c:a libmp3lame -b:a 96k -acodec mp3 $mp3_file

            in_file=$mp3_file
            out_file=$out_file_prefix.mp3
        fi

        echo "处理前后静音: $in_file"
        $FFMPEG_EXE -hide_banner -loglevel warning -i $in_file -filter_complex \
                    "$FFMPEG_PARAM" \
                    -ar 16000 -ac 1 -b:a $MUTE_AUDIO_RATE -acodec mp3 $out_file -y
        
        # 移动文件
        mv $out_file $in_file

        # 去除ID3v1 ID3v2 TAG
        $ID3TAG_EXE -D $in_file >/dev/null
    else
        echo "不支持的文件后缀: $in_file_suffix"
    fi
}

function pack_tone_binary()
{
    echo "打包音频, 目录: $1"

    if [ -f $1/tone.h ]; then
        rm -rf $1/tone.h
    fi

    if [ -f $1/tone.bin ]; then
        rm -rf $1/tone.bin
    fi

    $TONETOOL_EXE $1 > /dev/null

    if [ ! -z "$PACK_TONE_OUT_BINARY" ] && [ -d $PACK_TONE_OUT_BINARY ]; then
        mv $1/tone.bin $$PACK_TONE_OUT_BINARY
    else
        echo "音频包二进制文件: $1/tone.bin"
    fi

    if [ ! -z "$PACK_TONE_OUT_HEADER" ] && [ -d $PACK_TONE_OUT_HEADER ]; then
        mv $1/tone.h $$PACK_TONE_OUT_HEADER
    else
        echo "音频包头文件: $1/tone.h"
    fi
}

# 暂存文件夹
tmp_dir=$CUR_DIR/tmp

if [ -d $tmp_dir ]; then
    rm -rf $tmp_dir
fi

mkdir $tmp_dir

if [ $PROC_IS_FILE -eq 1 ]; then
    # 处理文件
    audio_file=`basename $PROC_USER_ARG`
    proc_audio $PROC_USER_ARG $tmp_dir/$audio_file
else
    # 去除路径后的反斜杠
    PROC_USER_ARG=$(echo "$PROC_USER_ARG" | sed 's/\/$//')

    if [ $PROC_ONLY_PACK -eq 0 ]; then
        # 处理目录
        for audio_file in `ls $PROC_USER_ARG`
        do
            proc_audio $PROC_USER_ARG/$audio_file $tmp_dir/$audio_file
        done
    fi

    pack_tone_binary $PROC_USER_ARG
fi

# 清除暂存文件夹
if [ -d $tmp_dir ]; then
    rm -rf $tmp_dir
fi