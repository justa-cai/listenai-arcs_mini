#!/usr/bin/env python3
import sys
import os
import argparse

def parse_size(size_str):
    try:
        return int(size_str, 0)
    except ValueError:
        return int(size_str)

def merge_bins(output_file, bins):
    # bins is a list of (address, file_path) tuples
    # Sort by address
    bins.sort(key=lambda x: x[0])
    
    # Calculate total size
    max_addr = 0
    bin_data_list = []
    
    for addr, path in bins:
        if not os.path.exists(path):
            print(f"Error: File not found: {path}")
            sys.exit(1)
            
        with open(path, 'rb') as f:
            data = f.read()
            
        end_addr = addr + len(data)
        if end_addr > max_addr:
            max_addr = end_addr
            
        bin_data_list.append({'addr': addr, 'data': data, 'path': path})
        
    print(f"Total merged size: {max_addr} bytes")
    
    # Create buffer filled with 0xFF
    merged_data = bytearray([0xFF] * max_addr)
    
    # Fill buffer
    for item in bin_data_list:
        addr = item['addr']
        data = item['data']
        size = len(data)
        
        # Check for overlap
        # Since we sorted by address, we only need to check if current overlaps with previous filled area?
        # Actually, since we write to a bytearray, later writes will overwrite earlier ones.
        # But we should probably warn or error on overlap.
        # Let's do a simple check against the 'merged_data' if it's not 0xFF? 
        # No, 0xFF is valid data. 
        # Let's check ranges.
        
        print(f"Merging {item['path']} at 0x{addr:X} (size: {size} bytes)")
        merged_data[addr:addr+size] = data

    # Write to output file
    output_dir = os.path.dirname(os.path.abspath(output_file))
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)
        
    with open(output_file, 'wb') as f:
        f.write(merged_data)
    
    print(f"Successfully created {output_file}")

def main():
    parser = argparse.ArgumentParser(description='Merge multiple binary files into one with padding (0xFF).')
    parser.add_argument('-o', '--output', required=True, help='Output merged binary file path')
    parser.add_argument('--bin', action='append', nargs=2, metavar=('ADDRESS', 'FILE'), 
                        help='Input binary file and its destination address (hex or decimal). Can be used multiple times.')
    
    args = parser.parse_args()
    
    if not args.bin:
        print("Error: No input binaries specified. Use --bin <address> <file>")
        sys.exit(1)
        
    parsed_bins = []
    for addr_str, file_path in args.bin:
        addr = parse_size(addr_str)
        parsed_bins.append((addr, file_path))
        
    merge_bins(args.output, parsed_bins)

if __name__ == '__main__':
    main()
