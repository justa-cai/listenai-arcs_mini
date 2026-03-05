#!/usr/bin/env python3
"""Build manifest.json and package all referenced images into an .lpk archive."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import zipfile
from typing import Dict, List


MANIFEST_VERSION = 2
CHIP = "arcs"


def compute_md5(path: Path) -> str:
    digest = hashlib.md5()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(8192), b""):
            digest.update(chunk)
    return digest.hexdigest()


def resolve_vars(file_spec: str, vars_map: Dict[str, Path]) -> str:
    expanded = file_spec
    for key, value in vars_map.items():
        expanded = expanded.replace(f"${{{key}}}", str(value))

    unresolved = re.findall(r"\$\{([^}]+)\}", expanded)
    if unresolved:
        missing = ", ".join(sorted(set(unresolved)))
        raise ValueError(f"missing --var for: {missing}")

    return expanded


def resolve_image_path(
    table_path: Path, file_spec: str, vars_map: Dict[str, Path]
) -> Path:
    expanded = resolve_vars(file_spec, vars_map)
    path = Path(expanded)
    if not path.is_absolute():
        path = (table_path.parent / path).resolve()
    return path


def load_partition_table(table_path: Path) -> List[Dict]:
    with table_path.open("r", encoding="utf-8") as f:
        data = json.load(f)
    if not isinstance(data, list):
        raise ValueError("partition table must be a JSON array")
    return data


def collect_images(
    table_path: Path, vars_map: Dict[str, Path], no_boot: bool = False
) -> List[Dict]:
    images = load_partition_table(table_path)
    collected = []
    for image in images:
        if no_boot and image.get("name") == "boot":
            continue

        file_spec = image.get("file")
        if not file_spec:
            raise ValueError("partition entry missing 'file' field")

        source_path = resolve_image_path(table_path, file_spec, vars_map)
        if not source_path.is_file():
            raise FileNotFoundError(f"missing image file: {source_path}")

        collected.append(
            {
                "name": image.get("name"),
                "addr": image.get("addr"),
                "file": source_path.name,
                "source_path": source_path,
                "md5": compute_md5(source_path),
            }
        )
    return collected


def build_manifest(images: List[Dict]) -> Dict:
    manifest = {"manifest": MANIFEST_VERSION, "chip": CHIP, "images": []}
    for image in images:
        manifest["images"].append(
            {
                "name": image.get("name"),
                "addr": image.get("addr"),
                "file": f"./{image.get('file')}",
                "md5": image.get("md5"),
            }
        )
    return manifest


def package_lpk(manifest: Dict, images: List[Dict], output_path: Path) -> None:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_text = json.dumps(manifest, indent=4) + "\n"

    with zipfile.ZipFile(output_path, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        zf.writestr("manifest.json", manifest_text)
        for image in images:
            source_path = image["source_path"]
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
        "--partition-table",
        type=Path,
        required=True,
        help="Partition table JSON file; file paths are relative to the JSON by default.",
    )
    parser.add_argument(
        "--no-boot",
        action="store_true",
        help="Exclude the boot image from the package.",
    )
    parser.add_argument(
        "--var",
        action="append",
        default=[],
        metavar="KEY=VALUE",
        help="Variable mapping for ${KEY}; VALUE is relative to cwd by default.",
    )
    return parser.parse_args()


def parse_vars(var_args: List[str]) -> Dict[str, Path]:
    vars_map: Dict[str, Path] = {}

    for item in var_args:
        if "=" not in item:
            raise ValueError(f"invalid --var '{item}', expected KEY=VALUE")
        key, value = item.split("=", 1)
        key = key.strip()
        value = value.strip()
        if not key:
            raise ValueError(f"invalid --var '{item}', empty KEY")
        value_path = Path(value)
        if not value_path.is_absolute():
            value_path = (Path.cwd() / value_path).resolve()
        vars_map[key] = value_path

    return vars_map


def main() -> None:
    args = parse_args()
    table_path = args.partition_table

    vars_map = parse_vars(args.var)
    images = collect_images(table_path, vars_map, no_boot=args.no_boot)
    manifest = build_manifest(images)
    package_lpk(manifest, images, args.output)

    print(f"wrote lpk to {args.output}")


if __name__ == "__main__":
    main()
