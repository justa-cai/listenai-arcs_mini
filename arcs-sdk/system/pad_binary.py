#!/usr/bin/env python3

import argparse
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--size", required=True)
    parser.add_argument("--fill-byte", default="0xFF")
    args = parser.parse_args()

    input_path = Path(args.input)
    output_path = Path(args.output)
    output_size = int(args.size, 0)
    fill_byte = int(args.fill_byte, 0)

    if fill_byte < 0 or fill_byte > 0xFF:
        raise ValueError("fill byte must be in range 0x00..0xFF")

    data = input_path.read_bytes()
    if len(data) > output_size:
        raise ValueError(
            f"input binary is {len(data)} bytes, exceeds padded size {output_size}"
        )

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_bytes(data + bytes([fill_byte]) * (output_size - len(data)))


if __name__ == "__main__":
    main()
