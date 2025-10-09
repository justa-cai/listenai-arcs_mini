if [ -d build ]; then
    rm -rf ./build
fi

if [ -d output ]; then
    rm -rf ./output
fi

mkdir build

cd build

cmake cmake -DCMAKE_INSTALL_PREFIX=./output ../
make
make install

cd -