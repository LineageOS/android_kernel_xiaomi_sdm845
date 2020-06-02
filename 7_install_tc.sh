#!/bin/bash
### FETCH LATEST COMPILERS
### built with llvm/clang by default
###########################################################

###### SET BASH COLORS
yellow="\033[1;93m"
magenta="\033[05;1;95m"
restore="\033[0m"

###### SET VARIABLES
source="$(pwd)"
tc=~/TOOLCHAIN/clang

###### MANUALLY INSTALL LLVM/CLANG-11 POLLY SUPPORT FOR NOW
### do this prior to clang-11 installation so that if support will officially come
### it will be overridden by the official latest clang libraries
echo -e "${yellow}"
echo "Adding support for clang-11 polly and ldgold..."
echo ""
polly=/usr/lib/llvm-11/lib
sudo mkdir -p $polly
sudo \cp -rf $source/LLVMPolly.so $polly/
sudo \cp -rf $source/LLVMPolly.so /usr/lib/
sudo \cp -rf $source/LLVMgold.so /usr/lib/
echo "done!"
echo -e "${restore}"

###### ADD SOURCES
echo -e "${yellow}"
echo "Adding llvm-11 repository"
echo "and upgrading system compilers"
echo -e "${restore}"

### daily llvm git builds - not always support for lto
### DO NOT change distro on the llvm repos. these branches are only meant for the toolchain
### and will ensure you will always use latest llvm when using "make CC=clang"
llvm_1='deb http://apt.llvm.org/focal/ llvm-toolchain-focal main'
llvm_2='deb-src http://apt.llvm.org/focal/ llvm-toolchain-focal main'
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

### UPGRADE COMPILERS
sudo apt update
sudo apt -f install -y aptitude
sudo aptitude -f install -y llvm-11
sudo aptitude -f install -y llvm
sudo aptitude -f install -y clang-11 lld-11 libclang-common-11-dev
sudo aptitude -f install -y gcc clang binutils make flex bison bc build-essential libncurses-dev libssl-dev libelf-dev qt5-default
sudo aptitude -f install -y gcc-aarch64-linux-gnu gcc-arm-linux-gnueabi gcc-10-aarc64-linux-gnu gcc-10-arm-linux-gnueabi
cd $tc && git pull && cd $source
sudo apt -f install -y && apt -f --fix-broken install -y apt -f upgrade -y

### COMPLETION
echo -e "${magenta}"
echo "llvm-11 installed and the rest of the toolchain updated!"
echo -e "${restore}"
sleep 2
clear

### reopen menu
cd $source
./0*

###### END
