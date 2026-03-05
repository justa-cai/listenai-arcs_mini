#!/usr/bin/env python3

import os
import json
import shutil
import glob
import sys
from pathlib import Path

def copy_assets(config_path, output_dir):
    """Copy assets based on the configuration"""
    # Create output directory if it doesn't exist
    os.makedirs(output_dir, exist_ok=True)
    
    # Load the configuration
    with open(config_path, 'r') as f:
        config = json.load(f)
    
    # Process each copy operation
    for op in config.get('copy_operations', []):
        source = op['source']
        destination = os.path.join(output_dir, op['destination'])
        
        # Create destination directory if it doesn't exist
        dest_dir = os.path.dirname(destination)
        if dest_dir:
            os.makedirs(dest_dir, exist_ok=True)
        
        print(f"Copying {source} to {destination}")
        
        # Handle wildcard in source
        if '*' in source:
            # For glob patterns, copy each matching file
            for src_file in glob.glob(source):
                if os.path.isfile(src_file):
                    dest = os.path.join(dest_dir, os.path.basename(src_file)) if os.path.isdir(destination) else destination
                    shutil.copy2(src_file, dest)
        else:
            # For single files
            if os.path.isfile(source):
                shutil.copy2(source, destination)
            else:
                print(f"Warning: Source file not found: {source}", file=sys.stderr)

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <config_path> <output_dir>")
        sys.exit(1)
    
    config_path = sys.argv[1]
    output_dir = sys.argv[2]
    
    copy_assets(config_path, output_dir)
