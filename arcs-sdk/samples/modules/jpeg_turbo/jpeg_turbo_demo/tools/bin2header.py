#!/usr/bin/env python3
"""
Convert binary file (e.g., JPEG image) to C header file with byte array
Usage: python bin2header.py input.jpg output.h [array_name]
"""

import sys
import os


def bin_to_header(input_file, output_file, array_name=None):
    """
    Convert binary file to C header file with byte array
    
    Args:
        input_file: Path to input binary file
        output_file: Path to output .h file
        array_name: Optional custom array name (default: derived from filename)
    """
    # Get file size
    file_size = os.path.getsize(input_file)
    
    # Derive array name from filename if not provided
    if array_name is None:
        base_name = os.path.splitext(os.path.basename(input_file))[0]
        # Replace invalid C identifier characters
        array_name = base_name.replace('-', '_').replace('.', '_').replace(' ', '_')
    
    # Generate header guard
    header_guard = f"{array_name.upper()}_H"
    
    # Read binary data
    with open(input_file, 'rb') as f:
        data = f.read()
    
    # Write header file
    with open(output_file, 'w') as f:
        # Write header
        f.write(f"/**\n")
        f.write(f" * @file {os.path.basename(output_file)}\n")
        f.write(f" * @brief Binary data from {os.path.basename(input_file)}\n")
        f.write(f" * @note Auto-generated file, do not edit manually\n")
        f.write(f" */\n\n")
        
        f.write(f"#ifndef {header_guard}\n")
        f.write(f"#define {header_guard}\n\n")
        
        f.write(f"#include <stdint.h>\n\n")
        
        # Write array declaration
        f.write(f"/* Size: {file_size} bytes */\n")
        f.write(f"static const unsigned char {array_name}[] = {{\n")
        
        # Write data in rows of 12 bytes
        bytes_per_line = 12
        for i in range(0, len(data), bytes_per_line):
            chunk = data[i:i + bytes_per_line]
            hex_values = ', '.join(f'0x{b:02X}' for b in chunk)
            
            # Add comma if not last line
            if i + bytes_per_line < len(data):
                f.write(f"    {hex_values},\n")
            else:
                f.write(f"    {hex_values}\n")
        
        f.write(f"}};\n\n")
        
        # Write size constant
        f.write(f"static const unsigned int {array_name}_len = {file_size};\n\n")
        
        f.write(f"#endif /* {header_guard} */\n")
    
    print(f"✓ Converted {input_file} -> {output_file}")
    print(f"  Array name: {array_name}")
    print(f"  Size: {file_size} bytes")


def main():
    if len(sys.argv) < 3:
        print("Usage: python bin2header.py <input_file> <output_file> [array_name]")
        print("")
        print("Example:")
        print("  python bin2header.py picture.jpg picture.h")
        print("  python bin2header.py picture.jpg picture.h my_image_data")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_file = sys.argv[2]
    array_name = sys.argv[3] if len(sys.argv) > 3 else None
    
    # Check input file exists
    if not os.path.exists(input_file):
        print(f"Error: Input file '{input_file}' not found")
        sys.exit(1)
    
    # Convert
    bin_to_header(input_file, output_file, array_name)


if __name__ == '__main__':
    main()
