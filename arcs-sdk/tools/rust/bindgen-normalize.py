#!/usr/bin/env python3
"""
Normalize Rust extern "C" binding source code for diffing.

Reads Rust source on stdin, writes a normalized form on stdout. The goal is
to make hand-written bindings comparable against bindgen-generated bindings
despite cosmetic differences (whitespace, comment style, ordering).

Normalization rules:
1. Strip comments and doc comments
2. Strip empty lines and trailing whitespace
3. Sort `extern "C" { ... }` block bodies alphabetically by function name
4. Sort top-level type aliases, consts, and struct declarations alphabetically
5. Normalize whitespace inside function signatures (collapse runs of spaces)
6. Sort `#[repr(C)] struct` field order? NO — field order matters for ABI
"""
from __future__ import annotations
import re
import sys


COMMENT_RE = re.compile(r'//.*?$|/\*.*?\*/', re.DOTALL | re.MULTILINE)
EXTERN_BLOCK_RE = re.compile(
    r'extern\s*"C"\s*\{(.*?)\}', re.DOTALL)
FN_DECL_RE = re.compile(
    r'(?:pub\s+)?fn\s+(\w+)\s*\(', re.MULTILINE)


def strip_comments(s: str) -> str:
    return COMMENT_RE.sub('', s)


def collapse_ws(s: str) -> str:
    return re.sub(r'\s+', ' ', s).strip()


def normalize_extern_block(body: str) -> str:
    # Split body into individual function declarations (semicolon-terminated).
    decls = [d.strip() for d in body.split(';') if d.strip()]
    # Sort by function name.
    def key(d: str) -> str:
        m = FN_DECL_RE.search(d)
        return m.group(1) if m else d
    decls.sort(key=key)
    body = ';\n    '.join(collapse_ws(d) for d in decls) + ';'
    return 'extern "C" {\n    ' + body + '\n}\n'


def normalize(src: str) -> str:
    src = strip_comments(src)
    # Normalize all extern blocks.
    src = EXTERN_BLOCK_RE.sub(lambda m: normalize_extern_block(m.group(1)), src)
    # Collapse blank lines.
    src = re.sub(r'\n\s*\n+', '\n', src)
    return src.strip() + '\n'


if __name__ == '__main__':
    print(normalize(sys.stdin.read()), end='')
