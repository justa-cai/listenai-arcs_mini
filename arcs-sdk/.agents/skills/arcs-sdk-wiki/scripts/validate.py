#!/usr/bin/env python3
"""validate.py — Wiki 机械检查：frontmatter、wiki-link、index 一致性。

用法:
    python scripts/validate.py <wiki_dir>
    python scripts/validate.py wiki/v0.1.5/

检查项:
    1. frontmatter — 必需字段（title, type, sources, confidence, created, updated）
    2. wiki-link — [[target]] 目标文件是否存在
    3. index — index.md 中列出的页面是否存在，wiki 中的页面是否都在 index 中
    4. type — frontmatter type 是否与目录匹配
    5. structure — entity/concept 的 Evolution Log、concept 多源约束
    6. state files — manifest.md / log.md 的基础 schema

输出: 按类别分组的问题列表，退出码 = 问题总数（上限 127）。
"""

import re
import sys
from pathlib import Path

VALID_TYPES = {'summary', 'entity', 'concept', 'comparison', 'matrix',
               'guide', 'scenario', 'overview'}

VALID_CONFIDENCE = {'low', 'medium', 'high'}

REQUIRED_FIELDS = {'title', 'type', 'sources', 'confidence', 'created', 'updated'}

MANIFEST_REQUIRED_FIELDS = {
    'version',
    'status',
    'mode',
    'source_count',
    'wiki_page_count',
    'created',
    'updated',
}

TYPE_DIR_MAP = {
    'summary': 'summaries',
    'entity': 'entities',
    'concept': 'concepts',
    'comparison': 'comparisons',
    'matrix': 'matrices',
    'guide': 'guides',
    'scenario': 'scenarios',
}

SKIP_FILES = {'index.md', 'manifest.md', 'log.md', 'CLAUDE.md',
              'question-dimensions.md'}

WIKILINK_RE = re.compile(r'\[\[([^\]|]+)(?:\|[^\]]+)?\]\]')
FRONTMATTER_RE = re.compile(r'^---\s*\n(.*?)\n---', re.DOTALL)
LOG_HEADING_RE = re.compile(r'^## \[[^\]]+\] ([^|]+) \| .+$')
LOG_OPERATIONS = {'ingest', 'query', 'lint', 'fix'}


def extract_frontmatter(text: str) -> str | None:
    m = FRONTMATTER_RE.match(text)
    if not m:
        return None
    return m.group(1)


def parse_frontmatter(text: str) -> dict | None:
    block = extract_frontmatter(text)
    if block is None:
        return None
    fm = {}
    lines = block.splitlines()
    i = 0
    while i < len(lines):
        line = lines[i]
        if ':' in line and not line.startswith(' ') and not line.startswith('-'):
            key, _, val = line.partition(':')
            key = key.strip()
            val = val.strip()
            if key == 'sources':
                sources = []
                i += 1
                while i < len(lines):
                    subline = lines[i]
                    stripped = subline.strip()
                    if not stripped:
                        i += 1
                        continue
                    if subline.startswith('  - ') or subline.startswith('- '):
                        sources.append(stripped[2:].strip())
                        i += 1
                        continue
                    if subline.startswith(' '):
                        i += 1
                        continue
                    break
                fm[key] = sources
                continue
            fm[key] = val
        i += 1
    return fm


def collect_wiki_pages(wiki_dir: Path) -> dict[str, Path]:
    """Return {relative_stem: absolute_path} for all wiki .md pages."""
    pages = {}
    for p in wiki_dir.rglob('*.md'):
        rel = p.relative_to(wiki_dir)
        if rel.name in SKIP_FILES:
            continue
        if rel.parts[0] == 'reports':
            continue
        stem = str(rel.with_suffix(''))
        pages[stem] = p
    return pages


def check_frontmatter(pages: dict[str, Path]) -> list[str]:
    issues = []
    for stem, path in sorted(pages.items()):
        text = path.read_text(errors='replace')
        fm = parse_frontmatter(text)
        if fm is None:
            issues.append(f"[frontmatter] {stem}: missing frontmatter block")
            continue
        for field in REQUIRED_FIELDS:
            if field not in fm:
                issues.append(f"[frontmatter] {stem}: missing field '{field}'")
        page_type = fm.get('type', '')
        if page_type and page_type not in VALID_TYPES:
            issues.append(f"[frontmatter] {stem}: unknown type '{page_type}'")
        confidence = fm.get('confidence', '')
        if confidence and confidence not in VALID_CONFIDENCE:
            issues.append(f"[frontmatter] {stem}: unknown confidence '{confidence}'")
        sources = fm.get('sources', [])
        if isinstance(sources, list):
            if not sources:
                issues.append(f"[frontmatter] {stem}: sources must not be empty")
            for source in sources:
                if not source.startswith('docs/output/zh/html/'):
                    issues.append(f"[frontmatter] {stem}: non-canonical source '{source}'")
    return issues


def check_type_dir(pages: dict[str, Path]) -> list[str]:
    issues = []
    for stem, path in sorted(pages.items()):
        text = path.read_text(errors='replace')
        fm = parse_frontmatter(text)
        if fm is None:
            continue
        page_type = fm.get('type', '')
        if not page_type or page_type not in TYPE_DIR_MAP:
            continue
        expected_dir = TYPE_DIR_MAP[page_type]
        actual_dir = Path(stem).parts[0] if '/' in stem else ''
        if actual_dir != expected_dir:
            issues.append(
                f"[type-dir] {stem}: type='{page_type}' but in '{actual_dir}/' "
                f"(expected '{expected_dir}/')")
    return issues


def resolve_wikilink_targets(target: str, all_stems: set[str]) -> set[str]:
    target = target.strip()
    if target in all_stems:
        return {target}

    without_prefix = target
    for prefix in TYPE_DIR_MAP.values():
        if target.startswith(prefix + '/'):
            without_prefix = target[len(prefix) + 1:]
            break

    return {
        stem for stem in all_stems
        if stem.endswith('/' + without_prefix) or stem == without_prefix
    }


def format_target_set(targets: set[str]) -> str:
    return ", ".join(sorted(targets))


def check_wikilinks(pages: dict[str, Path]) -> list[str]:
    issues = []
    all_stems = set(pages.keys())
    for stem, path in sorted(pages.items()):
        text = path.read_text(errors='replace')
        for m in WIKILINK_RE.finditer(text):
            target = m.group(1).strip()
            targets = resolve_wikilink_targets(target, all_stems)
            if not targets:
                issues.append(f"[broken-link] {stem}: [[{target}]] target not found")
            elif len(targets) > 1:
                issues.append(
                    f"[ambiguous-link] {stem}: [[{target}]] matches "
                    f"{format_target_set(targets)}; use a fully qualified link"
                )
    return issues


def parse_index_links(wiki_dir: Path) -> set[str]:
    index_path = wiki_dir / 'index.md'
    if not index_path.exists():
        return set()
    text = index_path.read_text(errors='replace')
    links = set()
    for m in WIKILINK_RE.finditer(text):
        links.add(m.group(1).strip())
    return links


def check_index(pages: dict[str, Path], wiki_dir: Path) -> list[str]:
    issues = []
    index_links = parse_index_links(wiki_dir)
    all_stems = set(pages.keys())
    resolved_index_links = set()

    for link in sorted(index_links):
        targets = resolve_wikilink_targets(link, all_stems)
        if len(targets) == 1:
            resolved_index_links.update(targets)
        elif not targets:
            issues.append(f"[index-dangling] index.md links to [[{link}]] but file not found")
        else:
            issues.append(
                f"[index-ambiguous] index.md links to [[{link}]] which matches "
                f"{format_target_set(targets)}; use a fully qualified link"
            )

    for stem in sorted(all_stems):
        if stem not in resolved_index_links:
            issues.append(f"[index-missing] {stem} exists but not listed in index.md")

    return issues


def check_structure(pages: dict[str, Path]) -> list[str]:
    issues = []
    for stem, path in sorted(pages.items()):
        text = path.read_text(errors='replace')
        fm = parse_frontmatter(text)
        if fm is None:
            continue
        page_type = fm.get('type', '')
        if page_type in {'entity', 'concept'} and '## Evolution Log' not in text:
            issues.append(f"[structure] {stem}: missing '## Evolution Log' section")
        if page_type == 'concept' and len(fm.get('sources', [])) < 2:
            issues.append(f"[structure] {stem}: concept pages require at least 2 sources")
    return issues


def check_state_files(wiki_dir: Path) -> list[str]:
    issues = []

    manifest_path = wiki_dir / 'manifest.md'
    if not manifest_path.exists():
        issues.append("[manifest] manifest.md: missing required state file")
    else:
        manifest = parse_frontmatter(manifest_path.read_text(errors='replace'))
        if manifest is None:
            issues.append("[manifest] manifest.md: missing frontmatter block")
        else:
            for field in sorted(MANIFEST_REQUIRED_FIELDS):
                if field not in manifest:
                    issues.append(f"[manifest] manifest.md: missing field '{field}'")

    log_path = wiki_dir / 'log.md'
    if not log_path.exists():
        issues.append("[log] log.md: missing required state file")
    else:
        for line in log_path.read_text(errors='replace').splitlines():
            match = LOG_HEADING_RE.match(line)
            if not match:
                continue
            op = match.group(1).strip()
            if op not in LOG_OPERATIONS:
                issues.append(f"[log] log.md: unknown operation '{op}'")

    return issues


def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <wiki_dir>", file=sys.stderr)
        sys.exit(1)

    wiki_dir = Path(sys.argv[1])
    if not wiki_dir.is_dir():
        print(f"Error: {wiki_dir} is not a directory", file=sys.stderr)
        sys.exit(1)

    pages = collect_wiki_pages(wiki_dir)
    print(f"Scanning {len(pages)} wiki pages in {wiki_dir}", file=sys.stderr)

    all_issues = []
    all_issues.extend(check_frontmatter(pages))
    all_issues.extend(check_type_dir(pages))
    all_issues.extend(check_wikilinks(pages))
    all_issues.extend(check_index(pages, wiki_dir))
    all_issues.extend(check_structure(pages))
    all_issues.extend(check_state_files(wiki_dir))

    if all_issues:
        for issue in all_issues:
            print(issue)
        print(f"\n# Total: {len(all_issues)} issues", file=sys.stderr)
    else:
        print("# All checks passed", file=sys.stderr)

    sys.exit(min(len(all_issues), 127))


if __name__ == '__main__':
    main()
