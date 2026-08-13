#!/usr/bin/env python3
"""diff.py — 比较两个版本的源文件列表，输出变化摘要。

用法:
    python scripts/diff.py <old_docs_zh> <new_docs_zh>
    python scripts/diff.py Docs/v0.1.4/docs/zh Docs/v0.1.5/docs/zh

输出:
    - added / deleted / modified / reorganized 文件列表
    - 统计摘要（stdout）
    - 可选 --json 输出机器可读格式

用于 wiki-update-agent 判断增量/全量模式。
"""

import argparse
import hashlib
import json
import sys
from pathlib import Path


def scan_files(root: Path) -> dict[str, Path]:
    """Return {relative_path: absolute_path} for all .md/.rst files."""
    files = {}
    for ext in ('*.md', '*.rst'):
        for p in root.rglob(ext):
            rel = str(p.relative_to(root))
            files[rel] = p
    return files


def file_hash(path: Path) -> str:
    return hashlib.md5(path.read_bytes()).hexdigest()


def _path_similarity(a: str, b: str) -> float:
    """Score 0-1 based on shared path components."""
    pa, pb = Path(a).parts, Path(b).parts
    shared = len(set(pa) & set(pb))
    total = max(len(set(pa) | set(pb)), 1)
    return shared / total


def _content_similarity(path_a: Path, path_b: Path) -> float:
    """Quick line-based Jaccard similarity."""
    try:
        lines_a = set(path_a.read_text(errors='replace').splitlines())
        lines_b = set(path_b.read_text(errors='replace').splitlines())
    except OSError:
        return 0.0
    if not lines_a and not lines_b:
        return 1.0
    union = len(lines_a | lines_b)
    if union == 0:
        return 1.0
    return len(lines_a & lines_b) / union


def detect_reorganizations(old_files: dict, new_files: dict,
                            added: set, deleted: set) -> tuple[list[dict], set]:
    """Detect files that moved (same basename + content similarity > 0.3)."""
    reorgs = []
    del_by_name: dict[str, list[str]] = {}
    for d in deleted:
        name = Path(d).name
        del_by_name.setdefault(name, []).append(d)

    matched_del = set()
    for a in sorted(added):
        name = Path(a).name
        candidates = del_by_name.get(name, [])
        if not candidates:
            continue
        scored = []
        for c in candidates:
            cs = _content_similarity(old_files[c], new_files[a])
            ps = _path_similarity(c, a)
            scored.append((cs * 0.7 + ps * 0.3, c, cs))
        scored.sort(reverse=True)
        best_score, best, best_cs = scored[0]
        if best_cs < 0.3:
            continue
        reorgs.append({'old': best, 'new': a, 'similarity': round(best_cs, 2)})
        matched_del.add(best)
        candidates.remove(best)

    return reorgs, matched_del


def diff_versions(old_root: Path, new_root: Path) -> dict:
    old_files = scan_files(old_root)
    new_files = scan_files(new_root)

    old_set = set(old_files)
    new_set = set(new_files)

    added = new_set - old_set
    deleted = old_set - new_set
    common = old_set & new_set

    modified = set()
    unchanged = set()
    for f in common:
        if file_hash(old_files[f]) != file_hash(new_files[f]):
            modified.add(f)
        else:
            unchanged.add(f)

    reorgs, matched_del = detect_reorganizations(old_files, new_files,
                                                  added, deleted)
    reorg_old = {r['old'] for r in reorgs}
    reorg_new = {r['new'] for r in reorgs}
    pure_added = added - reorg_new
    pure_deleted = deleted - reorg_old

    return {
        'old_total': len(old_files),
        'new_total': len(new_files),
        'added': sorted(pure_added),
        'deleted': sorted(pure_deleted),
        'modified': sorted(modified),
        'unchanged': len(unchanged),
        'reorganized': reorgs,
    }


def suggest_mode(result: dict) -> str:
    """Suggest full or incremental ingest based on diff stats."""
    total_changed = (len(result['added']) + len(result['deleted'])
                     + len(result['modified']) + len(result['reorganized']))
    ratio = total_changed / max(result['old_total'], 1)
    if result['old_total'] == 0:
        return 'full'
    if ratio > 0.4:
        return 'full'
    return 'incremental'


def print_report(result: dict):
    mode = suggest_mode(result)
    print(f"Old: {result['old_total']} files  →  New: {result['new_total']} files")
    print(f"Added: {len(result['added'])}  Deleted: {len(result['deleted'])}  "
          f"Modified: {len(result['modified'])}  Reorganized: {len(result['reorganized'])}  "
          f"Unchanged: {result['unchanged']}")
    print(f"Suggested mode: {mode}")
    print()

    if result['added']:
        print("--- Added ---")
        for f in result['added']:
            print(f"  + {f}")
        print()

    if result['deleted']:
        print("--- Deleted ---")
        for f in result['deleted']:
            print(f"  - {f}")
        print()

    if result['modified']:
        print("--- Modified ---")
        for f in result['modified']:
            print(f"  ~ {f}")
        print()

    if result['reorganized']:
        print("--- Reorganized ---")
        for r in result['reorganized']:
            print(f"  {r['old']}  →  {r['new']}")
        print()


def main():
    parser = argparse.ArgumentParser(
        description='Compare source files between two SDK versions.')
    parser.add_argument('old', type=Path, help='Old version docs/zh/ directory')
    parser.add_argument('new', type=Path, help='New version docs/zh/ directory')
    parser.add_argument('--json', action='store_true', help='Output JSON')
    args = parser.parse_args()

    for d in (args.old, args.new):
        if not d.is_dir():
            print(f"Error: {d} is not a directory", file=sys.stderr)
            sys.exit(1)

    result = diff_versions(args.old, args.new)

    if args.json:
        print(json.dumps(result, indent=2, ensure_ascii=False))
    else:
        print_report(result)


if __name__ == '__main__':
    main()
