#!/bin/bash

KERNEL_DIR=$(pwd)
ANYKERNEL_DIR=$KERNEL_DIR/anykernel/equuleus
DATE=$(date +"%d%m%Y")
KERNEL_NAME="clueless"
DEVICE="-equuleus-"
VER="v01"
TYPE="-AOSP"
FINAL_ZIP="$KERNEL_NAME""$DEVICE""$DATE""$TYPE""$VER".zip

rm -rf $ANYKERNEL_DIR//equuleus/Image.gz-dtb
rm -rf $KERNEL_DIR/out/arch/arm64/boot/Image.gz $KERNEL_DIR/out/arch/arm64/boot/Image.gz-dtb
rm -rf $FINAL_ZIP

mkdir -p out
make O=out clean
make O=out mrproper
make O=out ARCH=arm64 equuleus_defconfig


PATH="~/android/toolchain/linux-x86/clang-r377782c/bin:~/android/toolchain/aarch64-linux-android-4.9/bin:~/android/toolchain/arm-linux-androideabi-4.9/bin:${PATH}" \
make -j$(nproc --all) O=out \
                      ARCH=arm64 \
                      CC=clang \
                      CXX=clang++ \
                      CLANG_TRIPLE=aarch64-linux-gnu- \
                      CROSS_COMPILE=aarch64-linux-android- \
                      CROSS_COMPILE_ARM32=arm-linux-androideabi- \
                      CCHACHE=1 \
                      CCACHE_COMPRESS=1



{
cp $KERNEL_DIR/out/arch/arm64/boot/Image.gz-dtb $ANYKERNEL_DIR/equuleus;
}



cd $ANYKERNEL_DIR/equuleus
zip -r9 $FINAL_ZIP * -x *.zip $FINAL_ZIP

cd
cd $KERNEL_DIR

