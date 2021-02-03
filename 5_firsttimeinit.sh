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
#git clone https://github.com/TwistedPrime/twisted-clang.git
#mv twisted-clang clang
cd $MLX
### update stuff

### daily llvm git builds - not always support for lto
### DO NOT change distro on the llvm repos. these branches are only meant for the toolchain
### and will ensure you will always use latest llvm when using "make CC=clang"
#llvm_1='deb http://apt.llvm.org/focal/ llvm-toolchain-focal main'
#llvm_2='deb-src http://apt.llvm.org/focal/ llvm-toolchain-focal main'
llvm_3='deb http://apt.llvm.org/unstable/ llvm-toolchain main'
llvm_4='deb-src http://apt.llvm.org/unstable/ llvm-toolchain main'
new_sources=("$llvm_1" "$llvm_2" "$llvm_3" "$llvm_4")
for i in ${!new_sources[@]}; do
    if ! grep -q "${new_sources[$i]}" /etc/apt/sources.list; then
        echo "${new_sources[$i]}" | sudo tee -a /etc/apt/sources.list
        echo "Added ${new_sources[$i]} to source list"
        ### fetch keys llvm git
        wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key|sudo apt-key add -
    fi
done

./6_UPDATE*
