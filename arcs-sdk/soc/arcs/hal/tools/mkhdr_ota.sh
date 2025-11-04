#!/bin/sh
#
# the script builds binary files with ota header
#
# parameters:
# $1: file name

img_size_pos=0x20

hdr_sum_pos=0x38

hdr_crc_pos=0x3c

# output image size
img_size=$(stat -c "%s" $1)
printf \""%x:%02x%02x%02x%02x"\" $img_size_pos $((img_size&0xff)) \
 $((img_size>>8 &0xff)) $((img_size>>16 &0xff)) $((img_size>>24)) \
 | xxd -r - $1

# output checksum
checksum=0
pos=1024
while [ $pos -lt $img_size ]; do
    val=$(od -An -v -j $pos -N 4 -t u4 $1)
    ((checksum^=val))
    ((pos+=1024))
    if [ $pos -gt 32768 ]; then break; fi
done
printf \""%x:%02x%02x%02x%02x"\" $hdr_sum_pos $((checksum&0xff)) \
 $(((checksum>>8)&0xff)) $(((checksum>>16)&0xff)) $(((checksum>>24)&0xff)) | xxd -r - $1

img_crc=$(gzip -1 -c $1 | tail -c 8 | od -An -t u4 -N4 -) 
# output image crc
printf \""%x:%02x%02x%02x%02x"\" $hdr_crc_pos $((img_crc&0xff)) \
 $(((img_crc>>8)&0xff)) $(((img_crc>>16)&0xff)) $(((img_crc>>24)&0xff)) | xxd -r - $1
 

