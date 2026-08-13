#!/usr/bin/env python3
"""Merge VenusA face-detect AP, FD resources and CP demo firmware."""

from pathlib import Path
import sys


SCRIPT_DIR = Path(__file__).resolve().parent
SDK_DIR = SCRIPT_DIR.parents[2]
OUTPUT_FILE = SCRIPT_DIR / "merged_firmware.bin"
FILL_BYTE = 0xFF

FIRMWARE_MAP = [
    (0x000000, SCRIPT_DIR / "res/ap.bin"),
    (0x100000, SCRIPT_DIR / "res/algo/fdetect_venusA.bin"),
    (0x170000, SCRIPT_DIR / "res/algo/falign_venusA104_concat.bin"),
    (0x2E0000, SCRIPT_DIR / "res/algo/fverify_venusA.bin"),
    (0x800000, SDK_DIR / "build/venusa_face_detect.bin"),
]


def display_path(path):
    try:
        return path.relative_to(SCRIPT_DIR)
    except ValueError:
        return path.relative_to(SDK_DIR)


def check_inputs():
    print("Checking input files...")
    missing = False
    for offset, path in FIRMWARE_MAP:
        if not path.exists():
            print(f"  missing 0x{offset:08X}: {path}")
            missing = True
            continue
        print(f"  ok      0x{offset:08X}: {path} ({path.stat().st_size} bytes)")
    return not missing


def check_overlaps():
    ordered = sorted(FIRMWARE_MAP, key=lambda item: item[0])
    previous_end = 0
    previous_path = None

    for offset, path in ordered:
        size = path.stat().st_size
        end = offset + size
        if offset < previous_end:
            print(
                f"Overlap: {path} at 0x{offset:08X} overlaps "
                f"{previous_path} ending at 0x{previous_end:08X}"
            )
            return False
        previous_end = end
        previous_path = path

    return True


def merge_bins():
    if not check_inputs():
        return False
    if not check_overlaps():
        return False

    total_size = max(offset + path.stat().st_size for offset, path in FIRMWARE_MAP)
    firmware = bytearray([FILL_BYTE] * total_size)

    print(f"\nMerging firmware, total size: {total_size} bytes ({total_size / 1024 / 1024:.2f} MiB)")
    for offset, path in FIRMWARE_MAP:
        data = path.read_bytes()
        firmware[offset:offset + len(data)] = data
        print(f"  wrote {len(data):>10} bytes at 0x{offset:08X}: {display_path(path)}")

    OUTPUT_FILE.write_bytes(firmware)
    print(f"\nOutput: {OUTPUT_FILE} ({OUTPUT_FILE.stat().st_size} bytes)")

    print("\nFirmware layout:")
    print("=" * 78)
    print(f"{'start':<12} {'end':<12} {'size':>12}  file")
    print("-" * 78)
    ordered = sorted(FIRMWARE_MAP, key=lambda item: item[0])
    for index, (offset, path) in enumerate(ordered):
        size = path.stat().st_size
        end = offset + size
        print(f"0x{offset:08X}  0x{end:08X}  {size:>10} B  {display_path(path)}")

        if index < len(ordered) - 1:
            next_offset = ordered[index + 1][0]
            gap = next_offset - end
            if gap > 0:
                print(f"0x{end:08X}  0x{next_offset:08X}  {gap:>10} B  fill 0x{FILL_BYTE:02X}")
    print("=" * 78)

    print("\nBurn command:")
    print("  ./tools/burn/cskburn.darwin-x64 -C venus -s <tty> -b 3000000 "
          f"0x0 {OUTPUT_FILE.relative_to(SDK_DIR)}")
    return True


def main():
    return 0 if merge_bins() else 1


if __name__ == "__main__":
    sys.exit(main())
