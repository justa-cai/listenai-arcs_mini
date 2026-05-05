#!/usr/bin/env python3
"""Render text to a BMP suitable for feeding into boot_recovery_img_tool.py."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    sys.stderr.write("error: Pillow is required. Install with: pip install Pillow\n")
    sys.exit(1)


def parse_color(spec: str) -> tuple[int, int, int]:
    s = spec.strip().lstrip("#")
    if s.startswith(("0x", "0X")):
        s = s[2:]
    if len(s) == 3:
        s = "".join(c * 2 for c in s)
    if len(s) != 6:
        raise argparse.ArgumentTypeError(
            f"color must be 6 hex digits (or #rgb / 0xRRGGBB), got {spec!r}"
        )
    return int(s[0:2], 16), int(s[2:4], 16), int(s[4:6], 16)


def load_font(path: Path, size: int) -> ImageFont.FreeTypeFont:
    try:
        return ImageFont.truetype(str(path), size)
    except OSError as e:
        raise SystemExit(f"error: cannot open font {path}: {e}")


def render(
    text: str,
    font_path: Path,
    font_size: int,
    fg: tuple[int, int, int],
    fg2: tuple[int, int, int],
    bg: tuple[int, int, int],
    width: int | None,
    height: int | None,
    padding: int,
    spacing: int,
    align: str,
) -> Image.Image:
    font = load_font(font_path, font_size)

    # Use a throwaway canvas to query the multi-line bounding box.
    probe = ImageDraw.Draw(Image.new("RGB", (1, 1)))
    raw = probe.multiline_textbbox((0, 0), text, font=font, spacing=spacing, align=align)
    # Newer Pillow returns floats; coerce to int so downstream sizing is clean.
    bbox = tuple(int(v) for v in raw)
    text_w = bbox[2] - bbox[0]
    text_h = bbox[3] - bbox[1]

    img_w = width if width is not None else text_w + padding * 2
    img_h = height if height is not None else text_h + padding * 2

    img = Image.new("RGB", (img_w, img_h), bg)
    draw = ImageDraw.Draw(img)
    draw_x = (img_w - text_w) // 2 - bbox[0]
    draw_y = (img_h - text_h) // 2 - bbox[1]

    lines = text.split("\n")
    if len(lines) > 1 and fg2 != fg:
        # 整段用次色 fg2 画一遍，再用主色 fg 把首行覆盖回去。借 Pillow 自己
        # 的 multiline 排版避免手算行距出现偏差。
        draw.multiline_text(
            (draw_x, draw_y), text, font=font, fill=fg2,
            spacing=spacing, align=align,
        )
        line0_bbox = probe.textbbox((0, 0), lines[0], font=font)
        line0_w = line0_bbox[2] - line0_bbox[0]
        if align == "center":
            x0 = draw_x + (text_w - line0_w) // 2 - line0_bbox[0]
        elif align == "right":
            x0 = draw_x + text_w - line0_bbox[2]
        else:
            x0 = draw_x - line0_bbox[0]
        draw.text((x0, draw_y), lines[0], font=font, fill=fg)
    else:
        draw.multiline_text(
            (draw_x, draw_y),
            text,
            font=font,
            fill=fg,
            spacing=spacing,
            align=align,
        )
    return img


def main() -> int:
    p = argparse.ArgumentParser(
        description="Render text into a BMP image. "
        "Use \\n inside --text for line breaks."
    )
    p.add_argument(
        "--text",
        required=True,
        help=r"text to render; supports \n \r \t escapes for multi-line",
    )
    p.add_argument("--font", type=Path, required=True, help="TTF/OTF font path")
    p.add_argument("--size", type=int, required=True, help="font size in px")
    p.add_argument(
        "--fg",
        type=parse_color,
        default=(0xFF, 0xFF, 0xFF),
        help="text color (default: white)",
    )
    p.add_argument(
        "--fg2",
        type=parse_color,
        default=None,
        help="text color for lines after the first (default: 60%% of --fg)",
    )
    p.add_argument(
        "--bg",
        type=parse_color,
        default=(0x00, 0x00, 0x00),
        help="background color (default: black)",
    )
    p.add_argument(
        "--width",
        type=int,
        default=None,
        help="fixed output width (default: text width + padding)",
    )
    p.add_argument(
        "--height",
        type=int,
        default=None,
        help="fixed output height (default: text height + padding)",
    )
    p.add_argument(
        "--padding",
        type=int,
        default=4,
        help="padding around text when size is auto (default: %(default)s)",
    )
    p.add_argument(
        "--spacing",
        type=int,
        default=4,
        help="pixels between lines for multi-line text (default: %(default)s)",
    )
    p.add_argument(
        "--align",
        choices=("left", "center", "right"),
        default="center",
        help="per-line alignment within the text block (default: %(default)s)",
    )
    p.add_argument("output", type=Path)

    args = p.parse_args()

    # Decode \n / \r / \t so users can pass them through argparse without
    # depending on shell-specific $'...' quoting. 不走 unicode_escape，
    # 那条路径会把非 ASCII 字符（含中文）的 UTF-8 多字节拆成独立
    # Latin-1 码点，变成乱码。
    text = args.text.replace("\\n", "\n").replace("\\r", "\r").replace("\\t", "\t")

    fg2 = args.fg2 if args.fg2 is not None else tuple(c * 3 // 5 for c in args.fg)

    img = render(
        text,
        args.font,
        args.size,
        args.fg,
        fg2,
        args.bg,
        args.width,
        args.height,
        args.padding,
        args.spacing,
        args.align,
    )
    img.save(args.output, format="BMP")
    print(f"{args.output} ({img.width}x{img.height})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
