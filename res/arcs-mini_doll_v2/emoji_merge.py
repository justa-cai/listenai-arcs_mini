#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path
from typing import Iterable, List, Tuple

from PIL import Image


def _find_frame_files(dir_path: Path) -> List[Path]:
    if not dir_path.is_dir():
        return []
    return sorted(p for p in dir_path.iterdir() if p.is_file() and p.name.startswith("frame-") and p.suffix.lower() == ".png")


def _pair_frames(left_dir: Path, right_dir: Path) -> List[Tuple[Path, Path]]:
    left_frames = {p.name: p for p in _find_frame_files(left_dir)}
    right_frames = {p.name: p for p in _find_frame_files(right_dir)}
    pairs = []
    for name in sorted(set(left_frames) | set(right_frames)):
        if name in left_frames and name in right_frames:
            pairs.append((left_frames[name], right_frames[name]))
        else:
            missing = "left" if name not in left_frames else "right"
            print(f"[WARN] missing {missing} frame: {name} in {left_dir.parent.name}")
    return pairs


def _merge_pair(left_path: Path, right_path: Path, out_path: Path, distance: int, offset: int) -> None:
    with Image.open(left_path) as left_img, Image.open(right_path) as right_img:
        # Swap left/right placement as requested.
        left = right_img.convert("RGBA")
        right = left_img.convert("RGBA")

        extra_top = max(offset, 0)
        extra_bottom = max(-offset, 0)
        y_offset = extra_top

        half = distance / 2.0
        left_x = int(round(-half))
        right_x = int(round(left.width + half))

        if distance > 0:
            min_x = min(left_x, right_x)
            max_x = max(left_x + left.width, right_x + right.width)
            out_width = max_x - min_x
            out_height = max(left.height, right.height) + extra_top + extra_bottom
            merged = Image.new("RGBA", (out_width, out_height), (0, 0, 0, 0))
            offset = -min_x
            merged.paste(left, (left_x + offset, y_offset))
            merged.paste(right, (right_x + offset, y_offset))
        else:
            out_width = left.width + right.width
            out_height = max(left.height, right.height) + extra_top + extra_bottom
            merged = Image.new("RGBA", (out_width, out_height), (0, 0, 0, 0))

            if distance < 0:
                midline = left.width
                left_max_x = midline - left_x
                right_min_x = midline - right_x

                if left_max_x > 0:
                    left_crop = left.crop((0, 0, min(left.width, left_max_x), left.height))
                    merged.paste(left_crop, (left_x, y_offset))

                if right_min_x < right.width:
                    right_crop = right.crop((max(0, right_min_x), 0, right.width, right.height))
                    merged.paste(right_crop, (right_x + max(0, right_min_x), y_offset))
            else:
                merged.paste(left, (left_x, y_offset))
                merged.paste(right, (right_x, y_offset))

        out_path.parent.mkdir(parents=True, exist_ok=True)
        merged.save(out_path, format="PNG")


def merge_all(input_root: Path, output_root: Path, distance: int, offset: int) -> None:
    if not input_root.is_dir():
        raise FileNotFoundError(f"input root not found: {input_root}")

    emoji_ids = [p for p in input_root.iterdir() if p.is_dir()]
    for emoji_dir in sorted(emoji_ids, key=lambda p: p.name):
        left_dir = emoji_dir / "left"
        right_dir = emoji_dir / "right"
        if not left_dir.is_dir() or not right_dir.is_dir():
            print(f"[WARN] skip {emoji_dir.name}: missing left/right dirs")
            continue

        for left_path, right_path in _pair_frames(left_dir, right_dir):
            out_path = output_root / emoji_dir.name / left_path.name
            _merge_pair(left_path, right_path, out_path, distance, offset)
            print(f"[OK] {emoji_dir.name}/{left_path.name}")


def _parse_args(argv: Iterable[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Merge left/right emoji frames into single frames.")
    parser.add_argument(
        "--input",
        default="res/emoji_src",
        help="Input root directory, default: res/emoji_src",
    )
    parser.add_argument(
        "--output",
        default="res/emoji",
        help="Output root directory, default: res/emoji",
    )
    parser.add_argument(
        "--distance",
        type=int,
        default=0,
        help="Horizontal distance between left/right. Positive moves outward (can overflow); negative moves inward and clips at midline.",
    )
    parser.add_argument(
        "--offset",
        type=int,
        default=0,
        help="Vertical offset padding. Positive adds blank above; negative adds blank below.",
    )
    return parser.parse_args(list(argv) if argv is not None else None)


def main() -> None:
    args = _parse_args()
    merge_all(Path(args.input), Path(args.output), int(args.distance), int(args.offset))


if __name__ == "__main__":
    main()
