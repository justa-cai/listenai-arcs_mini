#!/bin/bash
# 小智云端配置和测试脚本

echo "====================================="
echo "小智云端 (XiaoZhi Cloud) 配置脚本"
echo "====================================="

# 检查设备连接
echo ""
echo "1. 检查 ADB 设备连接..."
if ! adb devices | grep -q "device$"; then
    echo "错误: 未检测到 ADB 设备，请先连接设备"
    exit 1
fi
echo "   ADB 设备已连接"

# 设置默认配置
echo ""
echo "2. 设置默认配置..."
adb shell "lisa_kv set xz.url 'wss://api.tenclass.net/xiaozhi/v1/'"
adb shell "lisa_kv set xz.token 'default_token'"
adb shell "lisa_kv set xz.device_id 'arcs_mini'"
adb shell "lisa_kv set xz.client_id 'default'"
echo "   默认配置已设置"

# 读取配置
echo ""
echo "3. 当前配置:"
echo "   URL: $(adb shell 'lisa_kv get xz.url')"
echo "   Token: $(adb shell 'lisa_kv get xz.token')"
echo "   Device ID: $(adb shell 'lisa_kv get xz.device_id')"
echo "   Client ID: $(adb shell 'lisa_kv get xz.client_id')"

# 设置自定义配置（如果提供）
if [ -n "$1" ]; then
    echo ""
    echo "4. 设置自定义 Token: $1"
    adb shell "lisa_kv set xz.token '$1'"
fi

if [ -n "$2" ]; then
    echo "   设置自定义 URL: $2"
    adb shell "lisa_kv set xz.url '$2'"
fi

# 重启设备以应用配置
echo ""
echo "5. 重启设备以应用配置..."
read -p "是否立即重启设备? (y/n) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    adb shell reboot
    echo "   设备正在重启..."
    echo "   重启完成后，请查看串口日志确认连接状态"
else
    echo "   请手动重启设备以应用配置"
fi

echo ""
echo "====================================="
echo "配置完成！"
echo "====================================="
echo ""
echo "日志查看命令:"
echo "  adb shell 'log set_tag_level xz_* 6'"
echo ""
echo "验证连接:"
echo "  查看日志中是否出现: [xz_cloud] Connected to xiaozhi cloud"
echo ""
echo "测试语音交互:"
echo "  1. 等待 WiFi 连接成功"
echo "  2. 点击设备唤醒按钮或说唤醒词"
echo "  3. 查看日志中的 STT/LLM/TTS 响应"
echo ""
echo "修改配置:"
echo "  ./xz_config.sh <token> [url]"
echo ""
