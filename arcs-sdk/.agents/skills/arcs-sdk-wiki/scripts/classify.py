#!/usr/bin/env python3
"""classify.py — 对可达源文件做重建前分类。

用法:
    python scripts/classify.py <docs_zh_dir> --doctrees-dir <doctrees_dir> --output <report.tsv>

输出:
    page    source    classification    reason

classification:
    - substantive: 实质性文档，应进入 summary/entity/concept 流程
    - index_skip: 纯导航索引页，应跳过 summary
    - overview: 索引页但带实质导语，可保留为轻量概览页
"""

from __future__ import annotations

import argparse
import importlib.util
from pathlib import Path


INDEX_FILENAMES = {"index.md", "index.rst", "index_zh.rst"}


def load_scope_module():
    scope_path = Path(__file__).with_name("scope.py")
    spec = importlib.util.spec_from_file_location("scope_module", scope_path)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
    return module


def resolve_source_path(docs_zh_dir: Path, page: str) -> Path:
    for suffix in (".md", ".rst"):
        candidate = docs_zh_dir / f"{page}{suffix}"
        if candidate.exists():
            return candidate
    raise FileNotFoundError(f"source page not found for {page}")


def _is_heading_underline(line: str) -> bool:
    stripped = line.strip()
    return stripped != "" and set(stripped) <= {"=", "-", "~", "^", "`", ":"}


def _looks_like_path(line: str) -> bool:
    stripped = line.strip()
    return "/" in stripped and " " not in stripped and not stripped.startswith("http")


def meaningful_paragraphs(text: str) -> list[str]:
    paragraphs = []
    current = []
    in_rst_toctree = False
    in_md_toctree = False

    for raw_line in text.splitlines():
        line = raw_line.rstrip()
        stripped = line.strip()

        if stripped.startswith("```{toctree}"):
            in_md_toctree = True
            continue
        if in_md_toctree and stripped == "```":
            in_md_toctree = False
            continue
        if in_md_toctree:
            continue

        if stripped.startswith(".. toctree::"):
            in_rst_toctree = True
            continue
        if in_rst_toctree:
            if not stripped:
                continue
            if line.startswith(" ") or stripped.startswith(":"):
                continue
            in_rst_toctree = False

        if not stripped:
            if current:
                paragraphs.append(" ".join(current))
                current = []
            continue
        if stripped.startswith(".. "):
            continue
        if stripped.startswith(":"):
            continue
        if stripped.startswith("#"):
            continue
        if _is_heading_underline(stripped):
            continue
        if _looks_like_path(stripped):
            continue

        current.append(stripped)

    if current:
        paragraphs.append(" ".join(current))

    return paragraphs


def classify_page(source_path: Path, page: str) -> dict[str, str]:
    text = source_path.read_text(errors="replace")
    paragraphs = meaningful_paragraphs(text)
    is_index = source_path.name in INDEX_FILENAMES
    has_toctree = ".. toctree::" in text or "```{toctree}" in text

    if has_toctree and len(paragraphs) < 3:
        classification = "index_skip"
        reason = "pure-toctree-index"
    elif is_index and len(paragraphs) >= 3:
        classification = "overview"
        reason = "index-with-substantive-intro"
    else:
        classification = "substantive"
        reason = "substantive-body"

    return {
        "page": page,
        "source": str(source_path),
        "classification": classification,
        "reason": reason,
    }


def classify_reachable_pages(docs_zh_dir: Path, reachable_pages: list[str]) -> list[dict[str, str]]:
    rows = []
    for page in sorted(reachable_pages):
        source_path = resolve_source_path(docs_zh_dir, page)
        row = classify_page(source_path, page)
        row["source"] = str(source_path.relative_to(docs_zh_dir))
        rows.append(row)
    return rows


def write_report(output_path: Path, rows: list[dict[str, str]]) -> None:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    lines = ["page\tsource\tclassification\treason"]
    for row in rows:
        lines.append(
            "\t".join(
                [row["page"], row["source"], row["classification"], row["reason"]]
            )
        )
    output_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def load_reachable_pages(doctrees_dir: Path, docs_zh_dir: Path) -> list[str]:
    scope = load_scope_module()
    pickle_path = doctrees_dir / "environment.pickle"
    if pickle_path.exists():
        try:
            toctree_includes = scope.load_toctree_includes(doctrees_dir)
            return sorted(scope.walk_reachable(toctree_includes))
        except Exception:
            pass
    return sorted(scope.fallback_scan(docs_zh_dir))


def main() -> None:
    parser = argparse.ArgumentParser(description="Classify reachable wiki source pages.")
    parser.add_argument("docs_zh_dir", type=Path, help="docs/zh source directory")
    parser.add_argument("--doctrees-dir", type=Path, required=True, help="docs/output/zh/doctrees directory")
    parser.add_argument("--output", type=Path, required=True, help="TSV report output path")
    args = parser.parse_args()

    rows = classify_reachable_pages(
        args.docs_zh_dir,
        load_reachable_pages(args.doctrees_dir, args.docs_zh_dir),
    )
    write_report(args.output, rows)
    print(f"Wrote {len(rows)} rows to {args.output}")


if __name__ == "__main__":
    main()
