#!/usr/bin/env python3
import argparse
import os
import shlex
import sys
from pathlib import Path

VERSION = "1.0.1"


def _split_merge_list(value):
    if not value:
        return []
    try:
        parts = shlex.split(value, posix=False)
    except ValueError:
        parts = value.split()
    return [p.strip('"') for p in parts if p.strip('"')]


def _ensure_parent(path):
    Path(path).parent.mkdir(parents=True, exist_ok=True)


def main(argv=None):
    argv = list(sys.argv[1:] if argv is None else argv)
    if "--version" in argv:
        print(f"listenai-kconfig-win-wrapper {VERSION}")
        return 0

    parser = argparse.ArgumentParser(description="Windows wrapper for ARCS SDK Kconfig parsing")
    parser.add_argument("-W", action="store_true", dest="warn_error", help="treat warnings as errors")
    parser.add_argument("-k", required=True, dest="kconfig_root")
    parser.add_argument("-c", required=True, dest="dot_config")
    parser.add_argument("-H", required=True, dest="autoconf_h")
    parser.add_argument("-l", required=True, dest="kconfig_list")
    parser.add_argument("-m", nargs="*", default=[], dest="merge_list")
    args = parser.parse_args(argv)

    try:
        import kconfiglib
    except Exception as exc:
        print(f"error: kconfiglib is not installed: {exc}", file=sys.stderr)
        print("hint: run `python -m pip install --user kconfiglib==14.1.0`", file=sys.stderr)
        return 1

    os.environ.setdefault("CONFIG_", "CONFIG_")
    root = os.path.abspath(args.kconfig_root)

    try:
        kconf = kconfiglib.Kconfig(root, warn=True, warn_to_stderr=True)
        first = True
        merge_values = args.merge_list if isinstance(args.merge_list, list) else [args.merge_list]
        for cfg in _split_merge_list(" ".join(merge_values)):
            if not cfg:
                continue
            cfg_path = os.path.abspath(cfg)
            if not os.path.exists(cfg_path):
                continue
            kconf.load_config(cfg_path, replace=first)
            first = False
        if first:
            # No merge file was loaded; keep Kconfig defaults but still emit outputs.
            pass

        _ensure_parent(args.dot_config)
        _ensure_parent(args.autoconf_h)
        _ensure_parent(args.kconfig_list)
        kconf.write_config(args.dot_config, save_old=False)
        kconf.write_autoconf(args.autoconf_h)
        with open(args.kconfig_list, "w", encoding="utf-8", newline="\n") as fp:
            for name in sorted(set(map(os.path.abspath, kconf.kconfig_filenames))):
                fp.write(name.replace("\\", "/") + "\n")

        if args.warn_error and kconf.warnings:
            print("error: Kconfig warnings were treated as errors", file=sys.stderr)
            return 1
        return 0
    except Exception as exc:
        print(f"error: Kconfig parse failed: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())

