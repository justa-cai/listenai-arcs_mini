#!/bin/sh

# mkhdr.sh - patch ListenAI boot header (image size + checksums) into a firmware
# image, in-place. Pure POSIX sh, no external toolchain dependency.

usage() {
    cat <<EOF
Usage:
  $0 <image>      Patch image in-place: write image size + header/vector checksums.

Supported product strings (matched at chip-specific offsets):
  VENUS VEGA VEGAH ARCS MARS APUS VENUSA SPICA NEBULAA
EOF
}

case "$1" in
    -h|--help)
        usage
        exit 0
        ;;
    "")
        usage >&2
        exit 1
        ;;
esac

# Input file passed as the first argument
input_file=$1

# Keep byte-oriented tools deterministic when scanning arbitrary firmware bytes.
export LC_ALL=C

# Define product strings and corresponding offsets
product_strings="VENUS VEGA VEGAH ARCS MARS APUS VENUSA SPICA NEBULAA"
offsets="213 212 229 340 180 196 294 229 295"
img_hdr_pos_s="192 192 208 320 160 176 272 208 272"
img_size_pos_s="196 196 212 324 164 180 276 212 276"
hdr_sum_pos_s="252 252 252 380 220 236 332 252 332"

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
    product_value=$(head -c $offset "$input_file" | tail -c ${#product} | tr -d '\000')

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
    exit 1
fi

# output image size
img_size=$(wc -c < "$input_file") || {
    echo "error: failed to get image size: $input_file" >&2
    exit 1
}
printf \""%x:%02x%02x%02x%02x"\" $img_size_pos $((img_size&0xff)) \
 $((img_size>>8 &0xff)) $((img_size>>16 &0xff)) $((img_size>>24)) \
 | xxd -r - "$input_file"

## output header check sum and vector check sum
sumv=0
sumh=0
i=0
#for val in $(od -An -v -j 0 -N "$hdr_sum_pos" -t u1 "$1" | tr -s ' ' '\n'); do
#    # Process each value
#    echo "val is $val"
#done
for val in $(od -An -v -j 0 -N $hdr_sum_pos -t u1 "$input_file"); do
    if [ "$i" -lt "$img_hdr_pos" ]; then
        sumv=$((sumv + val))
    else
        sumh=$((sumh + val))
    fi
    i=$((i + 1))
done
sumv=$((sumv + sumh + (sumh & 0xff) + (sumh >> 8)))

printf \""%x:%02x%02x%02x%02x"\" $hdr_sum_pos $((sumh&0xff)) \
 $((sumh>>8)) $((sumv&0xff)) $((sumv>>8)) | xxd -r - "$input_file"
