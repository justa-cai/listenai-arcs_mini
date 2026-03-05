#!/usr/bin/env python3

import os
import sys
import struct
import hashlib
import json
from pathlib import Path
try:
    from PIL import Image
    HAS_PIL = True
except ImportError:
    HAS_PIL = False

def sanitize_var_name(path_str):
    var_name = path_str.replace('/', '_').replace('\\', '_').replace('.', '_').replace('-', '_')
    var_name = ''.join(c if c.isalnum() or c == '_' else '_' for c in var_name)
    if var_name[0].isdigit():
        var_name = '_' + var_name
    return var_name

def get_file_hash(file_path):
    hash_md5 = hashlib.md5()
    with open(file_path, 'rb') as f:
        for chunk in iter(lambda: f.read(4096), b""):
            hash_md5.update(chunk)
    return hash_md5.hexdigest()

def load_cache(cache_file):
    if cache_file.exists():
        try:
            with open(cache_file, 'r') as f:
                return json.load(f)
        except:
            return {}
    return {}

def save_cache(cache_file, cache_data):
    with open(cache_file, 'w') as f:
        json.dump(cache_data, f, indent=2)

def get_png_dimensions(data):
    if len(data) < 24:
        return 0, 0
    if data[0:8] != b'\x89PNG\r\n\x1a\n':
        return 0, 0
    width = struct.unpack('>I', data[16:20])[0]
    height = struct.unpack('>I', data[20:24])[0]
    return width, height

def get_jpeg_dimensions(data):
    if len(data) < 2:
        return 0, 0
    if data[0:2] != b'\xff\xd8':
        return 0, 0

    i = 2
    while i < len(data) - 1:
        if data[i] != 0xff:
            return 0, 0
        marker = data[i + 1]
        i += 2

        if marker == 0xd8 or marker == 0xd9 or marker == 0x01:
            continue
        if marker >= 0xd0 and marker <= 0xd7:
            continue

        if i + 2 > len(data):
            return 0, 0
        length = struct.unpack('>H', data[i:i+2])[0]

        if marker >= 0xc0 and marker <= 0xcf and marker not in [0xc4, 0xc8, 0xcc]:
            if i + 5 <= len(data):
                height = struct.unpack('>H', data[i+3:i+5])[0]
                width = struct.unpack('>H', data[i+5:i+7])[0]
                return width, height

        i += length

    return 0, 0

def get_image_info(file_path, data):
    ext = file_path.suffix.lower()

    if ext == '.png':
        width, height = get_png_dimensions(data)
        return 'LV_IMG_CF_TRUE_COLOR_ALPHA', width, height
    elif ext in ['.jpg', '.jpeg']:
        width, height = get_jpeg_dimensions(data)
        return 'LV_IMG_CF_TRUE_COLOR_ALPHA', width, height

    return 'LV_IMG_CF_RAW', 0, 0

def file_to_c_file(file_path, var_name):
    with open(file_path, 'rb') as f:
        data = f.read()

    color_format, width, height = get_image_info(file_path, data)

    c_content = '#include "lvgl.h"\n\n'

    c_content += f'static const unsigned char {var_name}_data[] = {{\n'

    for i in range(0, len(data), 16):
        chunk = data[i:i+16]
        hex_values = ', '.join(f'0x{b:02x}' for b in chunk)
        c_content += f'    {hex_values},\n'

    c_content += '};\n\n'

    c_content += f'const lv_img_dsc_t {var_name} = {{\n'
    c_content += '    .header = {\n'
    c_content += f'        .cf = {color_format},\n'
    c_content += '        .always_zero = 0,\n'
    c_content += '        .reserved = 0,\n'
    c_content += f'        .w = {width},\n'
    c_content += f'        .h = {height},\n'
    c_content += '    },\n'
    c_content += f'    .data_size = {len(data)},\n'
    c_content += f'    .data = {var_name}_data,\n'
    c_content += '};\n'

    return c_content

def generate_assets(root_dir, output_dir):
    root_path = Path(root_dir)
    output_path = Path(output_dir)

    if not root_path.exists():
        print(f"Error: Directory {root_dir} does not exist")
        return

    output_path.mkdir(parents=True, exist_ok=True)

    cache_file = output_path / '.assets_cache.json'
    cache_data = load_cache(cache_file)

    file_count = 0
    updated_count = 0
    skipped_count = 0
    var_names = []
    current_files = {}

    for file_path in sorted(root_path.rglob('*')):
        if file_path.is_file() and file_path.suffix not in ['.py', '.pyc']:
            rel_path = file_path.relative_to(root_path)
            rel_path_str = str(rel_path)
            var_name = sanitize_var_name(rel_path_str)

            output_c_file = output_path / (var_name + '.c')

            file_hash = get_file_hash(file_path)
            current_files[rel_path_str] = file_hash

            cached_hash = cache_data.get(rel_path_str)

            if cached_hash == file_hash and output_c_file.exists():
                # print(f"Skipping (unchanged): {rel_path}")
                skipped_count += 1
            else:
                # print(f"Processing: {rel_path} -> {output_c_file.name}")

                c_content = file_to_c_file(file_path, var_name)

                with open(output_c_file, 'w') as f:
                    f.write(c_content)

                updated_count += 1

            var_names.append(var_name)
            file_count += 1

    for old_file in cache_data.keys():
        if old_file not in current_files:
            var_name = sanitize_var_name(old_file)
            old_c_file = output_path / (var_name + '.c')
            if old_c_file.exists():
                old_c_file.unlink()
                print(f"Removed: {old_c_file.name}")

    header_file = output_path / 'lisa_ui_assets.h'
    h_content = '#ifndef __LISA_UI_ASSETS_H__\n'
    h_content += '#define __LISA_UI_ASSETS_H__\n\n'
    h_content += '#include "lvgl.h"\n\n'

    for var_name in var_names:
        h_content += f'LV_IMG_DECLARE({var_name});\n'

    h_content += '\n#endif\n'

    header_needs_update = True
    if header_file.exists():
        with open(header_file, 'r') as f:
            old_content = f.read()
        header_needs_update = (old_content != h_content)

    if header_needs_update:
        with open(header_file, 'w') as f:
            f.write(h_content)
        print(f"Updated header: {header_file.name}")
    else:
        print(f"Header unchanged: {header_file.name}")

    save_cache(cache_file, current_files)

    print(f"\nTotal: {file_count} files, Updated: {updated_count}, Skipped: {skipped_count}")
    print(f"Output directory: {output_dir}")

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python assets.py <source_directory> [output_directory]")
        print("Example: python assets.py emoji ./generated")
        sys.exit(1)

    root_dir = sys.argv[1]
    output_dir = sys.argv[2] if len(sys.argv) > 2 else './generated'

    generate_assets(root_dir, output_dir)
