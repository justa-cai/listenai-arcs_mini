#!/usr/bin/env bash

DB_FAT_BIN_SIZE=90
# CURRENT_PATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
BLOB_PATH=$SCRIPT_DIR/image
DB_SRC_DEC_FILE=$SCRIPT_DIR/test.db
DB_SRC_CSK_ENC_FILE=$SCRIPT_DIR/test_csk_enc.db
DB_SRC_WX_ENC_FILE=$SCRIPT_DIR/test_wx_enc.db
DB_MOUNT_POINT=$BLOB_PATH/fatfs_db_mp

echo $SCRIPT_DIR

DB_FAT_BIN_PATH=$BLOB_PATH/fatfs_db.bin

mkdir -p $BLOB_PATH $DB_MOUNT_POINT
echo $DB_FAT_BIN_PATH


dd if=/dev/zero of=$DB_FAT_BIN_PATH bs=1M count=$DB_FAT_BIN_SIZE status=progress
mkfs.vfat -F 16 -s 16 $DB_FAT_BIN_PATH


sudo mount -t auto $DB_FAT_BIN_PATH $DB_MOUNT_POINT


echo 'copying ...'
sudo cp -r $DB_SRC_DEC_FILE $DB_MOUNT_POINT
sudo cp -r $DB_SRC_CSK_ENC_FILE $DB_MOUNT_POINT
sudo cp -r $DB_SRC_WX_ENC_FILE $DB_MOUNT_POINT
echo 'copying done'

sudo umount $DB_MOUNT_POINT


