#!/usr/bin/env python3
"""Convert an image to an RGB565 C header for the Thinker ResNet18 sample."""

from __future__ import annotations

import argparse
from pathlib import Path
import re
import sys

try:
    from PIL import Image
except ImportError as exc:
    raise SystemExit("Pillow is required. Install it with: python3 -m pip install Pillow") from exc


def parse_rgb(value: str) -> tuple[int, int, int]:
    match = re.fullmatch(r"#?([0-9a-fA-F]{6})", value)
    if match is None:
        raise argparse.ArgumentTypeError("background must be a 6-digit RGB hex value, for example #ffffff")

    raw = match.group(1)
    return int(raw[0:2], 16), int(raw[2:4], 16), int(raw[4:6], 16)


def default_guard(output: Path) -> str:
    name = re.sub(r"[^0-9A-Za-z]+", "_", output.name).upper().strip("_")
    if not name:
        name = "IMAGE_RGB565_H"
    if name[0].isdigit():
        name = "_" + name
    return f"THINKER_RESNET18_{name}_"


def rgb888_to_rgb565(pixel: tuple[int, int, int]) -> int:
    r, g, b = pixel
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def load_resized_rgb(path: Path, width: int, height: int, background: tuple[int, int, int]) -> Image.Image:
    image = Image.open(path).convert("RGBA")
    canvas = Image.new("RGBA", image.size, (*background, 255))
    image = Image.alpha_composite(canvas, image).convert("RGB")

    resampling = getattr(Image, "Resampling", Image).LANCZOS
    return image.resize((width, height), resampling)


def write_header(
    output: Path,
    source: Path,
    pixels: list[int],
    width: int,
    height: int,
    symbol: str,
    width_macro: str,
    height_macro: str,
    guard: str,
    values_per_line: int,
) -> None:
    lines = [
        f"/* Generated from {source.as_posix()}. */",
        f"#ifndef {guard}",
        f"#define {guard}",
        "",
        "#include <stdint.h>",
        "",
        f"#define {width_macro}  ({width}U)",
        f"#define {height_macro} ({height}U)",
        "",
        f"static const uint16_t {symbol}[{width_macro} * {height_macro}] = {{",
    ]

    for index in range(0, len(pixels), values_per_line):
        chunk = pixels[index : index + values_per_line]
        lines.append("    " + ", ".join(f"0x{value:04x}" for value in chunk) + ",")

    lines.extend(
        [
            "};",
            "",
            f"#endif /* {guard} */",
            "",
        ]
    )

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("\n".join(lines), encoding="ascii")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Convert PNG/JPEG/etc. to a static RGB565 uint16_t C header.",
    )
    parser.add_argument("input", type=Path, help="source image path")
    parser.add_argument("output", type=Path, help="output header path")
    parser.add_argument("--width", type=int, default=64, help="output width in pixels")
    parser.add_argument("--height", type=int, default=64, help="output height in pixels")
    parser.add_argument("--symbol", default="test_image", help="C array symbol name")
    parser.add_argument("--width-macro", default="TEST_IMAGE_WIDTH", help="C width macro name")
    parser.add_argument("--height-macro", default="TEST_IMAGE_HEIGHT", help="C height macro name")
    parser.add_argument("--guard", help="include guard; defaults to one derived from output file name")
    parser.add_argument("--background", type=parse_rgb, default=parse_rgb("#ffffff"), help="RGBA matte color")
    parser.add_argument("--values-per-line", type=int, default=8, help="number of RGB565 values per output line")
    return parser


def main(argv: list[str]) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)

    if args.width <= 0 or args.height <= 0:
        parser.error("--width and --height must be positive")
    if args.values_per_line <= 0:
        parser.error("--values-per-line must be positive")

    image = load_resized_rgb(args.input, args.width, args.height, args.background)
    pixels = [rgb888_to_rgb565(pixel) for pixel in image.getdata()]
    guard = args.guard or default_guard(args.output)

    write_header(
        output=args.output,
        source=args.input,
        pixels=pixels,
        width=args.width,
        height=args.height,
        symbol=args.symbol,
        width_macro=args.width_macro,
        height_macro=args.height_macro,
        guard=guard,
        values_per_line=args.values_per_line,
    )
    print(f"Wrote {args.output} ({args.width}x{args.height}, {len(pixels)} pixels)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
