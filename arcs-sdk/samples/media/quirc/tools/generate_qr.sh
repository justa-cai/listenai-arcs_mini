#!/bin/bash
# QR Code Generator and Decoder Test Script

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUTPUT_DIR="$SCRIPT_DIR/output"

echo "QR Code Generator and Test Suite"
echo "=================================="
echo ""

# Check if Python 3 is available
if ! command -v python3 &> /dev/null; then
    echo "Error: Python 3 is required but not found"
    exit 1
fi

# Check if required packages are installed
echo "Checking Python dependencies..."
python3 -c "import qrcode, PIL, numpy" 2>/dev/null
if [ $? -ne 0 ]; then
    echo "Installing required Python packages..."
    pip3 install -r "$SCRIPT_DIR/requirements.txt"
    if [ $? -ne 0 ]; then
        echo "Error: Failed to install dependencies"
        echo "Please run: pip3 install -r tools/requirements.txt"
        exit 1
    fi
fi

# Generate QR codes
echo ""
echo "Generating QR codes..."
python3 "$SCRIPT_DIR/qr_generator.py" -o "$OUTPUT_DIR" -a

# Copy header file to src directory
if [ -f "$OUTPUT_DIR/qr_test_data.h" ]; then
    cp "$OUTPUT_DIR/qr_test_data.h" "$SCRIPT_DIR/../src/"
    echo ""
    echo "✓ Header file copied to src/qr_test_data.h"
fi

echo ""
echo "Done! You can now build and run the sample to test QR code decoding."
echo ""
echo "Generated files:"
echo "  - PNG images: $OUTPUT_DIR/qr_*.png"
echo "  - Header files: $OUTPUT_DIR/*.h"
echo "  - Test data: src/qr_test_data.h"
