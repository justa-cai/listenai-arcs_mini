
COMMIT=67ad0ae68cd0d992025087080c8f46c53796564a
PROJECT_ID=2033

SAVE_DIR=res
BASE_URL="http://listenai-firmware-delivery.oss-cn-beijing.aliyuncs.com/ARCS/firmware"
DOWNLOAD_URL="$BASE_URL/$PROJECT_ID/$COMMIT"

files=(
    emmc_respack.bin
    respak.bin
    scanpen.bin
    res_info.md
    memap.h
)

mkdir -p $SAVE_DIR

for file in "${files[@]}"; do
    wget -O $SAVE_DIR/$file "$DOWNLOAD_URL/$file"
done
