#!/usr/bin/env python3
"""Build and inspect ARCS-MINI series factory parameter sectors."""

import argparse
import binascii
import json
import pathlib
import re
import struct
import sys

try:
    import yaml
except ImportError:
    yaml = None


class ConfigYamlError(Exception):
    """Fallback type used when PyYAML is unavailable."""


YAML_ERROR = yaml.YAMLError if yaml is not None else ConfigYamlError


MAGIC = 0x54434146
HEADER_WORDS = 6
HEADER_SIZE = HEADER_WORDS * 4
SECTOR_SIZE = 4096
HEADER_FORMAT = "<6I"
FACTORY_PARAMS_OFFSET = 0x03E000
FACTORY_BOOT_OFFSET = 0xA00000


def strip_yaml_comment(line):
    quote = None
    escaped = False
    for index, char in enumerate(line):
        if escaped:
            escaped = False
            continue
        if char == "\\" and quote == '"':
            escaped = True
            continue
        if char in ("'", '"'):
            if quote is None:
                quote = char
            elif quote == char:
                quote = None
            continue
        if char == "#" and quote is None:
            return line[:index]
    if quote is not None:
        raise ValueError("unterminated quoted YAML scalar")
    return line


def parse_yaml_scalar(value):
    if value.startswith('"'):
        try:
            parsed = json.loads(value)
        except json.JSONDecodeError as error:
            raise ValueError(f"invalid quoted YAML scalar: {value}") from error
        if not isinstance(parsed, str):
            raise ValueError(f"unsupported YAML scalar: {value}")
        return parsed
    if value.startswith("'"):
        if len(value) < 2 or not value.endswith("'"):
            raise ValueError(f"invalid quoted YAML scalar: {value}")
        return value[1:-1].replace("''", "'")
    if value in ("true", "True"):
        return True
    if value in ("false", "False"):
        return False
    if value in ("null", "Null", "NULL", "~"):
        return None
    if re.fullmatch(r"[-+]?[0-9]+", value):
        return int(value, 10)
    if any(char in value for char in "{}[]&*!>|`"):
        raise ValueError(f"unsupported YAML syntax: {value}")
    return value


def parse_simple_yaml(text):
    """Parse the mapping/scalar subset used by the offline factory config."""
    root = {}
    stack = [(-1, root)]

    for line_number, original in enumerate(text.splitlines(), 1):
        if "\t" in original:
            raise ValueError(f"line {line_number}: tabs are not allowed")
        line = strip_yaml_comment(original).rstrip()
        if not line.strip():
            continue

        indent = len(line) - len(line.lstrip(" "))
        if indent % 2 != 0:
            raise ValueError(f"line {line_number}: indentation must use two spaces")
        content = line.lstrip(" ")
        if ":" not in content:
            raise ValueError(f"line {line_number}: expected key: value")
        key, value = content.split(":", 1)
        key = key.strip()
        value = value.strip()
        if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", key):
            raise ValueError(f"line {line_number}: invalid key: {key}")

        while stack[-1][0] >= indent:
            stack.pop()
        parent_indent, parent = stack[-1]
        if indent > parent_indent + 2:
            raise ValueError(f"line {line_number}: unexpected indentation")
        if key in parent:
            raise ValueError(f"line {line_number}: duplicate key: {key}")

        if value == "":
            child = {}
            parent[key] = child
            stack.append((indent, child))
        else:
            parent[key] = parse_yaml_scalar(value)

    return root


def load_config(path):
    text = path.read_text(encoding="utf-8")
    if yaml is not None:
        return yaml.safe_load(text)
    return parse_simple_yaml(text)


def parse_integer(value, name):
    if isinstance(value, int):
        return value
    if isinstance(value, str):
        try:
            return int(value, 0)
        except ValueError as error:
            raise ValueError(f"{name} is not an integer: {value}") from error
    raise ValueError(f"{name} is not an integer")


def require_mapping(parent, name):
    value = parent.get(name)
    if not isinstance(value, dict):
        raise ValueError(f"missing mapping: {name}")
    return value


def require_value(parent, name):
    value = parent.get(name)
    if value is None or value == "":
        raise ValueError(f"missing value: {name}")
    return value


def validate_config(config):
    if not isinstance(config, dict):
        raise ValueError("YAML root must be a mapping")

    factory = require_mapping(config, "factory")
    require_value(factory, "wifi_ssid")
    if "wifi_pass" not in factory:
        raise ValueError("missing value: wifi_pass")
    require_value(factory, "serial_port")
    require_value(factory, "serial_baud")

    project = require_mapping(config, "project")
    require_value(project, "project_id")
    require_value(project, "token")

    firmware = require_mapping(config, "firmware")
    require_value(firmware, "ch32")
    ls26 = require_mapping(firmware, "ls26")
    require_value(ls26, "main")
    require_value(ls26, "test")
    params_offset = parse_integer(require_value(ls26, "factory_params_offset"),
                                  "factory_params_offset")
    boot_offset = parse_integer(require_value(ls26, "factory_boot_offset"),
                                "factory_boot_offset")
    if params_offset != FACTORY_PARAMS_OFFSET or boot_offset != FACTORY_BOOT_OFFSET:
        raise ValueError(
            "factory_params_offset/factory_boot_offset must be "
            f"0x{FACTORY_PARAMS_OFFSET:X}/0x{FACTORY_BOOT_OFFSET:X}"
        )
    return params_offset, boot_offset


def encode_sector(config):
    _, boot_offset = validate_config(config)
    payload = json.dumps(config, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    if HEADER_SIZE + len(payload) > SECTOR_SIZE:
        raise ValueError(
            f"factory.bin exceeds {SECTOR_SIZE} bytes: {HEADER_SIZE + len(payload)}"
        )

    header = struct.pack(
        HEADER_FORMAT,
        MAGIC,
        HEADER_WORDS,
        0,
        boot_offset,
        HEADER_WORDS,
        len(payload),
    )
    crc = binascii.crc32(header) & 0xFFFFFFFF
    header = struct.pack(
        HEADER_FORMAT,
        MAGIC,
        HEADER_WORDS,
        crc,
        boot_offset,
        HEADER_WORDS,
        len(payload),
    )
    return header + payload + bytes([0xFF]) * (SECTOR_SIZE - HEADER_SIZE - len(payload))


def inspect_sector(path):
    data = path.read_bytes()
    if len(data) != SECTOR_SIZE:
        raise ValueError(f"factory.bin size must be {SECTOR_SIZE}, got {len(data)}")

    magic, header_size, saved_crc, boot_addr, payload_addr, payload_size = struct.unpack_from(
        HEADER_FORMAT, data
    )
    header = bytearray(data[:HEADER_SIZE])
    struct.pack_into("<I", header, 8, 0)
    actual_crc = binascii.crc32(header) & 0xFFFFFFFF
    payload_offset = payload_addr * 4 if header_size == HEADER_WORDS else payload_addr
    if magic != MAGIC:
        raise ValueError("factory.bin magic is invalid")
    if header_size not in (HEADER_WORDS, HEADER_SIZE):
        raise ValueError("factory.bin header_size is invalid")
    if saved_crc != actual_crc:
        raise ValueError("factory.bin header CRC is invalid")
    if payload_offset < HEADER_SIZE or payload_size > SECTOR_SIZE - payload_offset:
        raise ValueError("factory.bin payload range is invalid")

    config = json.loads(data[payload_offset:payload_offset + payload_size].decode("utf-8"))
    params_offset, expected_boot_addr = validate_config(config)
    if boot_addr != expected_boot_addr:
        raise ValueError(
            f"factory.bin boot_addr 0x{boot_addr:X} does not match config 0x{expected_boot_addr:X}"
        )
    return {
        "layout": "arcs_mini_series",
        "magic": "FACT",
        "header_size": header_size,
        "header_crc": f"0x{saved_crc:08X}",
        "boot_addr": f"0x{boot_addr:X}",
        "factory_params_offset": f"0x{params_offset:X}",
        "payload_offset": payload_offset,
        "payload_size": payload_size,
        "sector_size": len(data),
    }


def build_command(args):
    config = load_config(args.input)
    validate_config(config)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(encode_sector(config))
    metadata = inspect_sector(args.output)
    print(json.dumps(metadata, indent=2))


def inspect_command(args):
    print(json.dumps(inspect_sector(args.input), indent=2))


def main():
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)

    build_parser = subparsers.add_parser("build", help="build factory.bin from config.yaml")
    build_parser.add_argument("input", type=pathlib.Path)
    build_parser.add_argument("output", type=pathlib.Path)
    build_parser.set_defaults(handler=build_command)

    inspect_parser = subparsers.add_parser("inspect", help="validate factory.bin")
    inspect_parser.add_argument("input", type=pathlib.Path)
    inspect_parser.set_defaults(handler=inspect_command)

    args = parser.parse_args()
    try:
        args.handler(args)
    except (OSError, ValueError, json.JSONDecodeError, YAML_ERROR) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
