#!/usr/bin/env bash
# 作用: 调用 QA records API 并运行 analyze_qa.py 生成 QA 反馈报告。
# 用法: ./scripts/export_and_analyze.sh <wiki_version_dir> [qa_records_api_url]
# 示例: ./scripts/export_and_analyze.sh wiki/v0.1.0/

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

if [ $# -lt 1 ]; then
    echo "Usage: $0 <wiki_version_dir> [qa_records_api_url]" >&2
    echo "Example: $0 wiki/v0.1.0/" >&2
    exit 1
fi

WIKI_DIR="$1"
API_URL="${2:-${QA_RECORDS_API_URL:-https://staging-api-docs2.listenai.com/api/v1/qa-records}}"
VERSION="${ARCS_WIKI_VERSION:-$(basename "$WIKI_DIR")}"
ARGS=(
    "$WIKI_DIR"
    "--api-url" "$API_URL"
    "--version" "$VERSION"
)

if [ -n "${QA_START_DATE:-}" ]; then
    ARGS+=("--start-date" "$QA_START_DATE")
fi
if [ -n "${QA_END_DATE:-}" ]; then
    ARGS+=("--end-date" "$QA_END_DATE")
fi
if [ -n "${QA_SINCE_DAYS:-}" ]; then
    ARGS+=("--since-days" "$QA_SINCE_DAYS")
fi

echo "=== Step 1: Analyzing QA feedback ==="
python3 "$SCRIPT_DIR/analyze_qa.py" "${ARGS[@]}"

echo ""
echo "=== Done ==="
echo "Reports are in: $WIKI_DIR/reports/"
