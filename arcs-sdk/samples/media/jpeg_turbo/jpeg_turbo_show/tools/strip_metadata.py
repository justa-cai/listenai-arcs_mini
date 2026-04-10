#!/usr/bin/env python3
"""
JPEG Metadata Stripper

This script removes EXIF, XMP, ICC, and other metadata from JPEG files,
creating clean baseline JPEG files compatible with embedded systems.

Usage:
    python3 strip_metadata.py <input.jpg> <output.jpg>

Example:
    python3 strip_metadata.py picture/NASA-Apollo8-Dec24-Earthrise.jpg picture/NASA-clean.jpg
"""

import sys
import os
from PIL import Image


def strip_jpeg_metadata(input_path, output_path, quality=90):
    """
    Strip all metadata from JPEG file and save as clean baseline JPEG.
    
    Args:
        input_path: Path to input JPEG file
        output_path: Path to output JPEG file
        quality: JPEG compression quality (1-100, default: 90)
    
    Returns:
        True if successful, False otherwise
    """
    try:
        # Open image
        with Image.open(input_path) as img:
            # Convert to RGB if needed (removes alpha channel, palette, etc.)
            if img.mode not in ('RGB', 'L'):
                img = img.convert('RGB')
            
            # Get image info
            width, height = img.size
            mode = img.mode
            
            # Save as clean baseline JPEG without any metadata
            # optimize=False: Disable progressive encoding (ensure baseline)
            # quality: Compression quality
            img.save(output_path, 
                    'JPEG',
                    quality=quality,
                    optimize=False,  # Baseline JPEG (not progressive)
                    progressive=False,
                    subsampling='4:2:0',  # Standard chroma subsampling
                    exif=b'',  # Remove EXIF
                    icc_profile=None)  # Remove ICC profile
            
            # Get file sizes
            input_size = os.path.getsize(input_path)
            output_size = os.path.getsize(output_path)
            
            print(f"✓ Successfully stripped metadata")
            print(f"  Input:  {input_path}")
            print(f"  Output: {output_path}")
            print(f"  Image:  {width}x{height} ({mode})")
            print(f"  Size:   {input_size} bytes → {output_size} bytes")
            print(f"  Saved:  {input_size - output_size} bytes ({100 - output_size*100/input_size:.1f}% reduction)")
            
            return True
            
    except FileNotFoundError:
        print(f"✗ Error: Input file not found: {input_path}", file=sys.stderr)
        return False
    except Exception as e:
        print(f"✗ Error: {e}", file=sys.stderr)
        return False


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        print("\nError: Missing arguments", file=sys.stderr)
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_file = sys.argv[2]
    quality = int(sys.argv[3]) if len(sys.argv) > 3 else 90
    
    if not os.path.exists(input_file):
        print(f"✗ Error: Input file does not exist: {input_file}", file=sys.stderr)
        sys.exit(1)
    
    # Ensure output directory exists
    output_dir = os.path.dirname(output_file)
    if output_dir and not os.path.exists(output_dir):
        os.makedirs(output_dir)
    
    success = strip_jpeg_metadata(input_file, output_file, quality)
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
