#!/usr/bin/env python3
"""analyze_qa.py — 从 QA records API 提取反馈信号，输出 wiki 维护证据包。

用法:
    python scripts/analyze_qa.py <wiki_dir> --api-url <qa-records-api>
    python scripts/analyze_qa.py wiki/v0.1.7/ --version v0.1.7 --since-days 30

默认 API:
    https://staging-api-docs2.listenai.com/api/v1/qa-records

输出:
    - reports/qa-feedback-YYYY-MM-DD.md      人读报告
    - reports/qa-signals-YYYY-MM-DD.jsonl    AI maintenance 输入
    - reports/qa-records-YYYY-MM-DD.json     原始 API 响应与查询参数（完整保留）
"""

from __future__ import annotations

import argparse
import datetime as dt
import json
import os
import re
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from collections import Counter
from pathlib import Path
from typing import Any


DEFAULT_API_URL = "https://staging-api-docs2.listenai.com/api/v1/qa-records"
RATING_VALUES = {"good", "medium", "poor"}


def _today() -> dt.date:
    return dt.date.today()


def _iso_date(value: str) -> str:
    try:
        return dt.date.fromisoformat(value).isoformat()
    except ValueError as exc:
        raise argparse.ArgumentTypeError(f"invalid date '{value}', expected YYYY-MM-DD") from exc


def build_query(args: argparse.Namespace) -> dict[str, str]:
    query: dict[str, str] = {}
    if args.version:
        query["version"] = args.version
    if args.rating:
        query["rating"] = args.rating

    start_date = args.start_date
    end_date = args.end_date
    if args.since_days is not None:
        end = _today()
        start = end - dt.timedelta(days=args.since_days)
        start_date = start_date or start.isoformat()
        end_date = end_date or end.isoformat()

    if start_date:
        query["start_date"] = start_date
    if end_date:
        query["end_date"] = end_date
    return query


def fetch_records(api_url: str, query: dict[str, str], timeout: float) -> dict[str, Any]:
    url = api_url
    if query:
        url = f"{api_url}?{urllib.parse.urlencode(query)}"
    req = urllib.request.Request(url, headers={"Accept": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=timeout) as response:
            raw = response.read().decode("utf-8", errors="replace")
    except urllib.error.HTTPError as exc:
        detail = exc.read().decode("utf-8", errors="replace")[:500]
        raise RuntimeError(f"QA records API failed with HTTP {exc.code}: {detail}") from exc
    except urllib.error.URLError as exc:
        raise RuntimeError(f"QA records API request failed: {exc}") from exc

    try:
        data = json.loads(raw)
    except json.JSONDecodeError as exc:
        raise RuntimeError(f"QA records API returned invalid JSON: {exc}") from exc
    if not isinstance(data, dict) or not isinstance(data.get("records"), list):
        raise RuntimeError("QA records API response must contain a records array")
    return data


def normalize_turns(value: Any) -> list[dict[str, str]]:
    if not isinstance(value, list):
        return []
    turns = []
    for item in value:
        if not isinstance(item, dict):
            continue
        turns.append({
            "q": str(item.get("q") or ""),
            "a": str(item.get("a") or ""),
        })
    return turns


def normalize_records(records: list[dict[str, Any]]) -> list[dict[str, Any]]:
    normalized = []
    for row in records:
        turns = normalize_turns(row.get("qa_record"))
        normalized.append({
            "session_id": str(row.get("session_id") or ""),
            "user_id": str(row.get("user_id") or ""),
            "version": str(row.get("version") or ""),
            "doc_related": str(row.get("doc_related") or ""),
            "wiki_searched": str(row.get("wiki_searched") or ""),
            "source_searched": str(row.get("source_searched") or ""),
            "rating": str(row.get("rating") or ""),
            "turns": turns,
            "created_at": str(row.get("created_at") or ""),
            "updated_at": str(row.get("updated_at") or ""),
        })
    return normalized


def _first_question(record: dict[str, Any]) -> str:
    turns = record.get("turns") or []
    if turns:
        return str(turns[0].get("q") or "")
    return ""


def _all_questions(record: dict[str, Any]) -> list[str]:
    return [str(t.get("q") or "") for t in record.get("turns", []) if t.get("q")]


def _all_answer_previews(record: dict[str, Any], max_chars: int = 500) -> list[str]:
    return [
        str(t.get("a") or "")[:max_chars]
        for t in record.get("turns", [])
        if t.get("a")
    ]


def _first_answer(record: dict[str, Any]) -> str:
    turns = record.get("turns") or []
    if turns:
        return str(turns[0].get("a") or "")
    return ""


def is_placeholder_record(record: dict[str, Any]) -> bool:
    """Drop obvious OpenAPI/example rows from analysis while preserving them in raw JSON."""
    return (
        record["version"].strip().lower() == "string"
        and _first_question(record).strip().lower() == "string"
    )


def _backend_mode(record: dict[str, Any]) -> str:
    doc_related = record["doc_related"]
    wiki_searched = record["wiki_searched"]
    source_searched = record["source_searched"]
    if doc_related == "no":
        return "out_of_scope"
    if wiki_searched == "no":
        return "no_answer"
    if source_searched == "yes":
        return "html_llm"
    return "wiki_llm"


def _record_signals(record: dict[str, Any]) -> list[str]:
    signals = []
    if record["rating"] == "poor":
        signals.append("poor-rated")
    mode = _backend_mode(record)
    if mode == "no_answer":
        signals.append("no-answer")
    elif mode == "html_llm":
        signals.append("html-fallback")
    if not signals:
        signals.append(mode)
    return signals


def find_poor_rated(records: list[dict[str, Any]]) -> list[dict[str, Any]]:
    return [r for r in records if r["rating"] == "poor"]


def find_no_answer(records: list[dict[str, Any]]) -> list[dict[str, Any]]:
    return [r for r in records if _backend_mode(r) == "no_answer"]


def find_html_fallback(records: list[dict[str, Any]]) -> list[dict[str, Any]]:
    return [r for r in records if _backend_mode(r) == "html_llm"]


def _normalize_question(question: str) -> str:
    question = question.strip().lower()
    question = re.sub(r"[？?。.！!，,、\s]+", " ", question)
    return question


def _question_features(question: str) -> set[str]:
    normalized = _normalize_question(question)
    compact = normalized.replace(" ", "")
    if re.search(r"[\u4e00-\u9fff]", compact):
        if len(compact) <= 2:
            return {compact} if compact else set()
        return {compact[i:i + 2] for i in range(len(compact) - 1)}
    return {word for word in normalized.split() if len(word) >= 2}


def _questions_similar(left: str, right: str) -> bool:
    if _normalize_question(left) == _normalize_question(right):
        return True
    left_features = _question_features(left)
    right_features = _question_features(right)
    if not left_features or not right_features:
        return False
    overlap = len(left_features & right_features) / min(len(left_features), len(right_features))
    return overlap >= 0.45


def find_high_frequency(
    records: list[dict[str, Any]],
    threshold: int = 2,
) -> list[tuple[str, int, list[str]]]:
    questions: list[str] = []
    for record in records:
        for question in _all_questions(record):
            if len(question) >= 4:
                questions.append(question)

    clusters: list[list[str]] = []
    for question in questions:
        matched = False
        for cluster in clusters:
            if any(_questions_similar(question, existing) for existing in cluster[:3]):
                cluster.append(question)
                matched = True
                break
        if not matched:
            clusters.append([question])

    results: list[tuple[str, int, list[str]]] = []
    for cluster in clusters:
        if len(cluster) < threshold:
            continue
        examples = list(dict.fromkeys(cluster))[:5]
        results.append((examples[0], len(cluster), examples))
    results.sort(key=lambda item: item[1], reverse=True)
    return results


def write_jsonl(path: Path, records: list[dict[str, Any]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as handle:
        for record in records:
            signal = {
                "session_id": record["session_id"],
                "version": record["version"],
                "rating": record["rating"],
                "backend_mode": _backend_mode(record),
                "signals": _record_signals(record),
                "question": _first_question(record),
                "questions": _all_questions(record),
                "answer_preview": _first_answer(record)[:500],
                "answer_previews": _all_answer_previews(record),
                "turn_count": len(record.get("turns", [])),
                "doc_related": record["doc_related"],
                "wiki_searched": record["wiki_searched"],
                "source_searched": record["source_searched"],
                "created_at": record["created_at"],
                "updated_at": record["updated_at"],
            }
            handle.write(json.dumps(signal, ensure_ascii=False) + "\n")


def write_raw_json(
    path: Path,
    api_url: str,
    query: dict[str, str],
    response: dict[str, Any],
) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    payload = {
        "api_url": api_url,
        "query": query,
        "fetched_at": dt.datetime.now(dt.timezone.utc).isoformat(),
        "response": response,
    }
    path.write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")


def generate_report(
    api_url: str,
    query: dict[str, str],
    wiki_dir: str,
    records: list[dict[str, Any]],
    raw_record_count: int,
    skipped_records: list[dict[str, Any]],
    jsonl_path: Path,
    raw_json_path: Path,
) -> str:
    lines = []
    today = time.strftime("%Y-%m-%d")
    total = len(records)
    mode_stats = Counter(_backend_mode(record) for record in records)
    version_stats = Counter(record["version"] for record in records)
    rating_stats = Counter(record["rating"] for record in records)

    lines.append(f"# QA 反馈报告 — {today}")
    lines.append("")
    lines.append(f"数据源 API: `{api_url}`")
    lines.append(f"查询参数: `{json.dumps(query, ensure_ascii=False)}`")
    lines.append(f"Wiki 目录: `{wiki_dir}`")
    lines.append(f"API 原始会话数: {raw_record_count}")
    lines.append(f"分析有效会话数: {total}")
    if skipped_records:
        lines.append(f"已排除占位/无效会话数: {len(skipped_records)}")
    lines.append(f"结构化信号: `{jsonl_path}`")
    lines.append(f"原始 API 记录: `{raw_json_path}`")
    lines.append("")

    lines.append("## 总览")
    lines.append("")
    lines.append("| 指标 | 数量 | 占比 |")
    lines.append("|------|------|------|")
    for mode in ["wiki_llm", "html_llm", "no_answer", "out_of_scope"]:
        count = mode_stats.get(mode, 0)
        pct = f"{count / total * 100:.0f}%" if total else "0%"
        lines.append(f"| {mode} | {count} | {pct} |")
    lines.append("")

    lines.append("| 评分 | 数量 |")
    lines.append("|------|------|")
    for rating in ["good", "medium", "poor", ""]:
        label = rating or "(empty)"
        lines.append(f"| {label} | {rating_stats.get(rating, 0)} |")
    lines.append("")

    if len(version_stats) > 1:
        lines.append("| 版本 | 会话数 |")
        lines.append("|------|--------|")
        for version, count in sorted(version_stats.items()):
            lines.append(f"| {version} | {count} |")
        lines.append("")

    poor = find_poor_rated(records)
    lines.append("## P0 差评会话")
    lines.append("")
    if poor:
        for record in poor[:20]:
            lines.append(f"### Session `{record['session_id'][:12]}`")
            lines.append(f"- 版本: `{record['version']}`")
            lines.append(f"- 模式: `{_backend_mode(record)}`")
            lines.append(f"- 时间: `{record['created_at']}`")
            lines.append(f"- 问题: {_first_question(record)}")
            answer = _first_answer(record)
            if answer:
                lines.append(f"- 回答摘要: {answer[:240]}{'...' if len(answer) > 240 else ''}")
            lines.append("- 建议动作: 回到相关 wiki 页面和原料检查回答缺失、错误或过薄内容。")
            lines.append("")
    else:
        lines.append("无差评会话。")
        lines.append("")

    no_answer = find_no_answer(records)
    lines.append("## P1 无法回答的问题")
    lines.append("")
    if no_answer:
        for record in no_answer[:50]:
            lines.append(f"- [{record['version']}] {_first_question(record)}")
        if len(no_answer) > 50:
            lines.append(f"- ...（共 {len(no_answer)} 个，仅显示前 50）")
        lines.append("")
        lines.append("建议动作: 分析这些问题是否应由当前 wiki 覆盖；需要时补充 entity/summary/guide/scenario。")
        lines.append("")
    else:
        lines.append("无 no_answer 记录。")
        lines.append("")

    html_fallback = find_html_fallback(records)
    lines.append("## P2 HTML 回源会话")
    lines.append("")
    if html_fallback:
        for record in html_fallback[:50]:
            lines.append(f"- [{record['version']}] {_first_question(record)}")
        if len(html_fallback) > 50:
            lines.append(f"- ...（共 {len(html_fallback)} 个，仅显示前 50）")
        lines.append("")
        lines.append("建议动作: wiki 命中后仍需回源，优先补强对应主题 summary/entity 的细节密度。")
        lines.append("")
    else:
        lines.append("无 HTML 回源记录。")
        lines.append("")

    high_freq = find_high_frequency(records, threshold=2)
    lines.append("## P3 高频问题")
    lines.append("")
    if high_freq:
        for representative, count, examples in high_freq[:20]:
            lines.append(f"### {representative[:80]} ({count} 次)")
            for example in examples[:5]:
                lines.append(f"- {example}")
            lines.append("")
    else:
        lines.append("未发现高频重复问题。")
        lines.append("")

    lines.append("## 维护输入")
    lines.append("")
    lines.append("后续 AI maintenance job 应优先读取：")
    lines.append(f"- `{jsonl_path}`")
    lines.append(f"- `{raw_json_path}`")
    lines.append("并结合当前 wiki 与 docs 原料修复 P0/P1/P2/P3 项。")
    lines.append("")
    return "\n".join(lines)


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Analyze QA records from API for wiki maintenance.")
    parser.add_argument("wiki_dir", type=Path, help="wiki version directory")
    parser.add_argument("--api-url", default=os.environ.get("QA_RECORDS_API_URL", DEFAULT_API_URL))
    parser.add_argument("--version", default=os.environ.get("ARCS_WIKI_VERSION"))
    parser.add_argument("--rating", choices=sorted(RATING_VALUES), default=None)
    parser.add_argument("--start-date", type=_iso_date, default=os.environ.get("QA_START_DATE"))
    parser.add_argument("--end-date", type=_iso_date, default=os.environ.get("QA_END_DATE"))
    parser.add_argument("--since-days", type=int, default=None)
    parser.add_argument("--timeout", type=float, default=float(os.environ.get("QA_RECORDS_API_TIMEOUT", "30")))
    parser.add_argument("--output", type=Path, default=None, help="markdown report path")
    parser.add_argument("--jsonl", type=Path, default=None, help="normalized signal JSONL path")
    parser.add_argument("--raw-json", type=Path, default=None, help="raw API response JSON path")
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_arg_parser()
    args = parser.parse_args(argv)

    if not args.wiki_dir.is_dir():
        print(f"Error: {args.wiki_dir} is not a directory", file=sys.stderr)
        return 1
    if args.since_days is not None and args.since_days < 1:
        print("Error: --since-days must be >= 1", file=sys.stderr)
        return 1

    today = time.strftime("%Y-%m-%d")
    reports_dir = args.wiki_dir / "reports"
    report_path = args.output or reports_dir / f"qa-feedback-{today}.md"
    jsonl_path = args.jsonl or reports_dir / f"qa-signals-{today}.jsonl"
    raw_json_path = args.raw_json or reports_dir / f"qa-records-{today}.json"

    query = build_query(args)
    api_response = fetch_records(args.api_url, query, args.timeout)

    reports_dir.mkdir(parents=True, exist_ok=True)
    all_records = normalize_records(api_response["records"])
    skipped_records = [record for record in all_records if is_placeholder_record(record)]
    records = [record for record in all_records if not is_placeholder_record(record)]

    write_jsonl(jsonl_path, records)
    write_raw_json(raw_json_path, args.api_url, query, api_response)
    report = generate_report(
        args.api_url,
        query,
        str(args.wiki_dir),
        records,
        len(all_records),
        skipped_records,
        jsonl_path,
        raw_json_path,
    )
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(report, encoding="utf-8")

    print(
        f"Fetched {len(all_records)} QA records from {args.api_url}; "
        f"analyzing {len(records)}, skipped {len(skipped_records)}",
        file=sys.stderr,
    )
    print(f"Report written to {report_path}", file=sys.stderr)
    print(f"Signals written to {jsonl_path}", file=sys.stderr)
    print(f"Raw records written to {raw_json_path}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
