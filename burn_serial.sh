#!/bin/bash
# burn_serial.sh — 串口烧录脚本（需手动执行，sudo 可能要输密码）
#
# 用法:
#   sudo ./burn_serial.sh           # 烧 LNN demo 三件套（AP+CP+模型）
#   sudo ./burn_serial.sh restore   # 恢复产品固件分区（ap+wake_word+emoji）
#   sudo ./burn_serial.sh retry 5   # 失败自动重试次数（默认 3）
#
# 烧录模式进入方法（cskburn 会自动拉 DTR）:
#   若一直 "Waiting for device..."，请在脚本运行期间给设备断电重新上电，
#   DTR 低电平期间上电/复位即可进入 ROM 烧录模式。
#
# 注意: 0x0 的 boot 分区永远不烧（烧不死的 boot），日常更新请优先用 ADB。

# ---- 释放串口：杀 picocom（root 进程需 sudo）+ 清理残留锁文件 ---------------
sudo killall picocom 2>/dev/null || true
rm -f /var/lock/LCK..ttyACM0 /var/lock/LCK..ttyACM 2>/dev/null || true
sleep 0.5

set -u

# ---- 配置 ----------------------------------------------------------------
DEV="${SERIAL_DEV:-/dev/ttyACM0}"
BAUD="${SERIAL_BAUD:-921600}"
RETRY="${RETRY:-3}"
MODE="${1:-demo}"

ROOT="$(cd "$(dirname "$0")" && pwd)"
DEMO_AP="$ROOT/build-lnn/remote/thinker_resnet18_real_ap.bin"
DEMO_CP="$ROOT/build-lnn/thinker_resnet18_real_cp.bin"
DEMO_MODEL="$ROOT/arcs-sdk/labs/lnn/thinker_resnet18/resources/resnet18_arcs.bin"
RES="$ROOT/res/arcs-mini"

GREEN='\033[0;32m'; YELLOW='\033[1;33m'; RED='\033[0;31m'; NC='\033[0m'

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}  串口烧录 (cskburn → $DEV @$BAUD)${NC}"
echo -e "${GREEN}========================================${NC}"

# ---- 停 ModemManager（会抢串口导致打开失败）-------------------------------
MM_WAS_ACTIVE=0
if systemctl is-active --quiet ModemManager 2>/dev/null; then
    MM_WAS_ACTIVE=1
    echo -e "${YELLOW}停止 ModemManager（烧完自动恢复）...${NC}"
    systemctl stop ModemManager
fi
trap '[ "$MM_WAS_ACTIVE" = 1 ] && systemctl start ModemManager 2>/dev/null' EXIT

# ---- 组装分区表 ------------------------------------------------------------
case "$MODE" in
demo)
    PARTS=(0x40000 "$DEMO_AP" 0x600000 "$DEMO_CP" 0x200000 "$DEMO_MODEL")
    LABEL="LNN demo (AP@0x40000 CP@0x600000 模型@0x200000)"
    ;;
restore)
    PARTS=(0x40000 "$RES/ap.bin" 0x200000 "$RES/wake_word.bin" 0x380000 "$RES/emoji.bin")
    LABEL="产品固件恢复 (ap+wake_word+emoji)"
    ;;
*)
    echo -e "${RED}未知模式: $MODE（可用: demo | restore）${NC}"
    exit 1
    ;;
esac

echo -e "模式: ${GREEN}${LABEL}${NC}"
for i in $(seq 0 2 $(( ${#PARTS[@]} - 1 ))); do
    f="${PARTS[$((i+1))]}"
    if [ ! -f "$f" ]; then
        echo -e "${RED}缺少文件: $f${NC}"
        exit 1
    fi
    echo -e "  $(basename "$f") → ${PARTS[$i]} ($(du -h "$f" | cut -f1))"
done

# ---- 烧录（带重试）---------------------------------------------------------
attempt=1
while [ "$attempt" -le "$RETRY" ]; do
    echo ""
    echo -e "${YELLOW}[尝试 $attempt/$RETRY] 开始烧录...${NC}"
    if ./tools/cskburn/cskburn -C arcs -b "$BAUD" -s "$DEV" "${PARTS[@]}"; then
        echo ""
        echo -e "${GREEN}烧录成功！设备已自动复位。${NC}"
        [ "$MODE" = "restore" ] && echo -e "产品 ap 已恢复，可用 adb_download.sh 补烧 app 分区。"
        exit 0
    fi
    attempt=$((attempt + 1))
    if [ "$attempt" -le "$RETRY" ]; then
        echo -e "${YELLOW}未握手成功。请现在给设备断电重新上电（或按复位），3 秒后重试...${NC}"
        sleep 3
    fi
done

echo ""
echo -e "${RED}烧录失败。排查建议：${NC}"
echo -e "  1. 确认没有其他程序占用 $DEV（picocom 需先退出）"
echo -e "  2. 烧录等待期间给设备断电重新上电（DTR 拉低时上电才进烧录模式）"
echo -e "  3. 降低波特率重试: sudo SERIAL_BAUD=1500000 ./burn_serial.sh"
exit 1
