#!/usr/bin/env python3

import argparse
import hashlib
import io
import json
import lzma
import tarfile
from pathlib import Path


def build_payload(path: Path, size: int, *, multiplier: int, bias: int) -> bytes:
    data = bytes((i * multiplier + bias) & 0xFF for i in range(size))
    path.write_bytes(data)
    return data


def add_tar_file(tar: tarfile.TarFile, arcname: str, data: bytes) -> None:
    info = tarfile.TarInfo(name=arcname)
    info.size = len(data)
    info.mode = 0o644
    tar.addfile(info, io.BytesIO(data))


def build_manifest(image_entries: list[dict]) -> bytes:
    manifest = {
        "version": "2.0",
        "app_name": "fixture",
        "icon": {
            "file": "default.png",
            "type": "png",
        },
        "image": image_entries,
    }

    return json.dumps(manifest, separators=(",", ":")).encode("utf-8")


def payload_md5_hex(payload: bytes, *, corrupt: bool) -> str:
    digest = hashlib.md5(payload).hexdigest()
    if not corrupt:
        return digest

    first = "0" if digest[0] != "0" else "1"
    return first + digest[1:]


def build_tar(payload_entries: list[tuple[str, bytes]],
              tar_path: Path,
              *,
              manifest_bytes: bytes | None,
              manifest_name: str,
              extra_file_name: str | None,
              extra_file_size: int,
              extra_file_before_image: bool) -> bytes:
    extra_bytes = bytes((i * 11 + 5) & 0xFF for i in range(extra_file_size))

    with tarfile.open(tar_path, "w", format=tarfile.USTAR_FORMAT) as tar:
        if manifest_bytes is not None:
            add_tar_file(tar, manifest_name, manifest_bytes)

        if extra_file_name and extra_file_before_image:
            add_tar_file(tar, extra_file_name, extra_bytes)

        for arcname, payload in payload_entries:
            add_tar_file(tar, arcname, payload)

        if extra_file_name and not extra_file_before_image:
            add_tar_file(tar, extra_file_name, extra_bytes)

    return tar_path.read_bytes()


def build_txz(tar_bytes: bytes, txz_path: Path) -> None:

    filters = [{
        "id": lzma.FILTER_LZMA2,
        "dict_size": 128 * 1024,
        "nice_len": 273,
    }]

    txz_path.write_bytes(
        lzma.compress(
            tar_bytes,
            format=lzma.FORMAT_XZ,
            check=lzma.CHECK_CRC32,
            filters=filters,
        )
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--payload", required=True)
    parser.add_argument("--tar", required=True)
    parser.add_argument("--txz")
    parser.add_argument("--size", required=True, type=int)
    parser.add_argument("--image-name", default="AP.bin")
    parser.add_argument("--target-addr")
    parser.add_argument("--declared-size", type=int)
    parser.add_argument("--second-payload")
    parser.add_argument("--second-size", type=int, default=0)
    parser.add_argument("--second-image-name", default="CP.bin")
    parser.add_argument("--second-target-addr")
    parser.add_argument("--second-declared-size", type=int)
    parser.add_argument("--manifest", action="store_true")
    parser.add_argument("--manifest-name", default="config.json")
    parser.add_argument("--bad-md5", action="store_true")
    parser.add_argument("--bad-second-md5", action="store_true")
    parser.add_argument("--omit-image", action="store_true")
    parser.add_argument("--extra-file-name")
    parser.add_argument("--extra-file-size", type=int, default=0)
    parser.add_argument("--extra-file-before-image", action="store_true")
    args = parser.parse_args()

    payload_path = Path(args.payload)
    tar_path = Path(args.tar)
    txz_path = Path(args.txz) if args.txz else None

    payload_path.parent.mkdir(parents=True, exist_ok=True)
    tar_path.parent.mkdir(parents=True, exist_ok=True)
    if txz_path is not None:
        txz_path.parent.mkdir(parents=True, exist_ok=True)

    payload = build_payload(payload_path, args.size, multiplier=17, bias=3)
    payload_entries: list[tuple[str, bytes]] = []

    if not args.omit_image:
        payload_entries.append((args.image_name, payload))

    if args.second_payload:
        second_payload_path = Path(args.second_payload)
        second_payload_path.parent.mkdir(parents=True, exist_ok=True)
        second_payload = build_payload(
            second_payload_path,
            args.second_size,
            multiplier=29,
            bias=7,
        )
        payload_entries.append((args.second_image_name, second_payload))

    manifest_bytes = None
    if args.manifest:
        image_entries = []
        if not args.omit_image:
            image_entries.append({
                "file": Path(args.image_name).name,
                "fmt": "raw",
                "md5": payload_md5_hex(payload, corrupt=args.bad_md5),
                "check": {
                    "type": "none",
                    "value": "0",
                },
                "copy_to": {
                    "type": "nor",
                    "addr": args.target_addr,
                    "size": str(args.declared_size or args.size),
                },
            })

        if args.second_payload:
            image_entries.append({
                "file": Path(args.second_image_name).name,
                "fmt": "raw",
                "md5": payload_md5_hex(second_payload, corrupt=args.bad_second_md5),
                "check": {
                    "type": "none",
                    "value": "0",
                },
                "copy_to": {
                    "type": "nor",
                    "addr": args.second_target_addr,
                    "size": str(args.second_declared_size or args.second_size),
                },
            })

        manifest_bytes = build_manifest(image_entries)

    tar_bytes = build_tar(
        payload_entries,
        tar_path,
        manifest_bytes=manifest_bytes,
        manifest_name=args.manifest_name,
        extra_file_name=args.extra_file_name,
        extra_file_size=args.extra_file_size,
        extra_file_before_image=args.extra_file_before_image,
    )
    if txz_path is not None:
        build_txz(tar_bytes, txz_path)


if __name__ == "__main__":
    main()
