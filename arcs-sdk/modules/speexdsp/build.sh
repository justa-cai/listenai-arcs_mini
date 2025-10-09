#!/bin/bash

if [ $# -lt 1 ]; then
    echo "Script Param ERROR!!!"
    echo "Example:"
    echo "      ./build.sh CMAKE_Config_File"
    echo "      ./build.sh ../../R328_linux_setup.cmake"
    exit
fi

if [ -d build ]; then
    rm -rf ./build

    if [ "$1" == "clean" ]; then
        exit
    fi
fi

mkdir build
cp $1 ./build/platform_setup.cmake

cd build
cmake cmake -DCMAKE_TOOLCHAIN_FILE=./platform_setup.cmake ../
make

cd -