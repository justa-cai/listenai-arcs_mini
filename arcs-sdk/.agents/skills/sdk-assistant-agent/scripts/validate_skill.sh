#!/bin/bash
# SDK Assistant Agent 校验脚本
# 用法: bash .agents/skills/sdk-assistant-agent/scripts/validate_skill.sh

set -euo pipefail
shopt -s nullglob

SKILL_DIR="$(cd "$(dirname "$0")/.." && pwd)"
ERRORS=0
WARNINGS=0

increment_error() {
    ERRORS=$((ERRORS + 1))
}

increment_warning() {
    WARNINGS=$((WARNINGS + 1))
}

print_section() {
    printf '%s\n' "--- $1 ---"
}

verify_experience_entry() {
    local file="$1"
    local title="$2"
    local has_scene="$3"
    local has_date="$4"

    if (( has_scene == 0 )); then
        printf "ERROR: %s 条目 \"%s\" 缺少'场景'字段\n" "$file" "$title"
        increment_error
    fi
    if (( has_date == 0 )); then
        printf "ERROR: %s 条目 \"%s\" 缺少'日期'字段\n" "$file" "$title"
        increment_error
    fi
}

check_index_registration() {
    print_section "检查 index.md 完整性"
    local reference

    for reference in "$SKILL_DIR"/references/*.md; do
        local basename
        basename=$(basename "$reference")
        [ "$basename" = "index.md" ] && continue
        if ! grep -qF "$basename" "$SKILL_DIR/references/index.md" 2>/dev/null; then
            printf 'ERROR: %s 未在 index.md 中注册\n' "$basename"
            increment_error
        fi
    done
}

check_header_declarations() {
    print_section "检查文件头类型声明"
    local files=(
        "$SKILL_DIR"/references/*.md
        "$SKILL_DIR"/references/experience/*.md
        "$SKILL_DIR"/references/knowledge/*.md
    )
    local file

    for file in "${files[@]}"; do
        [ -f "$file" ] || continue
        local basename
        basename=$(basename "$file")
        [ "$basename" = ".gitkeep" ] && continue
        if ! grep -q '^<!-- type:' "$file" 2>/dev/null; then
            printf 'WARNING: %s 缺少类型声明头\n' "$file"
            increment_warning
        fi
    done
}

check_experience_entry_format() {
    print_section "检查 experience 条目格式"
    local reference

    for reference in "$SKILL_DIR"/references/experience/*.md; do
        [ -f "$reference" ] || continue
        local basename
        basename=$(basename "$reference")
        [ "$basename" = ".gitkeep" ] && continue

        local entry_title=""
        local has_scene=0
        local has_date=0
        local entry_started=0

        while IFS= read -r line; do
            if [[ $line =~ ^##[[:space:]] ]]; then
                if (( entry_started )); then
                    verify_experience_entry "$basename" "$entry_title" "$has_scene" "$has_date"
                fi
                entry_started=1
                entry_title="${line#\#\# }"
                has_scene=0
                has_date=0
                continue
            fi
            [[ $line == "- 场景："* ]] && has_scene=1
            [[ $line == "- 日期："* ]] && has_date=1
        done < "$reference"

        if (( entry_started )); then
            verify_experience_entry "$basename" "$entry_title" "$has_scene" "$has_date"
        fi
    done
}

check_version_markers() {
    print_section "检查版本标记格式"
    local reference

    for reference in "$SKILL_DIR"/references/experience/*.md; do
        [ -f "$reference" ] || continue
        local basename
        basename=$(basename "$reference")
        [ "$basename" = ".gitkeep" ] && continue

        while IFS=$'\t' read -r lineno content; do
            [[ $content == "- 版本："* ]] || continue
            local value
            value=${content#- 版本：}
            value=${value#${value%%[![:space:]]*}}
            if ! [[ $value =~ (<=|>=|=)[[:space:]]*[0-9]+\.[0-9]+ ]]; then
                printf 'WARNING: %s:%s 版本标记格式不规范: %s\n' "$basename" "$lineno" "$value"
                increment_warning
            fi
        done < <(nl -ba -s $'\t' "$reference")
    done
}

print_summary() {
    printf '\n=== 校验完成: %s 个错误, %s 个警告 ===\n' "$ERRORS" "$WARNINGS"
    if (( ERRORS == 0 )); then
        echo "PASS"
    else
        echo "FAIL"
    fi
}

main() {
    echo "=== SDK Assistant Agent 校验 ==="
    echo "目录: $SKILL_DIR"
    echo ""

    check_index_registration
    check_header_declarations
    check_experience_entry_format
    check_version_markers
    print_summary

    exit $ERRORS
}

main "$@"
