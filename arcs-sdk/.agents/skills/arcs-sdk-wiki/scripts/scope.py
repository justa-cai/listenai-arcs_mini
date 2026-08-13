#!/usr/bin/env python3
"""scope.py — 解析 Sphinx doctree，输出 toctree 可达页面列表。

用法:
    python scripts/scope.py <doctrees_dir>
    python scripts/scope.py Arcs-SDK-wiki/Docs/v0.1.5/docs/output/zh/doctrees/

输出: 每行一个可达页面路径（相对于 docs/zh/），按字母排序。

如果 doctree 不存在或解析失败，回退到扫描 docs/zh/ 文件列表。
"""

import pickle
import sys
import os
from pathlib import Path


def _patch_sphinx():
    """Patch Sphinx classes to handle version mismatch when loading pickle."""
    try:
        import sphinx.config
        orig = sphinx.config.Config.__setstate__
        def patched(self, state):
            if isinstance(state, dict):
                state.setdefault('_options', {})
                state.setdefault('_raw_config', {})
            try:
                orig(self, state)
            except Exception:
                self.__dict__.update(state)
        sphinx.config.Config.__setstate__ = patched
    except ImportError:
        pass

    class Dummy:
        def __setstate__(self, state):
            if isinstance(state, dict):
                self.__dict__.update(state)

    for mod_name in ('sphinx.domains.c', 'sphinx.domains.cpp'):
        try:
            mod = __import__(mod_name, fromlist=['LookupKey'])
            if not hasattr(mod, 'LookupKey'):
                mod.LookupKey = Dummy
        except ImportError:
            pass


def load_toctree_includes(doctrees_dir: Path) -> dict:
    _patch_sphinx()
    pickle_path = doctrees_dir / 'environment.pickle'
    with open(pickle_path, 'rb') as f:
        env = pickle.load(f)
    return env.toctree_includes


def walk_reachable(toctree_includes: dict, root: str = 'index') -> set:
    """BFS from root, return all reachable page names."""
    reachable = set()
    queue = [root]
    while queue:
        page = queue.pop(0)
        if page in reachable:
            continue
        reachable.add(page)
        for child in toctree_includes.get(page, []):
            if child not in reachable:
                queue.append(child)
    return reachable


def fallback_scan(docs_zh_dir: Path) -> set:
    """Fallback: scan docs/zh/ for .md and .rst files."""
    pages = set()
    for ext in ('*.md', '*.rst'):
        for p in docs_zh_dir.rglob(ext):
            rel = p.relative_to(docs_zh_dir).with_suffix('')
            pages.add(str(rel))
    return pages


def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <doctrees_dir> [--fallback <docs_zh_dir>]", file=sys.stderr)
        sys.exit(1)

    doctrees_dir = Path(sys.argv[1])
    fallback_dir = None
    if '--fallback' in sys.argv:
        idx = sys.argv.index('--fallback')
        if idx + 1 < len(sys.argv):
            fallback_dir = Path(sys.argv[idx + 1])

    pickle_path = doctrees_dir / 'environment.pickle'

    if pickle_path.exists():
        try:
            ti = load_toctree_includes(doctrees_dir)
            reachable = walk_reachable(ti)
            for page in sorted(reachable):
                print(page)
            print(f"# Total: {len(reachable)} reachable pages", file=sys.stderr)
            return
        except Exception as e:
            print(f"# Warning: doctree parse failed ({e}), trying fallback", file=sys.stderr)

    if fallback_dir and fallback_dir.exists():
        pages = fallback_scan(fallback_dir)
        for page in sorted(pages):
            print(page)
        print(f"# Total: {len(pages)} pages (fallback scan)", file=sys.stderr)
    else:
        print("# Error: no doctree and no fallback directory", file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
