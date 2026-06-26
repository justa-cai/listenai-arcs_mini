#!/usr/bin/env python3
import argparse
import sys
from pathlib import Path

PRODUCTS = [
    ("VENUS", 213, 192, 196, 252),
    ("VEGA", 212, 192, 196, 252),
    ("VEGAH", 229, 208, 212, 252),
    ("ARCS", 340, 320, 324, 380),
    ("MARS", 180, 160, 164, 220),
    ("APUS", 196, 176, 180, 236),
    ("VENUSA", 294, 272, 276, 332),
    ("SPICA", 229, 208, 212, 252),
    ("NEBULAA", 295, 272, 276, 332),
]


def patch_image(path: Path) -> int:
    data = bytearray(path.read_bytes())
    selected = None
    for product, end_offset, img_hdr_pos, img_size_pos, hdr_sum_pos in PRODUCTS:
        start = end_offset - len(product)
        if start >= 0 and data[start:end_offset].rstrip(b"\0") == product.encode("ascii"):
            selected = (product, img_hdr_pos, img_size_pos, hdr_sum_pos)
            break

    if selected is None:
        print("error: unsupported binary files", file=sys.stderr)
        return 1

    product, img_hdr_pos, img_size_pos, hdr_sum_pos = selected
    print(f"Product string stored in variable: {product} hdr_pos {img_hdr_pos}, img_size {img_size_pos} img_sum is {hdr_sum_pos}")

    img_size = len(data)
    data[img_size_pos:img_size_pos + 4] = img_size.to_bytes(4, "little", signed=False)

    sumv = 0
    sumh = 0
    for i, value in enumerate(data[:hdr_sum_pos]):
        if i < img_hdr_pos:
            sumv += value
        else:
            sumh += value
    sumv = sumv + sumh + (sumh & 0xFF) + (sumh >> 8)

    data[hdr_sum_pos:hdr_sum_pos + 4] = bytes([
        sumh & 0xFF,
        (sumh >> 8) & 0xFF,
        sumv & 0xFF,
        (sumv >> 8) & 0xFF,
    ])
    path.write_bytes(data)
    return 0


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Patch ListenAI boot header fields into a firmware image."
    )
    parser.add_argument("image", nargs="?", help="image file to patch in place")
    args = parser.parse_args(argv)
    if not args.image:
        parser.print_help(sys.stderr)
        return 1
    return patch_image(Path(args.image))


if __name__ == "__main__":
    raise SystemExit(main())
