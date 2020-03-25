#!/bin/bash

KERNEL_DIR=$(pwd)
ANYKERNEL_DIR=$KERNEL_DIR/anykernel
DATE=$(date +"%d%m%Y")
KERNEL_NAME="clueless"
DEVICE="-equuleus-"
VER="-v0.1"
TYPE="-AOSP"
FINAL_ZIP="$KERNEL_NAME""$DEVICE""$DATE""$TYPE""$VER".zip

rm -rf $ANYKERNEL_DIR/Image.gz-dtb
rm -rf $KERNEL_DIR/arch/arm64/boot/Image.gz $KERNEL_DIR/arch/arm64/boot/Image.gz-dtb
rm -rf $FINAL_ZIP
rm -rf out/

make clean && make mrproper
make O=out ARCH=arm64 equuleus_defconfig


PATH="/home/clueless/android/toolchain/linux-x86/clang-r377782c/bin:/home/clueless/android/toolchain/aarch64-linux-android-4.9/bin:${PATH}" \
make -j$(nproc --all) O=out \
                      ARCH=arm64 \
                      CC=clang \
                      CLANG_TRIPLE=aarch64-linux-gnu- \
                      CROSS_COMPILE=aarch64-linux-android- 
                      
{
cp $KERNEL_DIR/arch/arm64/boot/Image.gz-dtb $ANYKERNEL_DIR;
}



cd $ANYKERNEL_DIR
zip -r9 $FINAL_ZIP * -x *.zip $FINAL_ZIP

cd
cd $KERNEL_DIR


