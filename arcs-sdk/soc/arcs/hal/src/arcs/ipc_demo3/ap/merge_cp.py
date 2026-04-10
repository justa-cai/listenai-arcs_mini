import sys
import os

def merge_files(cp_load_path, fhost_path, output_path):
    # read cp_load.bin and fhost.bin
    with open(cp_load_path, 'rb') as cp_load_file:
        cp_load_data = cp_load_file.read()
    with open(fhost_path, 'rb') as fhost_file:
        fhost_data = fhost_file.read()

    # calculate padding bytes
    cp_load_size = len(cp_load_data)
    padding_size = output_size - cp_load_size

    # padding cp_load.bin
    padding_data = b'\0' * padding_size
    merged_data = cp_load_data + padding_data + fhost_data

    # write merged file
    with open(output_path, 'wb') as output_file:
        output_file.write(merged_data)

if __name__ == "__main__":
    if len(sys.argv) != 5:
        print("Usage: python merge_files.py <cp_load_path> <fhost_path> <output_size> <output_file>")
        sys.exit(1)

    cp_load_path = sys.argv[1]
    fhost_path = sys.argv[2]
    output_size = int(sys.argv[3],16)
    output_path = sys.argv[4]

    merge_files(cp_load_path, fhost_path, output_path)
    print("Files merged successfully!")
