#!/bin/bash
MLX="$(pwd)"
AK=$MLX/AnyKernel3
AIK=$MLX/AIK
#CLANG=~/TOOLCHAIN/clang
sudo apt update
sudo apt -f install -y aptitude
#sudo aptitude -f install -y  libomp-14-dev llvm-14 llvm-14 clang-14 lld-14 gcc-11 binutils make flex bison bc build-essential libncurses-dev libssl-dev libelf-dev libclang-common-14-dev gcc-11 gcc-11-arm-linux-gnueabi gcc-11-aarch64-linux-gnu binutils-aarch64-linux-gnu 
sudo apt -f --fix-missing install -y
sudo aptitude -f upgrade -y --with-new-pkgs
cd $MLX && git pull
cd $AK && git pull
cd $AIK && git pull
#cd $CLANG && git pull
cd $MLX
