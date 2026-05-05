#!/bin/bash
# wifi_pm MQTT Broker 配置与测试脚本
# 用法:
#   ./mqtt_test.sh setup    - 配置并启动 Mosquitto Broker
#   ./mqtt_test.sh sub      - 订阅设备发布的消息
#   ./mqtt_test.sh pub      - 向设备发送测试命令
#   ./mqtt_test.sh status   - 查看 Broker 状态
#   ./mqtt_test.sh stop     - 停止 Broker

set -e

BROKER_IP=$(ip -4 addr show | grep -oP 'inet \K[0-9.]+' | grep -v '127.0.0.1' | head -1)
BROKER_PORT=1883
TOPIC_PUB="arcs/test/pub"
TOPIC_SUB="arcs/test/sub"
CONF_FILE="/etc/mosquitto/conf.d/wifi_pm.conf"

case "${1:-help}" in
setup)
    echo "=== 配置 Mosquitto MQTT Broker ==="
    echo ""

    # 写入配置：允许局域网匿名访问
    sudo tee "$CONF_FILE" > /dev/null << 'EOF'
# wifi_pm demo 配置
# 监听所有网卡的 1883 端口，允许局域网设备连接
listener 1883 0.0.0.0
allow_anonymous true
EOF

    echo "[OK] 配置已写入 $CONF_FILE"

    # 重启 mosquitto
    sudo systemctl restart mosquitto
    sleep 1

    if systemctl is-active --quiet mosquitto; then
        echo "[OK] Mosquitto 已启动"
        echo ""
        echo "=== 连接信息 ==="
        echo "  Broker IP:   $BROKER_IP"
        echo "  Broker Port: $BROKER_PORT"
        echo ""
        echo "请确保 wifi_pm 的 main.c 中配置为:"
        echo "  #define MQTT_BROKER_HOST   \"$BROKER_IP\""
        echo "  #define MQTT_BROKER_PORT   $BROKER_PORT"
    else
        echo "[FAIL] Mosquitto 启动失败"
        sudo systemctl status mosquitto
        exit 1
    fi
    ;;

sub)
    echo "=== 订阅设备发布的消息 (${TOPIC_PUB}) ==="
    echo "等待设备消息... (Ctrl+C 退出)"
    echo ""
    mosquitto_sub -h "$BROKER_IP" -p "$BROKER_PORT" -t "$TOPIC_PUB" -v
    ;;

pub)
    MSG="${2:-ping}"
    echo "=== 向设备发送命令: ${MSG} ==="
    mosquitto_pub -h "$BROKER_IP" -p "$BROKER_PORT" -t "$TOPIC_SUB" -m "$MSG"
    echo "[OK] 已发送到 ${TOPIC_SUB}: ${MSG}"
    echo ""
    echo "支持的命令:"
    echo "  ./mqtt_test.sh pub ping      - 心跳探测"
    echo "  ./mqtt_test.sh pub status    - 查询设备状态"
    echo "  ./mqtt_test.sh pub reboot    - 远程重启"
    ;;

status)
    echo "=== Mosquitto 状态 ==="
    systemctl is-active mosquitto && echo "状态: 运行中" || echo "状态: 已停止"
    echo "本机 IP: $BROKER_IP"
    echo "监听端口: $BROKER_PORT"
    echo ""
    echo "=== 测试连接 ==="
    if mosquitto_pub -h "$BROKER_IP" -p "$BROKER_PORT" -t "test/ping" -m "test" 2>/dev/null; then
        echo "[OK] Broker 可连接"
    else
        echo "[FAIL] Broker 不可连接"
    fi
    ;;

stop)
    echo "=== 停止 Mosquitto ==="
    sudo systemctl stop mosquitto
    echo "[OK] 已停止"
    ;;

help|*)
    echo "wifi_pm MQTT 测试工具"
    echo ""
    echo "用法: $0 <command> [args]"
    echo ""
    echo "命令:"
    echo "  setup     配置并启动 Mosquitto Broker (需要 sudo)"
    echo "  sub       订阅设备发布的消息"
    echo "  pub [msg] 向设备发送命令 (默认: ping)"
    echo "  status    查看 Broker 状态"
    echo "  stop      停止 Broker"
    echo ""
    echo "测试流程:"
    echo "  1. $0 setup                 # 首次配置并启动"
    echo "  2. $0 sub                   # 终端1: 订阅设备消息"
    echo "  3. $0 pub ping              # 终端2: 向设备发命令"
    echo "  4. $0 pub status            # 查询设备状态"
    ;;
esac
