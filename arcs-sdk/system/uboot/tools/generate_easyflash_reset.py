#!/usr/bin/env python3

import argparse
from pathlib import Path


def write_reset_image(output: Path, size: int) -> None:
    if size <= 0:
        raise ValueError("size must be positive")

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(b"\x00" * size)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True)
    parser.add_argument("--size", required=True)
    args = parser.parse_args()

    write_reset_image(Path(args.output), int(args.size, 0))


if __name__ == "__main__":
    main()
