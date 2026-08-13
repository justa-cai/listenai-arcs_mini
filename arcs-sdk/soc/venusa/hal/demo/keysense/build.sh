#!/bin/bash

# Build script for VENUSA test cases using CMake + Ninja

set -e

TGT="keysense"
CHIP="venusa"

# Configuration variables
IC_BOARD=${IC_BOARD:-1}

BUILD_TYPE=${BUILD_TYPE:-Debug}

# Toolchain path
NUCLEI_TOOLCHAIN_PATH=${NUCLEI_TOOLCHAIN_PATH:-"/opt/nuclei"}

# Build directory
BUILD_DIR="build"


# Check for clean command
if [[ "$1" == "clean" ]]; then
    echo "=========================================="
    echo "Cleaning all generated files..."
    echo "=========================================="
    
    cmake --build build --target clean_all
    # Remove all build directories
    rm -rf $BUILD_DIR
    
    echo "Clean completed successfully!"
    exit 0
fi

echo "=========================================="
echo "Building $TGT with CMake + Ninja"
echo "=========================================="
echo "CHIP: $CHIP"
echo "IC_BOARD: $IC_BOARD"
echo "BUILD_DIR: $BUILD_DIR"
echo "BUILD_TYPE: $BUILD_TYPE"
echo "TOOLCHAIN: $NUCLEI_TOOLCHAIN_PATH"
echo "=========================================="

# Create build directory
mkdir -p $BUILD_DIR
cd $BUILD_DIR

# Configure with CMake
cmake -G Ninja \
    -DCHIP=$CHIP \
    -DTGT=$TGT \
    -DIC_BOARD=$IC_BOARD \
    -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
    -DNUCLEI_TOOLCHAIN_PATH=$NUCLEI_TOOLCHAIN_PATH \
    -S ../ \
    -B .

# Build
cmake --build .
