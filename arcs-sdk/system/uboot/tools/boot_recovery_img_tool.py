#!/usr/bin/env python3
"""Convert between boot_recovery_img.h (RGB565 byte array) and BMP."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    sys.stderr.write("error: Pillow is required. Install with: pip install Pillow\n")
    sys.exit(1)


def rgb565_to_rgb888(b0: int, b1: int) -> tuple[int, int, int]:
    """Decode one little-endian RGB565 pixel (b0 = low byte, b1 = high byte)."""
    v = (b1 << 8) | b0
    r5 = (v >> 11) & 0x1F
    g6 = (v >> 5) & 0x3F
    b5 = v & 0x1F
    # Replicate high bits into low to preserve white as pure 0xFF.
    r = (r5 << 3) | (r5 >> 2)
    g = (g6 << 2) | (g6 >> 4)
    b = (b5 << 3) | (b5 >> 2)
    return r, g, b


def rgb888_to_rgb565(r: int, g: int, b: int) -> tuple[int, int]:
    """Encode one RGB888 pixel as little-endian RGB565 (low byte first)."""
    v = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
    return v & 0xFF, (v >> 8) & 0xFF


def parse_header(path: Path) -> tuple[int, int, str, bytes]:
    text = path.read_text()

    w = re.search(r"#define\s+\w+_WIDTH\s+(\d+)", text)
    h = re.search(r"#define\s+\w+_HEIGHT\s+(\d+)", text)
    if not w or not h:
        raise ValueError(f"{path}: missing WIDTH/HEIGHT define")

    arr = re.search(
        r"static\s+const\s+uint8_t\s+(\w+)\s*\[\s*\]\s*=\s*\{(.*?)\}\s*;",
        text,
        re.DOTALL,
    )
    if not arr:
        raise ValueError(f"{path}: cannot find uint8_t array")

    name = arr.group(1)
    data = bytes(int(tok, 0) for tok in re.findall(r"0[xX][0-9a-fA-F]+", arr.group(2)))

    width, height = int(w.group(1)), int(h.group(1))
    expected = width * height * 2
    if len(data) != expected:
        raise ValueError(
            f"{path}: array size {len(data)} != {width}*{height}*2 ({expected})"
        )
    return width, height, name, data


def header_to_bmp(header_path: Path, bmp_path: Path) -> None:
    width, height, _, data = parse_header(header_path)
    img = Image.new("RGB", (width, height))
    px = img.load()
    for y in range(height):
        for x in range(width):
            i = (y * width + x) * 2
            px[x, y] = rgb565_to_rgb888(data[i], data[i + 1])
    img.save(bmp_path, format="BMP")
    print(f"{bmp_path} ({width}x{height})")


HEADER_TEMPLATE = """\
/*
 * Copyright (c) 2026, LISTENAI
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Pre-rendered RGB565 recovery-mode indicator.
 * Generated from {src} by {tool}. */

#pragma once

#include <stdint.h>

#define {prefix}_WIDTH  {width}
#define {prefix}_HEIGHT {height}

__attribute__((section(".boot.image"))) static const uint8_t {name}[] = {{
{body}}};
"""


def bmp_to_header(
    bmp_path: Path,
    header_path: Path,
    name: str,
    prefix: str | None,
) -> None:
    img = Image.open(bmp_path).convert("RGB")
    width, height = img.size
    px = img.load()

    data = bytearray()
    for y in range(height):
        for x in range(width):
            r, g, b = px[x, y]
            data += bytes(rgb888_to_rgb565(r, g, b))

    per_line = 16
    rows = []
    for i in range(0, len(data), per_line):
        chunk = data[i : i + per_line]
        cells = ", ".join(f"0x{b:02X}" for b in chunk)
        if i + per_line < len(data):
            cells += ", "
        rows.append(f"    {cells}\n")

    if prefix is None:
        prefix = name.upper().removesuffix("_DATA")

    header_path.write_text(
        HEADER_TEMPLATE.format(
            src=bmp_path.name,
            tool=Path(__file__).name,
            prefix=prefix,
            width=width,
            height=height,
            name=name,
            body="".join(rows),
        )
    )
    print(f"{header_path} ({width}x{height}, {len(data)} bytes)")


def main() -> int:
    p = argparse.ArgumentParser(
        description="Convert between C header (RGB565 array) and BMP"
    )
    sub = p.add_subparsers(dest="cmd", required=True)

    to_bmp = sub.add_parser("to-bmp", help="header -> BMP")
    to_bmp.add_argument("input", type=Path)
    to_bmp.add_argument("output", type=Path)

    to_hdr = sub.add_parser("to-header", help="BMP -> header")
    to_hdr.add_argument("input", type=Path)
    to_hdr.add_argument("output", type=Path)
    to_hdr.add_argument(
        "--name",
        default="boot_recovery_img_data",
        help="array identifier (default: %(default)s)",
    )
    to_hdr.add_argument(
        "--prefix",
        default=None,
        help="#define prefix, default derived from --name (BOOT_RECOVERY_IMG)",
    )

    args = p.parse_args()

    if args.cmd == "to-bmp":
        header_to_bmp(args.input, args.output)
    elif args.cmd == "to-header":
        bmp_to_header(args.input, args.output, args.name, args.prefix)
    return 0


if __name__ == "__main__":
    sys.exit(main())
