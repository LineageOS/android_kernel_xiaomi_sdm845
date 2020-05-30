#!/bin/bash
MLX="$(pwd)"
AK=$MLX/AnyKernel3
AIK=$MLX/AIK
TC=~/TOOLCHAIN
cd $MLX
git pull
rm -rf $AK
git clone https://github.com/thanasxda/AnyKernel3.git
rm -rf $AIK
git clone https://github.com/thanasxda/AIK.git
rm -rf $TC/clang $TC/twisted-clang
mkdir -p ~/TOOLCHAIN
cd $TC
git clone https://github.com/TwistedPrime/twisted-clang.git
mv twisted-clang clang
cd $MLX
### update stuff
./6_UPDATE*
