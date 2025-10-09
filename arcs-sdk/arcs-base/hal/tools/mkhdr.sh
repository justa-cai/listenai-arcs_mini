#!/bin/sh

# Input file passed as the first argument
input_file=$1

# Define product strings and corresponding offsets
product_strings="VENUS VEGA VEGAH ARCS MARS APUS VENUSA"
offsets="213 212 229 340 180 180 294"
img_hdr_pos_s="192 192 208 320 160 160 272"
img_size_pos_s="196 196 212 324 164 164 276"
hdr_sum_pos_s="252 252 252 380 220 220 332"

# Initialize a counter
i=0

# Loop through the product strings and offsets
for product in $product_strings; do
    # Get the corresponding offset for the current product string
    offset=$(echo $offsets | cut -d' ' -f$((i + 1)))
    img_hdr_pos=$(echo $img_hdr_pos_s | cut -d' ' -f$((i + 1)))
    img_size_pos=$(echo $img_size_pos_s | cut -d' ' -f$((i + 1)))
    hdr_sum_pos=$(echo $hdr_sum_pos_s | cut -d' ' -f$((i + 1)))

    # Extract the product string from the input file starting at the given offset
    product_value=$(head -c $offset "$input_file" | tail -c ${#product})

    # Debug: Output the extracted value to verify
    #echo "Extracted value: '$product_value', hdr_pos $img_hdr_pos, img_size $img_size_pos img_sum is $hdr_sum_pos"

    # Check if the extracted string matches the expected product string
    if [ "$product_value" = "$product" ]; then
        #echo "Found product: $product at offset $offset"
        product_string=$product_value
        break
    fi

    # Increment the counter
    i=$((i + 1))
done

# Display the product string found
echo "Product string stored in variable: $product_string hdr_pos $img_hdr_pos, img_size $img_size_pos img_sum is $hdr_sum_pos"
if [ -z "$product_string" ]; then
    echo "error: unsupported binary files"
    exit
fi

# output image size
img_size=$(stat -c "%s" $1)
printf \""%x:%02x%02x%02x%02x"\" $img_size_pos $((img_size&0xff)) \
 $((img_size>>8 &0xff)) $((img_size>>16 &0xff)) $((img_size>>24)) \
 | xxd -r - $1

## output header check sum and vector check sum
sumv=0
sumh=0
i=0
#for val in $(od -An -v -j 0 -N "$hdr_sum_pos" -t u1 "$1" | tr -s ' ' '\n'); do
#    # Process each value
#    echo "val is $val"
#done
for val in $(od -An -v -j 0 -N $hdr_sum_pos -t u1 $1); do
    if [ "$i" -lt "$img_hdr_pos" ]; then
        sumv=$((sumv + val))
    else
        sumh=$((sumh + val))
    fi
    i=$((i + 1))
done
sumv=$((sumv + sumh + (sumh & 0xff) + (sumh >> 8)))

printf \""%x:%02x%02x%02x%02x"\" $hdr_sum_pos $((sumh&0xff)) \
 $((sumh>>8)) $((sumv&0xff)) $((sumv>>8)) | xxd -r - $1
