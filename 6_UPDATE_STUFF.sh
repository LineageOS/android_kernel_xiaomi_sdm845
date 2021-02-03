#!/bin/bash
MLX="$(pwd)"
AK=$MLX/AnyKernel3
AIK=$MLX/AIK
#CLANG=~/TOOLCHAIN/clang
sudo apt update
sudo apt -f install -y aptitude
sudo aptitude -f install -y  libomp-13-dev llvm-13 llvm-13 clang-13 lld-13 gcc-11 binutils make flex bison bc build-essential libncurses-dev libssl-dev libelf-dev qt5-default libclang-common-13-dev gcc-11 gcc-11-arm-linux-gnueabi gcc-11-aarch64-linux-gnu binutils-aarch64-linux-gnu 
sudo apt -f --fix-missing install -y
sudo aptitude -f upgrade -y --with-new-pkgs
cd $MLX && git pull
cd $AK && git pull
cd $AIK && git pull
#cd $CLANG && git pull
cd $MLX
