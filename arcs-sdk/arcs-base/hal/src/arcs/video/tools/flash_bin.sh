#!/bin/bash

TARGET_BIN=media.bin
SRC_PATH=../../../../../out/venus/

sync
md5sum $SRC_PATH$TARGET_BIN
cp $SRC_PATH$TARGET_BIN ./
sync

bash mkhdr_venus.sh $TARGET_BIN
sync
md5sum $TARGET_BIN

