#!/bin/bash
# build_materials.sh — 就地构建一个版本的 wiki 原料。
# 前提:运行环境已有该版本的 SDK 源码 checkout(由发版流水线提供)。
# 用法: build_materials.sh <version> <sdk_docs_dir> <materials_root>
#   <version>        如 v0.1.7
#   <sdk_docs_dir>   SDK 仓库内的 docs 目录(含 Makefile、zh/)
#   <materials_root> Doc-Assistant 的 AI_assistant/data/materials 目录
# 输出: <materials_root>/<version>/{zh,output}
set -euo pipefail

if [ "$#" -ne 3 ]; then
    echo "用法: $0 <version> <sdk_docs_dir> <materials_root>" >&2
    exit 1
fi

VERSION="$1"
DOCS_DIR="$2"
MATERIALS_ROOT="$3"

case "$VERSION" in
    ""|[!A-Za-z0-9]*|*[^A-Za-z0-9._-]*)
        echo "❌ 非法版本号: $VERSION" >&2
        echo "版本号只能是单个路径组件,且只能包含字母、数字、点、下划线和连字符。" >&2
        exit 1
        ;;
esac

TARGET_DIR="$MATERIALS_ROOT/$VERSION"

if [ ! -d "$DOCS_DIR" ]; then
    echo "❌ 未找到 docs 目录: $DOCS_DIR" >&2
    exit 1
fi

echo "🛠️  就地构建文档: make -C $DOCS_DIR zh"
make -C "$DOCS_DIR" zh

if [ ! -d "$DOCS_DIR/output" ] || [ ! -d "$DOCS_DIR/zh" ]; then
    echo "❌ 构建后缺少 output/ 或 zh/" >&2
    exit 1
fi

echo "📁 收进原料目录: $TARGET_DIR"
mkdir -p "$TARGET_DIR"
rm -rf "$TARGET_DIR/output" "$TARGET_DIR/zh"
cp -a "$DOCS_DIR/output" "$TARGET_DIR/output"
cp -a "$DOCS_DIR/zh" "$TARGET_DIR/zh"

echo "✅ 原料就绪: $TARGET_DIR"
