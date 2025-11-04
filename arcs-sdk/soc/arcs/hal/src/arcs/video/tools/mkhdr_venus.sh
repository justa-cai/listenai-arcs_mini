#!/bin/sh
# $1: input file


echo "make header for venus"
img_hdr_pos=192
img_size_pos=196   # 192 + 4
hdr_sum_pos=252    # 192 + 60


# output image size
img_size=$(stat -c "%s" $1)
printf \""%x:%02x%02x%02x%02x"\" $img_size_pos $((img_size&0xff)) \
 $((img_size>>8 &0xff)) $((img_size>>16 &0xff)) $((img_size>>24)) \
 | xxd -r - $1
 
# output header check sum and vector check sum
sumv=0
sumh=0
i=0
for val in $(od -An -v -j 0 -N $hdr_sum_pos -t u1 $1)
do
	if ((i < $img_hdr_pos))
	then
		((sumv+=val))
	else
		((sumh+=val))
	fi
	((i++))
done
((sumv+=sumh + (sumh&0xff) + (sumh>>8)))

printf \""%x:%02x%02x%02x%02x"\" $hdr_sum_pos $((sumh&0xff)) \
 $((sumh>>8)) $((sumv&0xff)) $((sumv>>8)) | xxd -r - $1
