#!/usr/bin/env python3
"""Build manifest.json and package all referenced images into an .lpk archive."""

import argparse
import hashlib
import json
from pathlib import Path
import zipfile


MANIFEST_VERSION = 2
CHIP = "arcs"

# Update file paths or addresses here if the layout changes.
IMAGES = [
    {"name": "boot", "addr": "0x0", "file": "./res/boot.bin"},
    {"name": "ap", "addr": "0x40000", "file": "./res/ap.bin"},
    {"name": "app-config", "addr": "0xF0000", "file": "./res/app-config.json"},
    {"name": "tone", "addr": "0x100000", "file": "./res/tone.bin"},
    {"name": "wake_word", "addr": "0x200000", "file": "./res/wake_word.bin"},
    {"name": "aiui", "addr": "0x600000", "file": "./build/aiui.bin"},
]


def compute_md5(path: Path) -> str:
    digest = hashlib.md5()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(8192), b""):
            digest.update(chunk)
    return digest.hexdigest()


def build_manifest(base_dir: Path) -> dict:
    manifest = {"manifest": MANIFEST_VERSION, "chip": CHIP, "images": []}

    for image in IMAGES:
        source_path = (base_dir / image["file"]).resolve()
        if not source_path.is_file():
            raise FileNotFoundError(f"missing image file: {source_path}")

        manifest["images"].append({**image, "md5": compute_md5(source_path)})

    return manifest


def package_lpk(manifest: dict, base_dir: Path, output_path: Path) -> None:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_text = json.dumps(manifest, indent=4) + "\n"

    with zipfile.ZipFile(output_path, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        zf.writestr("manifest.json", manifest_text)
        for image in manifest["images"]:
            source_path = (base_dir / image["file"]).resolve()
            print(
                f"packing image: {image['name']} ({image['addr']}) md5={image['md5']} -> {image['file']}"
            )
            zf.write(source_path, arcname=image["file"])


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate manifest.json and package an .lpk archive."
    )
    parser.add_argument(
        "output",
        type=Path,
        help="Output .lpk path",
    )
    parser.add_argument(
        "--base-dir",
        type=Path,
        default=None,
        help="Base directory for image files; defaults to the script directory.",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    base_dir = args.base_dir or Path.cwd()

    manifest = build_manifest(base_dir)
    package_lpk(manifest, base_dir, args.output)

    print(f"wrote lpk to {args.output}")


if __name__ == "__main__":
    main()
