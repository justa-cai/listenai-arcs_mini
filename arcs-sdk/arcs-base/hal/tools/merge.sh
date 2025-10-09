#!/bin/sh
#
# the script merge binary files with ota header
#
# parameters:
# $1: output file name
# $2: flash boot file, zone table in this file
# $3: input file2
# $4: input file3
# $n+1:  input file_n


tgt_file=$1
rm -f $tgt_file
shift

# find zone table
flash_boot=$1
table=$(od -An -t u2 -N2 -j 52 $flash_boot)
((table=table+16))
table=$(od -An -t u2 -N2 -j $table $flash_boot)
((table=table+8))


until [ $# -eq 0 ]
do

zone_size=$(od -An -t u4 -N4 -j $table $flash_boot)
((table=table+12))

cp $1 bin.tmp

# output image size
img_size=$(stat -c "%s" $1)

filling_size=$(($zone_size - $img_size))
dd if=/dev/zero bs=1 count="$filling_size" 2>/dev/null | tr '\0' '\377' > padding.tmp

# write valid flag
echo "0:88112244" | xxd -r - bin.tmp

# merge file
cat bin.tmp padding.tmp >> "$tgt_file"

shift
done

# padding 0xff to overwrite ota zone header
dd if=/dev/zero bs=1 count="4096" 2>/dev/null | tr '\0' '\377' >> "$tgt_file"

# Remove the temporary padding file
rm -rf padding.tmp bin.tmp

echo -e "Use $tgt_file for flash download"


