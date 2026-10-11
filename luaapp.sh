#!/usr/bin/env bash
# luaapp.sh - 把 Lua 应用打包进固件的 luaapp 分区（ROMFS 镜像）
#
#   ./luaapp.sh [脚本路径]     # 默认 tests/miniapp/21-douban.lua
#
# 产物：res/arcs-mini/luaapp.bin（partition_table.json 的 luaapp 分区，
# 0x580000）。固件里 CONFIG_MINIAPP_BUILTIN_APP=y 会在开机 15s 后挂载
# 并安装 /radio.lua 为 local:radio 自动运行（adb push 同名可热替换）。
# 烧录随整包（lpk/hex tag）：bash adb_download.sh -S res/arcs-mini
set -euo pipefail

SRC="${1:-tests/miniapp/21-douban.lua}"
ROOT="$(cd "$(dirname "$0")" && pwd)"
OUT="$ROOT/res/arcs-mini/luaapp.bin"
ENTRY_NAME="radio.lua"

[ -f "$ROOT/$SRC" ] || { echo "脚本不存在: $SRC" >&2; exit 1; }

STAGE="$ROOT/tmp/luaapp_stage"
rm -rf "$STAGE"
mkdir -p "$STAGE"
cp "$ROOT/$SRC" "$STAGE/$ENTRY_NAME"

python3 "$ROOT/tools/romfs.py" pack -i "$STAGE" -o "$OUT" -n luaapp
rm -rf "$STAGE"

echo "打包完成: $OUT ($(stat -c%s "$OUT") bytes, 条目 /$ENTRY_NAME)"
echo "烧录随整包: bash adb_download.sh -S res/arcs-mini   # 或只烧 luaapp 分区"
