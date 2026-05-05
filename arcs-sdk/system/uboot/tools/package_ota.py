#!/usr/bin/env python3

import argparse
import io
import json
import lzma
import shutil
import tarfile
import tempfile
from hashlib import md5
from pathlib import Path
from typing import Iterable, List, Optional, Tuple, Union


ImageEntry = Tuple[str, Path]


def _parse_image_arg(image_arg: str) -> ImageEntry:
    if "=" not in image_arg:
        raise ValueError(f"invalid --image value: {image_arg}")
    name, path = image_arg.split("=", 1)
    if not name or not path:
        raise ValueError(f"invalid --image value: {image_arg}")
    return name, Path(path)


def _normalize_images(images: Union[Iterable[ImageEntry], str, Path]) -> List[ImageEntry]:
    if isinstance(images, (str, Path)):
        return [("AP", Path(images))]

    normalized = []
    for name, image_path in images:
        normalized.append((str(name), Path(image_path)))
    return normalized


def _add_tar_bytes(tar: tarfile.TarFile, arcname: str, payload: bytes) -> None:
    info = tarfile.TarInfo(arcname)
    info.size = len(payload)
    info.mode = 0o644
    tar.addfile(info, io.BytesIO(payload))


def create_tar(images, output_tar: Path) -> None:
    with tarfile.open(output_tar, "w", format=tarfile.USTAR_FORMAT) as tar:
        for name, image_path in _normalize_images(images):
            image_bytes = image_path.read_bytes()
            _add_tar_bytes(tar, f"{name}.bin", image_bytes)


def create_tar_from_packager_dir(packager_dir: Path, output_tar: Path) -> None:
    config_path = packager_dir / "config.json"
    image_dir = packager_dir / "image"

    if not config_path.is_file():
        raise FileNotFoundError(f"missing config.json in {packager_dir}")
    if not image_dir.is_dir():
        raise FileNotFoundError(f"missing image directory in {packager_dir}")

    with tarfile.open(output_tar, "w", format=tarfile.USTAR_FORMAT) as tar:
        config_bytes = config_path.read_bytes()
        _add_tar_bytes(tar, "config.json", config_bytes)

        for image_path in sorted(path for path in image_dir.iterdir() if path.is_file()):
            image_bytes = image_path.read_bytes()
            _add_tar_bytes(tar, f"image/{image_path.name}", image_bytes)


def _resolve_config_path(raw_path: str, config_path: Path, base_dir: Optional[Path]) -> Path:
    path = Path(raw_path)
    candidates = []

    if path.is_absolute():
        return path
    if base_dir is not None:
        candidates.append(base_dir / path)
    candidates.append(config_path.parent / path)
    candidates.append(Path.cwd() / path)

    for candidate in candidates:
        if candidate.exists():
            return candidate

    return candidates[0]


def _load_single_app_config(config_path: Path) -> dict:
    payload = json.loads(config_path.read_text(encoding="utf-8"))
    apps = payload.get("apps")

    if apps is None:
        app_config = payload
    else:
        if not isinstance(apps, list) or len(apps) != 1:
            raise ValueError("config must contain exactly one app")
        app_config = apps[0]

    if not isinstance(app_config, dict):
        raise ValueError("app config must be an object")
    if not isinstance(app_config.get("app_name"), str) or not app_config["app_name"]:
        raise ValueError("app_name is required")
    if not isinstance(app_config.get("images"), list) or not app_config["images"]:
        raise ValueError("images is required")

    return app_config


def _manifest_icon_name(app_config: dict) -> str:
    raw_path = app_config.get("icon_path")
    if not isinstance(raw_path, str) or not raw_path:
        return "default.png"
    return Path(raw_path).name or "default.png"


def _manifest_icon_type(icon_name: str) -> str:
    suffix = Path(icon_name).suffix.lower()
    return suffix[1:] if suffix.startswith(".") else "png"


def _build_manifest_image(file_name: str, flash_addr: str, image_bytes: bytes) -> dict:
    return {
        "file": file_name,
        "fmt": "raw",
        "md5": md5(image_bytes).hexdigest(),
        "check": {
            "type": "none",
            "value": "0",
        },
        "copy_to": {
            "type": "nor",
            "addr": str(flash_addr),
            "size": str(len(image_bytes)),
        },
    }


def create_tar_from_config(config_path: Path, output_tar: Path, base_dir: Optional[Path] = None) -> None:
    app_config = _load_single_app_config(config_path)
    icon_name = _manifest_icon_name(app_config)
    image_members = []
    manifest_images = []

    for image_config in app_config["images"]:
        if not isinstance(image_config, dict):
            raise ValueError("image entry must be an object")
        raw_path = image_config.get("bin_path")
        flash_addr = image_config.get("flash_addr")
        if not isinstance(raw_path, str) or not raw_path:
            raise ValueError("image.bin_path is required")
        if not isinstance(flash_addr, str) or not flash_addr:
            raise ValueError("image.flash_addr is required")

        image_path = _resolve_config_path(raw_path, config_path, base_dir)
        image_bytes = image_path.read_bytes()
        image_name = image_path.name
        image_members.append((image_name, image_bytes))
        manifest_images.append(_build_manifest_image(image_name, flash_addr, image_bytes))

    manifest = {
        "version": "2.0",
        "app_name": app_config["app_name"],
        "icon": {
            "file": icon_name,
            "type": _manifest_icon_type(icon_name),
        },
        "image": manifest_images,
    }
    manifest_bytes = json.dumps(manifest, indent=4, ensure_ascii=False).encode("utf-8")

    with tarfile.open(output_tar, "w", format=tarfile.USTAR_FORMAT) as tar:
        _add_tar_bytes(tar, "config.json", manifest_bytes)

        for image_name, image_bytes in image_members:
            _add_tar_bytes(tar, f"image/{image_name}", image_bytes)


def create_txz(input_tar: Path, output_txz: Path) -> None:
    filters = [{
        "id": lzma.FILTER_LZMA2,
        "dict_size": 128 * 1024,
        "nice_len": 273,
    }]

    with input_tar.open("rb") as src, lzma.open(
        output_txz,
        "wb",
        format=lzma.FORMAT_XZ,
        check=lzma.CHECK_CRC32,
        filters=filters,
    ) as dst:
        shutil.copyfileobj(src, dst)


def _create_selected_tar(
    args: argparse.Namespace,
    images: List[ImageEntry],
    output_tar: Path,
) -> None:
    if args.config is not None:
        base_dir = Path(args.base_dir).resolve() if args.base_dir is not None else None
        create_tar_from_config(Path(args.config).resolve(), output_tar, base_dir)
    elif args.packager_dir is not None:
        create_tar_from_packager_dir(Path(args.packager_dir), output_tar)
    else:
        create_tar(images, output_tar)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--config")
    parser.add_argument("--base-dir")
    parser.add_argument("--input")
    parser.add_argument("--image", action="append", default=[])
    parser.add_argument("--packager-dir")
    parser.add_argument("--tar")
    parser.add_argument("--txz")
    args = parser.parse_args()

    images = [_parse_image_arg(image_arg) for image_arg in args.image]
    if args.input is not None:
        images.insert(0, ("AP", Path(args.input)))
    input_mode_count = sum([
        args.config is not None,
        args.packager_dir is not None,
        bool(images),
    ])
    if input_mode_count != 1:
        parser.error("exactly one of --config, --packager-dir or --input/--image is required")
    if args.tar is not None and args.txz is not None:
        parser.error("--tar and --txz are mutually exclusive")

    if args.tar is not None:
        output_tar = Path(args.tar)
        output_tar.parent.mkdir(parents=True, exist_ok=True)
        _create_selected_tar(args, images, output_tar)
        return

    output_txz = Path(args.txz) if args.txz is not None else Path.cwd() / "ota.txz"
    output_txz.parent.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory() as temp_dir:
        temp_tar = Path(temp_dir) / "ota.tar"
        _create_selected_tar(args, images, temp_tar)
        create_txz(temp_tar, output_txz)


if __name__ == "__main__":
    main()
