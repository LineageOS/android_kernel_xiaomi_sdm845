#!/bin/bash
### MLX COMPILATION SCRIPT
###setup bot
CHATID=#################################################
BOTAPIKEY=##############################################
###ping channel
 curl -F chat_id="$CHATID" -F text="Build started bitches" -F document=@"$KERNEL/$KERNELNAME" https://api.telegram.org/bot$BOTAPIKEY/sendMessage
###escalate privileges needed to zip recovery
MLX="$(pwd)"
sudo cd $MLX
cd $MLX && git pull
###
DATE_START=$(date +"%s")
yellow="\033[1;93m"
magenta="\033[05;1;95m"
restore="\033[0m"
echo -e "${magenta}"
echo ΜΑΛΆΚΑΣ KERNEL
echo -e "${yellow}"
make kernelversion
echo -e "${restore}"
export USE_CCACHE=1
export USE_PREBUILT_CACHE=1
export PREBUILT_CACHE_DIR=~/.ccache
export CCACHE_DIR=~/.ccache
ccache -M 30G

###leave disabled to show current developer
#export KBUILD_BUILD_USER=thanas
#export KBUILD_BUILD_HOST=MLX

###setup dirs
AK=$MLX/AnyKernel3
AIK=$MLX/AIK
RECOVERYNAME=OrangeFox-v10-1-MLX-67-60hz-Unofficial-beryllium.zip
OUT=$MLX/out/arch/arm64/boot
KERNEL=~/Desktop/MLX
TC=~/TOOLCHAIN
CLANG=$TC/clang/bin
COMPILE=$(cat "$KERNEL/.compile/compile.h")
cd $AK && git pull
cd $AIK && git pull
cd $CLANG/.. && git pull
cd $MLX
###setup kernel stuff
DEFCONFIG=malakas_beryllium_defconfig
HZ=69-60hz
HZ2=67-60hz
DEVICE=beryllium
VERSION=v2
KERNELINFO=${VERSION}_${DEVICE}_${HZ}_$(date +"%Y-%m-%d")
KERNELINFO2=${VERSION}_${DEVICE}_${HZ2}_$(date +"%Y-%m-%d")
KERNELNAME=mlx_kernel_$KERNELINFO.zip
KERNELNAME2=mlx_kernel_$KERNELINFO2.zip
THREADS=-j$(nproc --all)
FLAGS="AR=llvm-ar NM=llvm-nm OBJCOPY=llvm-objcopy OBJDUMP=llvm-objdump STRIP=llvm-strip"
#LD="LD=ld.gold"
CLANG_FLAGS="CC=clang"
#VERBOSE="V=1"
###set HZ
sed -i '68s/.*/					qcom,mdss-dsi-panel-framerate = < 0x45 >;/' $MLX/arch/arm64/boot/dts/qcom/dsi-panel-tianma-fhd-nt36672a-video.dtsi
### update compilers prior to compilation
sudo apt -f upgrade -y clang-11 lld-11
sudo apt -f upgrade -y clang-10 lld-10
sudo apt -f upgrade -y gcc-10
sudo apt -f upgrade -y gcc clang binutils make flex bison bc build-essential libncurses-dev  libssl-dev libelf-dev qt5-default
cd $TC/clang && git pull && cd MLX
###
export ARCH=arm64 && export SUBARCH=arm64 $DEFCONFIG

export CROSS_COMPILE=$CLANG/aarch64-linux-gnu-
export CROSS_COMPILE_ARM32=$CLANG/arm-linux-gnueabi-

#export CLANG_TRIPLE=aarch64-linux-gnu-
export LD_LIBRARY_PATH="$CLANG/../lib:$CLANG/../lib64:$LD_LIBRARY_PATH"
export PATH="$CLANG:$PATH"

###start compilation
mkdir -p out
make O=out ARCH=arm64 $DEFCONFIG
make O=out $THREADS $VERBOSE $CLANG_FLAGS $FLAGS $LD

###zip kernel
if [ -e $OUT/Image.gz-dtb ]; then
echo -e "${yellow}"
echo zipping kernel...
echo -e "${restore}"
mkdir -p $KERNEL/.compile/
mv $OUT/Image.gz-dtb $AK/Image.gz-dtb
cp $MLX/out/include/generated/compile.h $KERNEL/.compile/compile.h
cd $AK
zip -r $KERNELNAME * -x .git .gitignore README.md *placeholder
rm -rf Image.gz-dtb
mv $KERNELNAME $KERNEL

###build completion compile.h
echo -e "${yellow}"
cat $KERNEL/.compile/compile.h
echo "-------------------"
echo "Build Completed in:"
echo "-------------------"
DATE_END=$(date +"%s")
DIFF=$(($DATE_END - $DATE_START))
echo -e "${magenta}"
echo "Time: $(($DIFF / 60)) minute(s) and $(($DIFF % 60)) seconds."
echo -e "${restore}"

###push first kernel on telegram
cd $KERNEL
curl -F chat_id="$CHATID" -F document=@"$KERNEL/$KERNELNAME" https://api.telegram.org/bot$BOTAPIKEY/sendDocument

echo -e "${magenta}"
echo "BUILD 1 SENT TO TELEGRAM NEXT WILL START RN"
echo -e "${restore}"

else
 curl -F chat_id="$CHATID" -F text="Build failed lel..." -F document=@"$KERNEL/$KERNELNAME" https://api.telegram.org/bot$BOTAPIKEY/sendMessage
echo -e "${magenta}"
echo "-------------------"
echo "   Build Failed    "
echo "-------------------"
echo -e "${restore}"
fi;
###build second kernel
cd $MLX
sed -i '68s/.*/					qcom,mdss-dsi-panel-framerate = < 0x43 >;/' $MLX/arch/arm64/boot/dts/qcom/dsi-panel-tianma-fhd-nt36672a-video.dtsi
###
export ARCH=arm64 && export SUBARCH=arm64 $DEFCONFIG

export CROSS_COMPILE=$CLANG/aarch64-linux-gnu-
export CROSS_COMPILE_ARM32=$CLANG/arm-linux-gnueabi-

#export CLANG_TRIPLE=aarch64-linux-gnu-
export LD_LIBRARY_PATH="$CLANG/../lib:$CLANG/../lib64:$LD_LIBRARY_PATH"
export PATH="$CLANG:$PATH"

###start compilation
mkdir -p out
make O=out ARCH=arm64 $DEFCONFIG
make O=out $THREADS $VERBOSE $CLANG_FLAGS $FLAGS $LD

###zip kernel
if [ -e $OUT/Image.gz-dtb ]; then
echo -e "${yellow}"
echo zipping kernel...
echo -e "${restore}"
mkdir -p $KERNEL/.compile/
cp $OUT/Image.gz-dtb $AK/Image.gz-dtb
cp $MLX/out/include/generated/compile.h $KERNEL/.compile/compile.h
cd $AK
zip -r $KERNELNAME2 * -x .git .gitignore README.md *placeholder
rm -rf Image.gz-dtb
mv $KERNELNAME2 $KERNEL

###build completion compile.h
echo -e "${yellow}"
cat $KERNEL/.compile/compile.h
echo "-------------------"
echo "Build Completed in:"
echo "-------------------"
DATE_END=$(date +"%s")
DIFF=$(($DATE_END - $DATE_START))
echo -e "${magenta}"
echo "Time: $(($DIFF / 60)) minute(s) and $(($DIFF % 60)) seconds."
echo -e "${restore}"

###push second kernel on telegram
cd $KERNEL
curl -F chat_id="$CHATID" -F document=@"$KERNEL/$KERNELNAME2" https://api.telegram.org/bot$BOTAPIKEY/sendDocument

echo -e "${magenta}"
echo "BUILD 2 SENT TO TELEGRAM.. STARTING RECOVERY"
echo -e "${restore}"

###insert second kernel in prebuilt recovery
cd $AIK
./unpackimg.sh recovery.img
rm -rf $AIK/split_img/recovery.img-zImage
mv $OUT/Image.gz-dtb $AIK/split_img/recovery.img-zImage
./repackimg.sh
mv image-new.img recovery.img
zip -g $RECOVERYNAME recovery.img
rm -rf $AIK/split_img/recovery.img-zImage
./cleanup.sh
cp $RECOVERYNAME $KERNEL

###push recovery
cd $KERNEL
curl -F chat_id="$CHATID" -F document=@"$KERNEL/$RECOVERYNAME" https://api.telegram.org/bot$BOTAPIKEY/sendDocument
curl -F chat_id="$CHATID" -F text="$COMPILE"  https://api.telegram.org/bot$BOTAPIKEY/sendMessage

echo -e "${magenta}"
echo "RECOVERY PUSHED ON TELEGRAM. ALL DONE K THX"
echo -e "${restore}"
function clean_all {
if [ -e $MLX/out ]; then
cd $MLX
rm -rf out
make O=out clean
make mrproper
fi;
}
while read -p "Clean stuff (y/n)? " cchoice
do
case "$cchoice" in
    y|Y )
        clean_all
        echo
        echo "All Cleaned now."
        break
        ;;
    n|N )
        break
        ;;
    * )
        echo
        echo "Invalid try again!"
        echo
        ;;
esac
done
else
 curl -F chat_id="$CHATID" -F text="Build failed lel..." -F document=@"$KERNEL/$KERNELNAME" https://api.telegram.org/bot$BOTAPIKEY/sendMessage
echo -e "${magenta}"
echo "-------------------"
echo "   Build Failed    "
echo "-------------------"
echo -e "${restore}"
fi;
