#!/usr/bin/env python3

import argparse
import binascii
import struct
from pathlib import Path
from typing import Optional


PARTAB_MAGIC = 0x50415254
PARTAB_VERSION = 0x0001
BOOT_SCHEME_OTA = 1
MAX_PARTITIONS = 16
PART_NAME_SIZE = 16
PART_FLAG_VALID = 1 << 0
PART_FLAG_BOOTABLE = 1 << 1

HEADER_STRUCT = struct.Struct("<IIIIII4BII")
PARTITION_STRUCT = struct.Struct("<16sIIIII8x")
BOOT_CONFIG_SIZE = HEADER_STRUCT.size + PARTITION_STRUCT.size * MAX_PARTITIONS


def build_partition(name: str, base: int, size: int, exec_addr: int, flags: int) -> bytes:
    if not name:
        raise ValueError("partition name must not be empty")
    name_bytes = name.encode("ascii")
    if len(name_bytes) >= PART_NAME_SIZE:
        raise ValueError("partition name is too long")

    return PARTITION_STRUCT.pack(
        name_bytes.ljust(PART_NAME_SIZE, b"\x00"),
        base,
        size,
        exec_addr,
        flags,
        0,
    )


def build_partab_image(*, flash_base: int, boot_flash_size: int, partab_size: int,
                       control_store_base: int, control_store_size: int,
                       app_base: int, app_size: int,
                       cp_base: Optional[int] = None, cp_size: Optional[int] = None,
                       ota_base: int, ota_size: int) -> bytes:
    if partab_size < BOOT_CONFIG_SIZE:
        raise ValueError(
            f"partab size 0x{partab_size:x} is smaller than boot config size 0x{BOOT_CONFIG_SIZE:x}"
        )
    if boot_flash_size < partab_size:
        raise ValueError("boot flash size must be >= partab size")
    if app_base < flash_base + boot_flash_size:
        raise ValueError("app base overlaps boot reserved area")
    if app_size <= 0:
        raise ValueError("app size must be positive")
    if cp_base is not None and cp_size is None:
        raise ValueError("cp size is required when cp base is provided")
    if cp_size is not None and cp_base is None:
        raise ValueError("cp base is required when cp size is provided")
    if cp_size is not None and cp_size <= 0:
        raise ValueError("cp size must be positive")
    if ota_size <= 0:
        raise ValueError("ota size must be positive")
    if control_store_size <= 0:
        raise ValueError("control store size must be positive")

    partition_end = app_base + app_size
    if cp_base is not None:
        if cp_base < partition_end:
            raise ValueError("cp base overlaps app partition")
        partition_end = cp_base + cp_size

    if ota_base < partition_end:
        raise ValueError("ota base overlaps target partitions")
    if ota_base + ota_size > control_store_base:
        raise ValueError("ota partition overlaps control store area")

    partitions = [
        build_partition(
            "AP",
            app_base,
            app_size,
            app_base,
            PART_FLAG_VALID | PART_FLAG_BOOTABLE,
        ),
    ]
    if cp_base is not None:
        partitions.append(
            build_partition(
                "CP",
                cp_base,
                cp_size,
                0xFFFFFFFF,
                PART_FLAG_VALID,
            )
        )
    partitions.append(
        build_partition(
            "OTA_TXZ",
            ota_base,
            ota_size,
            0xFFFFFFFF,
            PART_FLAG_VALID,
        )
    )
    partitions.extend(PARTITION_STRUCT.pack(b"\x00" * PART_NAME_SIZE, 0, 0, 0, 0, 0)
                      for _ in range(MAX_PARTITIONS - len(partitions)))
    partition_blob = b"".join(partitions)
    part_count = 3 if cp_base is not None else 2

    header = HEADER_STRUCT.pack(
        PARTAB_MAGIC,
        PARTAB_VERSION,
        BOOT_CONFIG_SIZE,
        0,
        control_store_base,
        control_store_size,
        BOOT_SCHEME_OTA,
        0,
        0,
        0,
        part_count,
        0,
    )
    config_without_crc = header + partition_blob
    crc32 = binascii.crc32(config_without_crc) & 0xFFFFFFFF
    header = HEADER_STRUCT.pack(
        PARTAB_MAGIC,
        PARTAB_VERSION,
        BOOT_CONFIG_SIZE,
        crc32,
        control_store_base,
        control_store_size,
        BOOT_SCHEME_OTA,
        0,
        0,
        0,
        part_count,
        0,
    )
    config = header + partition_blob
    return config + (b"\xFF" * (partab_size - len(config)))


def write_partab_image(output: Path, **kwargs: int) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(build_partab_image(**kwargs))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True)
    parser.add_argument("--flash-base", required=True)
    parser.add_argument("--boot-flash-size", required=True)
    parser.add_argument("--partab-size", required=True)
    parser.add_argument("--control-store-base", required=True)
    parser.add_argument("--control-store-size", required=True)
    parser.add_argument("--app-base", required=True)
    parser.add_argument("--app-size", required=True)
    parser.add_argument("--cp-base")
    parser.add_argument("--cp-size")
    parser.add_argument("--ota-base", required=True)
    parser.add_argument("--ota-size", required=True)
    args = parser.parse_args()

    write_partab_image(
        Path(args.output),
        flash_base=int(args.flash_base, 0),
        boot_flash_size=int(args.boot_flash_size, 0),
        partab_size=int(args.partab_size, 0),
        control_store_base=int(args.control_store_base, 0),
        control_store_size=int(args.control_store_size, 0),
        app_base=int(args.app_base, 0),
        app_size=int(args.app_size, 0),
        cp_base=int(args.cp_base, 0) if args.cp_base is not None else None,
        cp_size=int(args.cp_size, 0) if args.cp_size is not None else None,
        ota_base=int(args.ota_base, 0),
        ota_size=int(args.ota_size, 0),
    )


if __name__ == "__main__":
    main()
