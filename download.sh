#!/bin/bash

RED='\033[0;31m'
GREEN='\033[0;32m'

adb_cmd=""
# 检查是否在WSL环境中
if uname -a | grep -qi "WSL"; then
    echo "检测到WSL环境${NC}"
    # 获取window系统下的 ADB 工具路径
    adb_win_path=$(powershell.exe -Command "(Get-Command adb).Source")  
    # 转化为wsl路径
    adb_cmd=$(echo "$adb_win_path" | tr -d '\r' | sed -e 's/\\/\//g' -e 's/^\(.\):/\/mnt\/\L\1/') 

else
    adb_cmd="adb"
fi


# 验证adb是否可用
if ! "$adb_cmd" version >/dev/null 2>&1; then
    echo -e "${RED}错误: ADB工具不可用, 请确保已安装 Android SDK Platform-Tools 并将其添加到环境变量中"
    exit 1
fi
echo "找到ADB工具: $adb_cmd"
"$adb_cmd" version


# 检查设备是否连接
DEVICE_ID="FFBBCCDDEE001122"
echo "检查 Arcs-mini 连接状态..."
if ! "$adb_cmd" devices | grep -q "$DEVICE_ID"; then
    echo -e "${RED}错误: Arcs-mini 未连接"
    exit 1
fi

echo "Arcs-mini 已连接，开始传输文件..."
# 定义文件推送命令的数组
declare -A PUSH_COMMANDS=(
    ["build/aiui.bin"]="/RAW/NAND/600000"
)
# 检查所有本地文件是否存在
echo "检查本地文件..."
for local_path in "${!PUSH_COMMANDS[@]}"; do
    if [ ! -f "$local_path" ]; then
        echo -e "${RED}错误: 本地文件不存在: $local_path"
        exit 1
    fi
done


# 进入recovery模式
echo "进入recovery模式..."
if ! "$adb_cmd" -s "$DEVICE_ID" shell recovery; then
    echo -e "${RED}错误: 进入recovery模式失败${NC}"
    exit 1
fi
# 等待设备进入recovery模式
echo "等待设备进入recovery模式..."
sleep 3


# 执行所有推送命令
NEW_DEVICE_ID=$("$adb_cmd" devices | grep "BOOT-" | awk '{print $1}')
for local_path in "${!PUSH_COMMANDS[@]}"; do
    remote_path="${PUSH_COMMANDS[$local_path]}"
    echo "正在推送: $local_path -> $remote_path"
    if "$adb_cmd" -s "$NEW_DEVICE_ID" push "$local_path" "$remote_path"; then
        echo "成功推送: $local_path"
    else
        echo -e "${RED}错误: 推送失败 $local_path"
        exit 1
    fi
done
echo "所有文件传输完成！"
# 重启设备
if ! "$adb_cmd" -s "$NEW_DEVICE_ID" shell reboot hard; then
    echo -e "${RED}错误: 重启命令失败${NC}"
    exit 1
fi

echo -e "${GREEN}烧录完成！设备正在重启...${NC}"