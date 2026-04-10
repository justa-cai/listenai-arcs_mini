#!/usr/bin/env python3
"""
QR Code Generator and Converter
Generates QR codes with different specifications and converts them to C header files
"""

import qrcode
import argparse
import os
from PIL import Image
import numpy as np
from datetime import datetime

# QR Code configurations with different sizes and error correction levels
QR_CONFIGS = [
    {
        "name": "small_low",
        "version": 1,  # 21x21
        "error_correction": qrcode.constants.ERROR_CORRECT_L,
        "data": "Hello World",
        "box_size": 10,
        "border": 4,
    },
    {
        "name": "small_medium",
        "version": 1,  # 21x21
        "error_correction": qrcode.constants.ERROR_CORRECT_M,
        "data": "QUIRC",
        "box_size": 10,
        "border": 4,
    },
    {
        "name": "medium_high",
        "version": 3,  # 29x29
        "error_correction": qrcode.constants.ERROR_CORRECT_H,
        "data": "https://github.com/dlbeer/quirc",
        "box_size": 10,
        "border": 4,
    },
    {
        "name": "large_quartile",
        "version": 5,  # 37x37
        "error_correction": qrcode.constants.ERROR_CORRECT_Q,
        "data": "QR Code Test Data 12345",
        "box_size": 10,
        "border": 4,
    },
    {
        "name": "numeric_only",
        "version": 1,
        "error_correction": qrcode.constants.ERROR_CORRECT_L,
        "data": "1234567890",
        "box_size": 10,
        "border": 4,
    },
    {
        "name": "alphanumeric",
        "version": 2,  # 25x25
        "error_correction": qrcode.constants.ERROR_CORRECT_M,
        "data": "ABC-123",
        "box_size": 10,
        "border": 4,
    },
    {
        "name": "chinese_text",
        "version": 4,  # 33x33
        "error_correction": qrcode.constants.ERROR_CORRECT_M,
        "data": "聆思科技",
        "box_size": 10,
        "border": 4,
    },
]


def generate_qr_code(config, output_dir="output"):
    """Generate a QR code based on configuration"""
    qr = qrcode.QRCode(
        version=config["version"],
        error_correction=config["error_correction"],
        box_size=config["box_size"],
        border=config["border"],
    )
    
    qr.add_data(config["data"])
    qr.make(fit=True)
    
    # Create image
    img = qr.make_image(fill_color="black", back_color="white")
    
    # Save as PNG
    os.makedirs(output_dir, exist_ok=True)
    png_path = os.path.join(output_dir, f"qr_{config['name']}.png")
    img.save(png_path)
    
    print(f"Generated: {png_path}")
    return img, config


def image_to_grayscale_array(img):
    """Convert PIL Image to grayscale numpy array"""
    # Convert to numpy array
    img_array = np.array(img.convert('L'))
    return img_array


def generate_c_header_single(config, img, output_dir="output"):
    """Generate C header file for a single QR code"""
    gray_array = image_to_grayscale_array(img)
    height, width = gray_array.shape
    
    header_filename = os.path.join(output_dir, f"qr_{config['name']}.h")
    
    ec_level_map = {
        qrcode.constants.ERROR_CORRECT_L: "L (Low)",
        qrcode.constants.ERROR_CORRECT_M: "M (Medium)",
        qrcode.constants.ERROR_CORRECT_Q: "Q (Quartile)",
        qrcode.constants.ERROR_CORRECT_H: "H (High)",
    }
    
    with open(header_filename, 'w') as f:
        f.write(f"/* Auto-generated QR code data: {config['name']} */\n")
        f.write(f"/* Generated on: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')} */\n")
        f.write(f"/* QR Code Version: {config['version']} */\n")
        f.write(f"/* Error Correction: {ec_level_map.get(config['error_correction'], 'Unknown')} */\n")
        f.write(f"/* Data: \"{config['data']}\" */\n")
        f.write(f"/* Image Size: {width}x{height} */\n\n")
        
        f.write(f"#ifndef QR_{config['name'].upper()}_H\n")
        f.write(f"#define QR_{config['name'].upper()}_H\n\n")
        f.write(f"#include <stdint.h>\n\n")
        
        # Write dimensions
        f.write(f"#define QR_{config['name'].upper()}_WIDTH  {width}\n")
        f.write(f"#define QR_{config['name'].upper()}_HEIGHT {height}\n")
        f.write(f"#define QR_{config['name'].upper()}_SIZE   {width * height}\n\n")
        
        # Write expected data
        f.write(f"#define QR_{config['name'].upper()}_EXPECTED_DATA \"{config['data']}\"\n\n")
        
        # Write image data
        f.write(f"const uint8_t qr_{config['name']}_data[{height * width}] = {{\n")
        
        for y in range(height):
            f.write("    ")
            for x in range(width):
                value = gray_array[y, x]
                f.write(f"0x{value:02X}")
                if y < height - 1 or x < width - 1:
                    f.write(", ")
                if (x + 1) % 16 == 0 and x < width - 1:
                    f.write("\n    ")
            f.write("\n")
        
        f.write("};\n\n")
        f.write(f"#endif /* QR_{config['name'].upper()}_H */\n")
    
    print(f"Generated header: {header_filename}")
    return header_filename


def generate_c_header_all(configs_data, output_dir="output"):
    """Generate a combined C header file with all QR codes"""
    header_filename = os.path.join(output_dir, "qr_test_data.h")
    
    with open(header_filename, 'w') as f:
        f.write("/* Auto-generated QR code test data */\n")
        f.write(f"/* Generated on: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')} */\n")
        f.write(f"/* Total QR codes: {len(configs_data)} */\n\n")
        
        f.write("#ifndef QR_TEST_DATA_H\n")
        f.write("#define QR_TEST_DATA_H\n\n")
        f.write("#include <stdint.h>\n\n")
        
        # Write count
        f.write(f"#define QR_TEST_COUNT {len(configs_data)}\n\n")
        
        # Write each QR code data
        for config, img in configs_data:
            gray_array = image_to_grayscale_array(img)
            height, width = gray_array.shape
            
            ec_level_map = {
                qrcode.constants.ERROR_CORRECT_L: "L",
                qrcode.constants.ERROR_CORRECT_M: "M",
                qrcode.constants.ERROR_CORRECT_Q: "Q",
                qrcode.constants.ERROR_CORRECT_H: "H",
            }
            
            f.write(f"/* QR Code: {config['name']} */\n")
            f.write(f"/* Version: {config['version']}, EC: {ec_level_map.get(config['error_correction'], '?')} */\n")
            f.write(f"/* Data: \"{config['data']}\" */\n")
            f.write(f"#define QR_{config['name'].upper()}_WIDTH  {width}\n")
            f.write(f"#define QR_{config['name'].upper()}_HEIGHT {height}\n")
            f.write(f"static const uint8_t qr_{config['name']}_data[{height * width}] = {{\n")
            
            for y in range(height):
                f.write("    ")
                for x in range(width):
                    value = gray_array[y, x]
                    f.write(f"0x{value:02X}")
                    if y < height - 1 or x < width - 1:
                        f.write(", ")
                    if (x + 1) % 16 == 0 and x < width - 1:
                        f.write("\n    ")
                f.write("\n")
            
            f.write("};\n\n")
        
        # Write test data structure
        f.write("/* Test data structure */\n")
        f.write("typedef struct {\n")
        f.write("    const char *name;\n")
        f.write("    const uint8_t *data;\n")
        f.write("    int width;\n")
        f.write("    int height;\n")
        f.write("    const char *expected_data;\n")
        f.write("} qr_test_data_t;\n\n")
        
        # Write test data array
        f.write("static const qr_test_data_t qr_test_cases[QR_TEST_COUNT] = {\n")
        for config, img in configs_data:
            gray_array = image_to_grayscale_array(img)
            height, width = gray_array.shape
            f.write(f"    {{\"{config['name']}\", qr_{config['name']}_data, ")
            f.write(f"{width}, {height}, \"{config['data']}\"}},\n")
        f.write("};\n\n")
        
        f.write("#endif /* QR_TEST_DATA_H */\n")
    
    print(f"\nGenerated combined header: {header_filename}")
    return header_filename


def main():
    parser = argparse.ArgumentParser(
        description="Generate QR codes and convert to C header files"
    )
    parser.add_argument(
        "-o", "--output",
        default="output",
        help="Output directory (default: output)"
    )
    parser.add_argument(
        "-c", "--combined",
        action="store_true",
        help="Generate combined header file for all QR codes"
    )
    parser.add_argument(
        "-s", "--separate",
        action="store_true",
        help="Generate separate header files for each QR code"
    )
    parser.add_argument(
        "-a", "--all",
        action="store_true",
        help="Generate both combined and separate header files (default)"
    )
    
    args = parser.parse_args()
    
    # Default to generating all if no specific option is chosen
    if not (args.combined or args.separate):
        args.all = True
    
    output_dir = args.output
    os.makedirs(output_dir, exist_ok=True)
    
    print(f"Generating {len(QR_CONFIGS)} QR codes...\n")
    
    # Generate all QR codes
    configs_data = []
    for config in QR_CONFIGS:
        img, cfg = generate_qr_code(config, output_dir)
        configs_data.append((cfg, img))
        
        if args.separate or args.all:
            generate_c_header_single(cfg, img, output_dir)
    
    # Generate combined header
    if args.combined or args.all:
        generate_c_header_all(configs_data, output_dir)
    
    print(f"\n✓ All files generated in '{output_dir}' directory")
    print(f"\nTo use in your C code:")
    print(f"  1. Copy the generated .h file(s) to your project")
    print(f"  2. Include in main.c: #include \"qr_test_data.h\"")
    print(f"  3. Use the qr_test_cases array to test decoding")


if __name__ == "__main__":
    main()
