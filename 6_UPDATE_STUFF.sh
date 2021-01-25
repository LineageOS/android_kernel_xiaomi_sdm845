#!/bin/bash
MLX="$(pwd)"
AK=$MLX/AnyKernel3
AIK=$MLX/AIK
#CLANG=~/TOOLCHAIN/clang
sudo apt update
sudo apt -f install -y aptitude
sudo aptitude -f install -y  libomp-12-dev llvm-12 llvm llvm-12 clang-12 lld-12 gcc clang binutils make flex bison bc build-essential libncurses-dev libssl-dev libelf-dev qt5-default libclang-common-12-dev gcc-11 gcc-11-arm-linux-gnueabi gcc-11-aarch64-linux-gnu
sudo apt -f --fix-missing install -y
sudo aptitude -f upgrade -y --with-new-pkgs
cd $MLX && git pull
cd $AK && git pull
cd $AIK && git pull
#cd $CLANG && git pull
cd $MLX
