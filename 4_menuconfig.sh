#!/bin/bash
rm -rf out
green='\033[01;32m'
restore='\033[0m'
echo -e "${green}"
make kernelversion
echo -e "${restore}"
###
DEFCONFIG=malakas_beryllium_defconfig
###
export ARCH=arm64 && export SUBARCH=arm64 $DEFCONFIG
make $DEFCONFIG
make menuconfig
cp .config arch/arm64/configs/malakas_beryllium_defconfig
clear
echo -e "${green}"
echo done
echo -e "${restore}"
