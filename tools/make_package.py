#!/usr/bin/env python3
"""Generate flash image packages (LPK / Intel HEX / OTA txz) from a partition table."""

import argparse
import hashlib
import json
import re
import shutil
import sys
import tempfile
import zipfile
from pathlib import Path
from typing import Dict, List, Optional

# Reuse boot-side `create_txz` so the xz filter params stay the single source of
# truth; drifting from boot's xzdec expectations would brick OTA silently.
_SDK_PACKAGE_OTA_DIR = (
    Path(__file__).resolve().parent.parent / "arcs-sdk" / "system" / "uboot" / "tools"
)
sys.path.insert(0, str(_SDK_PACKAGE_OTA_DIR))
import package_ota  # noqa: E402  (path injection required above)


MANIFEST_VERSION = 2
CHIP = "arcs"

HEX_BYTES_PER_LINE = 16

OTA_MANIFEST_VERSION = "2.0"
OTA_DEFAULT_FLASH_BASE = 0x30000000


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


def load_partition_table(table_path: Path) -> Dict:
    with table_path.open("r", encoding="utf-8") as f:
        data = json.load(f)
    if not isinstance(data, dict):
        raise ValueError("partition table must be a JSON object")
    if not isinstance(data.get("images"), list):
        raise ValueError("partition table must contain an 'images' array")
    return data


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


def collect_images(
    table: Dict,
    table_path: Path,
    vars_map: Dict[str, Path],
    tag: str,
) -> List[Dict]:
    """Filter images whose `tags` contains the given tag."""
    collected = []
    for image in table["images"]:
        tags = image.get("tags")
        if not isinstance(tags, list):
            raise ValueError(
                f"image entry {image.get('name')!r} missing 'tags' array"
            )
        if tag not in tags:
            continue

        file_spec = image.get("file")
        if not file_spec:
            raise ValueError(f"image entry {image.get('name')!r} missing 'file' field")

        source_path = resolve_image_path(table_path, file_spec, vars_map)
        if not source_path.is_file():
            raise FileNotFoundError(f"missing image file: {source_path}")

        collected.append(
            {
                "name": image.get("name"),
                "addr": image.get("addr"),
                "file": source_path.name,
                "source_path": source_path,
            }
        )
    return collected


def build_manifest(images: List[Dict]) -> Dict:
    manifest = {"manifest": MANIFEST_VERSION, "chip": CHIP, "images": []}
    for image in images:
        manifest["images"].append(
            {
                "name": image["name"],
                "addr": image["addr"],
                "file": f"./{image['file']}",
                "md5": image["md5"],
            }
        )
    return manifest


def package_lpk(images: List[Dict], output_path: Path) -> None:
    for image in images:
        image["md5"] = compute_md5(image["source_path"])

    manifest = build_manifest(images)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_text = json.dumps(manifest, indent=4) + "\n"

    with zipfile.ZipFile(output_path, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        zf.writestr("manifest.json", manifest_text)
        for image in images:
            print(
                f"packing image: {image['name']} ({image['addr']}) "
                f"md5={image['md5']} -> {image['file']}"
            )
            zf.write(image["source_path"], arcname=image["file"])


def ihex_record(record_type: int, address: int, data: bytes = b"") -> str:
    """Format a single Intel HEX record line with checksum."""
    length = len(data)
    raw = bytes([length, (address >> 8) & 0xFF, address & 0xFF, record_type]) + data
    checksum = (~sum(raw) + 1) & 0xFF
    return f":{raw.hex().upper()}{checksum:02X}"


def ihex_extended_linear_address(upper16: int) -> str:
    data = bytes([(upper16 >> 8) & 0xFF, upper16 & 0xFF])
    return ihex_record(0x04, 0x0000, data)


def ihex_eof() -> str:
    return ihex_record(0x01, 0x0000)


def write_ihex(images: List[Dict], output_path: Path) -> None:
    """Generate Intel HEX file from partition images."""
    # HEX spec requires sorted absolute addresses for predictable output
    images = sorted(images, key=lambda x: int(x["addr"], 16))

    output_path.parent.mkdir(parents=True, exist_ok=True)
    current_upper = None

    with output_path.open("w", encoding="ascii") as f:
        for image in images:
            base_addr = int(image["addr"], 16)
            data = image["source_path"].read_bytes()
            size = len(data)

            print(
                f"packing image: {image['name']} (0x{base_addr:06X}) "
                f"size={size} -> hex"
            )

            offset = 0
            while offset < size:
                abs_addr = base_addr + offset
                upper16 = abs_addr >> 16

                if upper16 != current_upper:
                    f.write(ihex_extended_linear_address(upper16) + "\n")
                    current_upper = upper16

                lower16 = abs_addr & 0xFFFF
                chunk_size = min(HEX_BYTES_PER_LINE, size - offset)

                # A single data record must not cross a 64KB boundary
                boundary_remaining = 0x10000 - lower16
                if chunk_size > boundary_remaining:
                    chunk_size = boundary_remaining

                chunk = data[offset : offset + chunk_size]
                f.write(ihex_record(0x00, lower16, chunk) + "\n")
                offset += chunk_size

        f.write(ihex_eof() + "\n")


def _build_ota_manifest(
    images: List[Dict], flash_base: int, app_name: str, icon_name: Optional[str]
) -> Dict:
    manifest_images = []
    for image in images:
        base_offset = int(image["addr"], 16)
        payload = image["source_path"].read_bytes()
        manifest_images.append(
            {
                "file": image["file"],
                "fmt": "raw",
                "md5": hashlib.md5(payload).hexdigest(),
                "check": {"type": "none", "value": "0"},
                "copy_to": {
                    "type": "nor",
                    "addr": f"0x{flash_base + base_offset:08X}",
                    "size": str(len(payload)),
                },
            }
        )

    manifest = {
        "version": OTA_MANIFEST_VERSION,
        "app_name": app_name,
        "icon": {
            "file": icon_name or "default.png",
            "type": Path(icon_name).suffix.lstrip(".").lower() if icon_name else "png",
        },
        "image": manifest_images,
    }
    return manifest


def _stage_packager_dir(packager_dir: Path, images: List[Dict], manifest: Dict) -> None:
    """在 build 下布置 arcs-sdk package_ota 约定的 packager_dir 结构。

    布局：
        <packager_dir>/config.json
        <packager_dir>/image/<name>.bin

    会清空 image/ 旧内容，但保留目录本体，便于增量构建。
    """
    image_dir = packager_dir / "image"
    image_dir.mkdir(parents=True, exist_ok=True)
    for old in image_dir.iterdir():
        if old.is_file() or old.is_symlink():
            old.unlink()

    manifest_bytes = json.dumps(manifest, indent=4, ensure_ascii=False).encode("utf-8")
    (packager_dir / "config.json").write_bytes(manifest_bytes)

    for image in images:
        dst = image_dir / image["file"]
        print(
            f"packing image: {image['name']} -> image/{image['file']} "
            f"(addr=0x{int(image['addr'], 16):08X}, size={image['source_path'].stat().st_size})"
        )
        shutil.copyfile(image["source_path"], dst)


def build_ota_package(table: Dict, images: List[Dict], output_path: Path) -> None:
    flash_base = int(table.get("flash_base", hex(OTA_DEFAULT_FLASH_BASE)), 16)
    ota_cfg = table.get("ota") if isinstance(table.get("ota"), dict) else {}
    app_name = ota_cfg.get("app_name", "app")
    icon_name = ota_cfg.get("icon")

    manifest = _build_ota_manifest(images, flash_base, app_name, icon_name)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    # 和 output 同级常驻的 packager 暂存目录，便于排查产物
    packager_dir = output_path.parent / (output_path.stem + "_packager")
    _stage_packager_dir(packager_dir, images, manifest)

    # tar + xz 全部委托给 arcs-sdk 的 package_ota：
    #   - create_tar_from_packager_dir 保证 config.json 在第一条目
    #   - create_txz 的 LZMA2 dict/nice_len 必须和 boot 侧 xzdec 对齐
    with tempfile.TemporaryDirectory() as tmp_dir:
        tar_path = Path(tmp_dir) / "ota.tar"
        package_ota.create_tar_from_packager_dir(packager_dir, tar_path)
        package_ota.create_txz(tar_path, output_path)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate flash image packages (LPK / Intel HEX / OTA txz) from a partition table."
    )
    parser.add_argument(
        "output",
        type=Path,
        help="Output package path (.lpk / .hex / .ota.txz)",
    )
    parser.add_argument(
        "--format",
        choices=["lpk", "hex", "ota"],
        required=True,
        help="Package format to produce",
    )
    parser.add_argument(
        "--partition-table",
        type=Path,
        required=True,
        help="Partition table JSON file; file paths are relative to the JSON by default.",
    )
    parser.add_argument(
        "--var",
        action="append",
        default=[],
        metavar="KEY=VALUE",
        help="Variable mapping for ${KEY}; VALUE is relative to cwd by default.",
    )
    parser.add_argument(
        "--ota-version",
        default=None,
        metavar="MAJOR.MINOR.BUILD",
        help="OTA 版本名；用于生成给后台粘贴的 info 文件（version_number 由此派生）",
    )
    return parser.parse_args()


def parse_version_triplet(s: str) -> Optional[tuple]:
    """解析 '2.3.0' 形式的三段版本号；失败返回 None。"""
    parts = s.strip().split(".")
    if len(parts) != 3:
        return None
    try:
        major, minor, build = (int(p) for p in parts)
    except ValueError:
        return None
    if any(x < 0 or x > 999 for x in (major, minor, build)):
        return None
    return (major, minor, build)


def version_triplet_to_number(triplet: tuple) -> int:
    """和 apps/arcs-mini/version.h.in 的 PROJECT_VERSION_NUMBER 保持一致。"""
    major, minor, build = triplet
    return major * 1_000_000 + minor * 1_000 + build


def write_ota_info(output_path: Path, ota_version: Optional[str]) -> None:
    """在 ota.txz 旁落一份纯文本说明，字段就是后台上传 OTA 时要填的那三项。

    输出文件名：把 .txz 替换成 -info.txt，例如
      build/arcs-mini-v2.3.0.ota.txz -> build/arcs-mini-v2.3.0.ota-info.txt
    """
    md5_hex = compute_md5(output_path)

    if ota_version:
        triplet = parse_version_triplet(ota_version)
        if triplet is None:
            raise ValueError(f"--ota-version '{ota_version}' 格式非法，期望 MAJOR.MINOR.BUILD")
        version_name = ota_version
        version_number = str(version_triplet_to_number(triplet))
    else:
        version_name = "<unspecified>"
        version_number = "<unspecified>"

    info_path = output_path.with_name(output_path.name[: -len(".txz")] + "-info.txt")
    info_path.write_text(
        f"版本名: {version_name}\n"
        f"版本号: {version_number}\n"
        f"MD5值校验: {md5_hex}\n",
        encoding="utf-8",
    )
    print(f"wrote ota info to {info_path}")


def main() -> None:
    args = parse_args()
    vars_map = parse_vars(args.var)
    table = load_partition_table(args.partition_table)
    images = collect_images(table, args.partition_table, vars_map, tag=args.format)

    if args.format == "lpk":
        package_lpk(images, args.output)
        print(f"wrote lpk to {args.output}")
    elif args.format == "hex":
        write_ihex(images, args.output)
        print(f"wrote hex to {args.output}")
    else:  # ota
        build_ota_package(table, images, args.output)
        print(f"wrote ota to {args.output}")
        write_ota_info(args.output, args.ota_version)


if __name__ == "__main__":
    main()
